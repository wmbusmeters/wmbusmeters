#!/bin/bash

if [ "$1" = "" ]
then
    echo Usage: install.sh [binary] [root]
    echo Example: install.sh build/wmbusmeters /
    exit 0
fi

ADDUSER=true
if [ "$1" = "--no-adduser" ]
then
    ADDUSER=false
    shift
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

if [ ! -d "$2" ]
then
    echo Oups, please supply a valid root directory.
    exit 1
fi

ROOT="${2%/}"

rm -f $ROOT/usr/bin/wmbusmeters $ROOT/usr/sbin/wmbusmetersd
mkdir -p $ROOT/usr/bin
mkdir -p $ROOT/usr/sbin
cp "$1" $ROOT/usr/bin/wmbusmeters
ln -s $ROOT/usr/bin/wmbusmeters $ROOT/usr/sbin/wmbusmetersd

echo binaries: installed $ROOT/usr/bin/wmbusmeters and $ROOT/usr/sbin/wmbusmetersd

ID=$(id -u wmbusmeters 2>/dev/null)

if [ "$ADDUSER" = "true" ]
then
    if [ "$ID" = "" ]
    then
        # Create the wmbusmeters user
        adduser --no-create-home --shell $ROOT/usr/sbin/nologin --disabled-login --gecos "" wmbusmeters
        echo user: added wmbusmeters
    else
        echo user: wmbusmeters unmodified
    fi
fi

if [ ! -f $ROOT/etc/wmbusmeters.conf ]
then
    # Create default configuration
    mkdir -p $ROOT/etc/
    cat <<EOF > $ROOT/etc/wmbusmeters.conf
loglevel=normal
device=auto
logtelegrams=false
meterfiles=/tmp/wmbusmeters
EOF
    chmod 644 $ROOT/etc/wmbusmeters.conf
    echo conf file: created $ROOT/etc/wmbusmeters.conf
else
    echo conf file: $ROOT/etc/wmbusmeters.conf unchanged
fi

if [ ! -d $ROOT/etc/wmbusmeters.d ]
then
    # Create the configuration directory
    mkdir -p $ROOT/etc/wmbusmeters.d
    chmod -R 644 $ROOT/etc/wmbusmeters.d
    echo conf dir: created $ROOT/etc/wmbusmeters.d
else
    echo conf dir: $ROOT/etc/wmbusmeters.d unchanged
fi

if [ ! -f $ROOT/etc/systemd/system/wmbusmeters.service ]
then
    mkdir -p $ROOT/etc/systemd/system/
    # Create service file
    cat <<EOF > $ROOT/etc/systemd/system/wmbusmeters.service
[Unit]
Description=wmbusmeters service
After=network.target

[Service]
Type=simple
#Restart=always
RestartSec=1
User=wmbusmeters
ExecStart=/usr/bin/wmbusmeters --useconfig

[Install]
WantedBy=multi-user.target
EOF

    echo systemd: installed $ROOT/etc/systemd/system/wmbusmeters.service
else
    echo systemd: $ROOT/etc/systemd/system/wmbusmeters.service unchanged
fi
