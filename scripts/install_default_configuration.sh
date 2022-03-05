#!/bin/sh
# Copyright (C) 2021 Fredrik Öhrström (gpl-3.0-or-later)

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
device=auto:t1
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
