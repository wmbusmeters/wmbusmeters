#!/bin/sh
# Copyright (C) 2021 Fredrik Öhrström (gpl-3.0-or-later)

####################################################################
##
## Prepare for  /var/log/wmbusmeters and /var/lib/wmbusmeters/meter_readings
##

if [ ! -d "$ROOT"/var/lib/wmbusmeters/meter_readings ]
then
    # Create the log directories
    mkdir -p "$ROOT"/var/lib/wmbusmeters/meter_readings
    chown -R wmbusmeters:wmbusmeters "$ROOT"/var/lib/wmbusmeters
    echo "log: created $ROOT/var/lib/wmbusmeters/meter_readings"
else
    echo "log: $ROOT/var/lib/wmbusmeters/meter_readings unchanged"
fi

if [ ! -d "$ROOT"/var/lib/wmbusmeters/wmbusmeters.drivers.d ]
then
    # Create the downloadable drivers directory
    mkdir -p "$ROOT"/var/lib/wmbusmeters/wmbusmeters.drivers.d
    chown -R wmbusmeters:wmbusmeters "$ROOT"/var/lib/wmbusmeters
    echo "drivers: created $ROOT/var/lib/wmbusmeters/wmbusmeters.drivers.d"
else
    echo "drivers: $ROOT/var/lib/wmbusmeters/wmbusmeters.drivers.d"
fi

if [ ! -d "$ROOT"/var/log/wmbusmeters ]
then
    # Create the log directories
    mkdir -p "$ROOT"/var/log/wmbusmeters
    chown -R wmbusmeters:wmbusmeters "$ROOT"/var/log/wmbusmeters
    echo "log: created $ROOT/var/log/wmbusmeters"
else
    echo "log: $ROOT/var/log/wmbusmeters"
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
        /bin/kill -HUP \`cat /run/wmbusmeters/wmbusmeters.pid 2> /dev/null\` 2> /dev/null || true
    endscript
}
EOF
    echo "logrotate: created $ROOT/etc/logrotate.d/wmbusmeters"
else
    echo "conf file: $ROOT/etc/logrotate.d/wmbusmeters unchanged"
fi
