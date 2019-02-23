#!/bin/bash

if [ "$1" = "" ]
then
    echo Usage: install.sh build/wmbusmeters
    exit 0
fi

if [ ! $(basename "$1") = "wmbusmeters" ]
then
    echo Oups, please only try to install wmbusmeters using this script.
    exit 1
fi

if [ ! -x "$1" ]
then
    echo This is not an executable.
    exit 1
fi

if [ "$EUID" -ne 0 ]
then echo "Please run as root."
     exit
fi

rm -f /usr/bin/wmbusmeters /usr/sbin/wmbusmetersd
cp "$1" /usr/bin/wmbusmeters
ln -s /usr/bin/wmbusmeters /usr/sbin/wmbusmetersd

echo binaries: installed /usr/bin/wmbusmeters and /usr/sbin/wmbusmetersd

ID=$(id -u wmbusmeters 2>/dev/null)

if [ "$ID" == "" ]
then
    # Create the wmbusmeters user
    adduser --no-create-home --shell /usr/sbin/nologin --disabled-login --gecos "" wmbusmeters
    echo user: added wmbusmeters
else
    echo user: wmbusmeters unmodified
fi

if [ ! -f /etc/wmbusmeters.conf ]
then
    # Create default configuration
    cat <<EOF > /etc/wmbusmeters.conf
loglevel=normal
device=auto
logtelegrams=false
meterfiles=/tmp/wmbusmeters
EOF
    chown root:root /etc/wmbusmeters.conf
    chmod 644 /etc/wmbusmeters.d
    echo conf file: created /etc/wmbusmeters.conf
else
    echo conf file: /etc/wmbusmeters.conf unchanged
fi

if [ ! -d /etc/wmbusmeters.d ]
then
    # Create the configuration directory
    mkdir -p /etc/wmbusmeters.d
    chown -R root:root /etc/wmbusmeters.d
    chmod -R 644 /etc/wmbusmeters.d
    echo conf dir: created /etc/wmbusmeters.d
else
    echo conf dir: /etc/wmbusmeters.d unchanged
fi

if [ -d /etc/systemd/system ]
then
    if [ ! -f /etc/systemd/system/wmbusmeters.service ]
    then
        # Create service file
        cat <<EOF > /etc/systemd/system/wmbusmeters.service
[Unit]
Description=wmbusmeters service
After=network.target

[Service]
Type=simple
#Restart=always
RestartSec=1
User=wmbusmeters
ExecStart=/usr/sbin/wmbusmetersd

[Install]
WantedBy=multi-user.target
EOF

        echo systemd: installed /etc/systemd/system/wmbusmeters.service
    else
        echo systemd: /etc/systemd/system/wmbusmeters.service unchanged
    fi
fi
