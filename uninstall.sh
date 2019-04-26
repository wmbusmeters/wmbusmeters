#!/bin/bash

if [ ! -d "$1" ]
then
    echo Oups, please supply a valid root directory.
    exit 1
fi

ROOT="${1%/}"

if [ -x "$ROOT"/usr/bin/wmbusmeters ] || [ -x "$ROOT"/usr/sbin/wmbusmeters ]
then
    rm -f "$ROOT"/usr/bin/wmbusmeters "$ROOT"/usr/sbin/wmbusmetersd
    echo binaries: removed "$ROOT"/usr/bin/wmbusmeters and "$ROOT"/usr/sbin/wmbusmetersd
fi

if [ -f "$ROOT"/usr/share/man/man1/wmbusmeters.1.gz ]
then
    rm "$ROOT"/usr/share/man/man1/wmbusmeters.1.gz
    echo man page: removed "$ROOT"/usr/share/man/man1/wmbusmeters.1.gz
fi

ID=$(id -u wmbusmeters 2>/dev/null)

if [ ! "$ID" = "" ]
then
    deluser wmbusmeters
    echo user: removed wmbusmeters
fi

if [ -d "$ROOT"/var/log/wmbusmeters/ ]
then
    rm -rf "$ROOT"/var/log/wmbusmeters/
    echo log dir: removed "$ROOT"/var/log/wmbusmeters/
fi

if [ -f "$ROOT"/etc/logrotate.d/wmbusmeters ]
then
    rm "$ROOT"/etc/logrotate.d/wmbusmeters
    echo logrotate file: removed "$ROOT"/etc/logrotate.d/wmbusmeters
fi

if [ -f "$ROOT"/etc/wmbusmeters.conf ]
then
    rm "$ROOT"/etc/wmbusmeters.conf
    echo conf file: removed "$ROOT"/etc/wmbusmeters.conf
fi

if [ -d "$ROOT"/etc/wmbusmeters.d ]
then
    rm -rf "$ROOT"/etc/wmbusmeters.d
    echo conf dir: removed "$ROOT"/etc/wmbusmeters.d
fi

if [ -f "$ROOT"/etc/systemd/system/wmbusmeters.service ]
then
    rm "$ROOT"/etc/systemd/system/wmbusmeters.service
    echo systemd: removed "$ROOT"/etc/systemd/system/wmbusmeters.service
fi

if [ -f "$ROOT"/etc/udev/rules.d/99-wmbus-usb-serial.rules ]
then
    rm "$ROOT"/etc/udev/rules.d/99-wmbus-usb-serial.rules
    echo udev: removed "$ROOT"/etc/udev/rules.d/99-wmbus-usb-serial.rules
fi
