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
EOF
    echo "logrotate: created $ROOT/etc/logrotate.d/wmbusmeters"
else
    echo "conf file: $ROOT/etc/logrotate.d/wmbusmeters unchanged"
fi
