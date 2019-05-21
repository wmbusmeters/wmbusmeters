#!/bin/bash

if [ "$1" == "" ] || [ "$1" == "-h" ]
then
    echo "Usage: install.sh [binary] [root] [OPTIONS]
    Example: install.sh build/wmbusmeters /

	Options:
	--no-adduser		Do not add wmbusmeters user
	--no-udev-rules		Do not add udev rules
	"
    exit 0
fi

if [ ! "$(basename "$1")" = "wmbusmeters" ]
then
    echo Oups, please only try to install wmbusmeters using this script.
    exit 1
fi

if [ ! -x "$1" ]
then
    echo This is not an executable.
    exit 1
fi

if [ ! -d "$2" ]
then
    echo Oups, please supply a valid root directory.
    exit 1
fi

SRC=$1
ROOT="${2%/}"
ADDUSER=true
ADDUDEVRULES=true

while [ $# -ne 0 ]
do
        ARG="$1"
        shift
        case "$ARG" in
        --no-adduser)
                ADDUSER=false
        ;;
        --no-udev-rules)
                ADDUDEVRULES=false
        shift
        ;;
        esac
done


####################################################################
##
## Intall binaries
##

rm -f "$ROOT"/usr/bin/wmbusmeters "$ROOT"/usr/sbin/wmbusmetersd
mkdir -p "$ROOT"/usr/bin
mkdir -p "$ROOT"/usr/sbin
cp "$SRC" "$ROOT"/usr/bin/wmbusmeters
ln -s "$ROOT"/usr/bin/wmbusmeters "$ROOT"/usr/sbin/wmbusmetersd

echo binaries: installed "$ROOT"/usr/bin/wmbusmeters and "$ROOT"/usr/sbin/wmbusmetersd

####################################################################
##
## Intall wmbusmeters manual page
##

rm -f "$ROOT"/usr/share/man/man1/wmbusmeters.1.gz
mkdir -p "$ROOT"/usr/share/man/man1
gzip -c wmbusmeters.1 > "$ROOT"/usr/share/man/man1/wmbusmeters.1.gz

echo man page: installed "$ROOT"/usr/share/man/man1/wmbusmeters.1.gz

####################################################################
##
## Create wmbusmeters user
##

ID=$(id -u wmbusmeters 2>/dev/null)

if [ -f "$ROOT"/usr/sbin/nologin ]
then
	USERSHELL="$ROOT/usr/sbin/nologin"
elif [ -f "$ROOT"/sbin/nologin ]
then
	USERSHELL="$ROOT/sbin/nologin"
else
	USERSHELL="/bin/false"
fi

if [ "$ADDUSER" = "true" ]
then
    if [ "$ID" = "" ]
    then
        # Create the wmbusmeters user
        useradd --system --shell $USERSHELL --groups dialout wmbusmeters
        echo user: added wmbusmeters
    else
        echo user: wmbusmeters unmodified
    fi
fi

####################################################################
##
## Prepare for  /var/run/wmbusmeters.pid
##

#if [ ! -d "$ROOT"/var/run ]
#then
#    # Create /var/run
#    mkdir -p "$ROOT"/var/run
#    echo pid store: created /var/run
#fi

####################################################################
##
## Prepare for  /var/log/wmbusmeters and /var/log/wmbusmeters/meter_readings
##

if [ ! -d "$ROOT"/var/log/wmbusmeters/meter_readings ]
then
    # Create the log directories
    mkdir -p "$ROOT"/var/log/wmbusmeters/meter_readings
    chown -R wmbusmeters:wmbusmeters "$ROOT"/var/log/wmbusmeters
    echo log: created "$ROOT"/var/log/wmbusmeters/meter_readings
fi

####################################################################
##
## Install /etc/logrotate.d/wmbusmeters
##

if [ ! -f "$ROOT"/etc/logrotate.d/wmbusmeters ]
then
    mkdir -p "$ROOT"/etc/logrotate.d
    # Create logrotate file
    cat <<EOF > "$ROOT"/etc/logrotate.d/wmbusmeters
/var/log/wmbusmeters/*.log {
    rotate 12
    weekly
    compress
    missingok
    postrotate
    start-stop-daemon -K -p /var/run/wmbusmeters.pid -s HUP -x /usr/sbin/wmbusmeters -q
    endscript
EOF
    echo logrotate: created "$ROOT"/etc/logrotate.d/wmbusmeters
fi

####################################################################
##
## Install /etc/wmbusmeters.conf
##

if [ ! -f "$ROOT"/etc/wmbusmeters.conf ]
then
    # Create default configuration
    mkdir -p "$ROOT"/etc/
    cat <<EOF > "$ROOT"/etc/wmbusmeters.conf
loglevel=normal
device=auto
logtelegrams=false
format=json
meterfiles=/var/log/wmbusmeters/meter_readings
meterfilesaction=overwrite
logfile=/var/log/wmbusmeters/wmbusmeters.log
EOF
    chmod 644 "$ROOT"/etc/wmbusmeters.conf
    echo conf file: created "$ROOT"/etc/wmbusmeters.conf
else
    echo conf file: "$ROOT"/etc/wmbusmeters.conf unchanged
fi

####################################################################
##
## Create /etc/wmbusmeters.d
##

if [ ! -d "$ROOT"/etc/wmbusmeters.d ]
then
    # Create the configuration directory
    mkdir -p "$ROOT"/etc/wmbusmeters.d
    chmod -R 655 "$ROOT"/etc/wmbusmeters.d
    echo conf dir: created "$ROOT"/etc/wmbusmeters.d
else
    echo conf dir: "$ROOT"/etc/wmbusmeters.d unchanged
fi

####################################################################
##
## Create /etc/systemd/system/wmbusmeters.service
##

if [ ! -f "$ROOT"/etc/systemd/system/wmbusmeters.service ]
then
    mkdir -p "$ROOT"/etc/systemd/system/
    # Create service file
    cat <<EOF > "$ROOT"/etc/systemd/system/wmbusmeters.service
[Unit]
Description=wmbusmeters service
After=network.target
StopWhenUnneeded=true

[Service]
Type=forking
PrivateTmp=yes
User=wmbusmeters
Group=wmbusmeters
Restart=always
RestartSec=1
StartLimitIntervalSec=10
StartLimitInterval=10
StartLimitBurst=3

# Run ExecStartPre with root-permissions

PermissionsStartOnly=true
ExecStartPre=-/bin/mkdir -p /var/log/wmbusmeters/meter_readings
ExecStartPre=/bin/chown -R wmbusmeters:wmbusmeters /var/log/wmbusmeters
ExecStartPre=-/bin/mkdir -p /var/run/wmbusmeters
ExecStartPre=/bin/chown -R wmbusmeters:wmbusmeters /var/run/wmbusmeters

ExecStart=/usr/sbin/wmbusmetersd /var/run/wmbusmeters/wmbusmeters.pid
PIDFile=/var/run/wmbusmeters/wmbusmeters.pid

[Install]
WantedBy=multi-user.target
EOF

    echo systemd: installed "$ROOT"/etc/systemd/system/wmbusmeters.service
else
    echo systemd: "$ROOT"/etc/systemd/system/wmbusmeters.service unchanged
fi


####################################################################
##
## Create /etc/udev/rules.d/99-wmbus-usb-serial.rules
##
if [ "$ADDUDEVRULES" = "true" ]
then
	if [ ! -f "$ROOT"/etc/udev/rules.d/99-wmbus-usb-serial.rules ]
	then
		mkdir -p "$ROOT"/etc/udev/rules.d
		# Create service file
		cat <<EOF > "$ROOT"/etc/udev/rules.d/99-wmbus-usb-serial.rules
SUBSYSTEM=="tty", ATTRS{idVendor}=="10c4", ATTRS{idProduct}=="ea60",SYMLINK+="im871a",MODE="0660", GROUP="wmbusmeters",TAG+="systemd",ENV{SYSTEMD_WANTS}="wmbusmeters.service"
SUBSYSTEM=="tty", ATTRS{idVendor}=="0403", ATTRS{idProduct}=="6001",SYMLINK+="amb8465",MODE="0660", GROUP="wmbusmeters",TAG+="systemd",ENV{SYSTEMD_WANTS}="wmbusmeters.service"
SUBSYSTEM=="usb", ATTRS{idVendor}=="0bda", ATTRS{idProduct}=="2838",SYMLINK+="rtlsdr",MODE="0660", GROUP="wmbusmeters",TAG+="systemd",ENV{SYSTEMD_WANTS}="wmbusmeters.service"
EOF
		echo udev: installed "$ROOT"/etc/udev/rules.d/99-wmbus-usb-serial.rules
	else
		echo udev: "$ROOT"/etc/udev/rules.d/99-wmbus-usb-serial.rules unchanged
	fi
fi
