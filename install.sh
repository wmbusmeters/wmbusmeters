#!/bin/bash

if [ "$1" == "" ] || [ "$1" == "-h" ]
then
    echo "Usage: install.sh [binary] [root] [OPTIONS]
    Example: install.sh build/wmbusmeters /

	Options:
	--no-adduser		Do not add wmbusmeters user
	"
    exit 0
fi

if [ ! "$(basename "$1")" = "wmbusmeters" ]
then
    echo "Oups, please only try to install wmbusmeters using this script."
    exit 1
fi

if [ ! -x "$1" ]
then
    echo "This is not an executable."
    exit 1
fi

if [ ! -d "$2" ]
then
    echo "Oups, please supply a valid root directory."
    exit 1
fi

SRC=$1
ROOT="${2%/}"
ADDUSER=true

while [ $# -ne 0 ]
do
        ARG="$1"
        shift
        case "$ARG" in
        --no-adduser)
                ADDUSER=false
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
ln -s /usr/bin/wmbusmeters "$ROOT"/usr/sbin/wmbusmetersd

echo "binaries: installed $ROOT/usr/bin/wmbusmeters and $ROOT/usr/sbin/wmbusmetersd"

####################################################################
##
## Intall wmbusmeters manual page
##

rm -f "$ROOT"/usr/share/man/man1/wmbusmeters.1.gz
mkdir -p "$ROOT"/usr/share/man/man1
gzip -c wmbusmeters.1 > "$ROOT"/usr/share/man/man1/wmbusmeters.1.gz

echo "man page: installed $ROOT/usr/share/man/man1/wmbusmeters.1.gz"

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
    if [ $(getent group wmbusmeters) ]
    then
	echo "group: wmbusmeters unmodified"
    else
 	groupadd -f wmbusmeters
	echo "group: added wmbusmeters"
    fi

    if [ -z "$ID" ]
    then
        # Create the wmbusmeters user
        useradd --system --shell $USERSHELL -g wmbusmeters wmbusmeters
        echo "user: added wmbusmeters"
    else
        echo "user: wmbusmeters unmodified"
    fi

    if [ $(getent group dialout) ]
    then
	if [ "$(groups wmbusmeters | grep -o dialout)" = "" ]
	then
		# Add the wmbusmeters user to dialout
		usermod -a -G dialout wmbusmeters
		echo "user: added wmbusmeters to dialout group"
	else
		echo "user: wmbusmeters already added to dialout"
	fi
    else
        echo "dialout group does not exist"
    fi

    if [ $(getent group uucp) ]
    then
	if [ "$(groups wmbusmeters | grep -o uucp)" = "" ]
	then
		# Add the wmbusmeters user to uucp
		usermod -a -G uucp wmbusmeters
		echo "user: added wmbusmeters to uucp group"
	else
		echo "user: wmbusmeters already added to uucp"
	fi
    else
        echo "uucp group does not exist"
    fi
fi

####################################################################
##
## Prepare for  /run/wmbusmeters.pid
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
    echo "log: created $ROOT/var/log/wmbusmeters/meter_readings"
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
        /bin/kill -HUP `cat /run/wmbusmeters/wmbusmeters.pid 2> /dev/null` 2> /dev/null || true
    endscript
EOF
    echo "logrotate: created $ROOT/etc/logrotate.d/wmbusmeters"
else
    echo "conf file: $ROOT/etc/logrotate.d/wmbusmeters unchanged"
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
    echo "conf file: created $ROOT/etc/wmbusmeters.conf"
else
    echo "conf file: $ROOT/etc/wmbusmeters.conf unchanged"
fi

####################################################################
##
## Create /etc/wmbusmeters.d
##

if [ ! -d "$ROOT"/etc/wmbusmeters.d ]
then
    # Create the configuration directory
    mkdir -p "$ROOT"/etc/wmbusmeters.d
    chmod -R 755 "$ROOT"/etc/wmbusmeters.d
    echo "conf dir: created $ROOT/etc/wmbusmeters.d"
else
    echo "conf dir: $ROOT/etc/wmbusmeters.d unchanged"
fi

####################################################################
##
## Create /lib/systemd/system/wmbusmeters.service
##

SYSTEMD_NEEDS_RELOAD=false
mkdir -p "$ROOT"/lib/systemd/system/

OLD_WMBS=~/old.wmbusmeters.service.backup
CURR_WMBS="$ROOT"/lib/systemd/system/wmbusmeters.service
if [ -f $CURR_WMBS ]
then
    echo "systemd: backing up $CURR_WMBS to here: $OLD_WMBS"
    cp $CURR_WMBS $OLD_WMBS 2>/dev/null
fi

# Create service file for starting daemon without explicit device.
# This means that wmbusmeters will rely on the conf file device setting.
cat <<'EOF' > $CURR_WMBS
[Unit]
Description="wmbusmeters service"
Documentation=https://github.com/weetmuts/wmbusmeters
Documentation=man:wmbusmeters(1)
After=network.target
StopWhenUnneeded=false
StartLimitIntervalSec=10
StartLimitInterval=10
StartLimitBurst=3

[Service]
Type=forking
PrivateTmp=yes
User=wmbusmeters
Group=wmbusmeters
Restart=always
RestartSec=1

# Run ExecStartPre with root-permissions

PermissionsStartOnly=true
ExecStartPre=-/bin/mkdir -p /var/log/wmbusmeters/meter_readings
ExecStartPre=/bin/chown -R wmbusmeters:wmbusmeters /var/log/wmbusmeters
ExecStartPre=-/bin/mkdir -p /run/wmbusmeters
ExecStartPre=/bin/chown -R wmbusmeters:wmbusmeters /run/wmbusmeters

ExecStart=/usr/sbin/wmbusmetersd /run/wmbusmeters/wmbusmeters.pid
ExecReload=/bin/kill -HUP $MAINPID
PIDFile=/run/wmbusmeters/wmbusmeters.pid

[Install]
WantedBy=multi-user.target
EOF

if diff $OLD_WMBS $CURR_WMBS 1>/dev/null
then
    echo "systemd: no changes to $CURR_WMBS"
else
    echo "systemd: updated $CURR_WMBS"
    SYSTEMD_NEEDS_RELOAD=true
fi

OLD_WMBAS=~/old.wmbusmeters@.service.backup
CURR_WMBAS="$ROOT"/lib/systemd/system/wmbusmeters@.service
if [ -f $CURR_WMBAS ]
then
    echo "systemd: removing $CURR_WMBAS"
    echo "systemd: backing up $CURR_WMBAS to here: $OLD_WMBAS"
    cp $CURR_WMBAS $OLD_WMBAS 2>/dev/null
    rm $CURR_WMBAS
    SYSTEMD_NEEDS_RELOAD=true
fi

####################################################################
##
## Remove any existing /etc/udev/rules.d/99-wmbus-usb-serial.rules
## The new wmbuseters daemon is not going to use these.
##

if [ -f "$ROOT"/etc/udev/rules.d/99-wmbus-usb-serial.rules ]
then
    echo "udev: removing "$ROOT"/etc/udev/rules.d/99-wmbus-usb-serial.rules"
    echo "udev: backup stored here: ~/old.wmbusmeters-wmbus-usb-serial.rules.backup"
    cp "$ROOT"/etc/udev/rules.d/99-wmbus-usb-serial.rules ~/old.wmbusmeters-wmbus-usb-serial.rules.backup
    rm "$ROOT"/etc/udev/rules.d/99-wmbus-usb-serial.rules
    UDEV_NEEDS_RELOAD=true
fi

if [ "$SYSTEMD_NEEDS_RELOAD" = "true" ]
then
    echo
    echo
    echo "You need to reload systemd configuration! Please do:"
    echo "sudo systemctl daemon-reload"
fi

if [ "$UDEV_NEEDS_RELOAD" = "true" ]
then
    echo
    echo
    echo "You need to reload udev configuration! Please do:"
    echo "sudo udevadm control --reload-rules"
    echo "sudo udevadm trigger"
fi
