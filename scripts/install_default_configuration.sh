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
# Remember to change auto here to the device you are going to use in production.
device=auto:t1
logfile=/var/log/wmbusmeters/wmbusmeters.log
# Set to true to capture all received telegrams in log file.
logtelegrams=false
format=json
# Enable the meterfiles to write telegrams to disk.
#meterfiles=/var/lib/wmbusmeters/meter_readings
#meterfilesaction=overwrite
# Enable execution of a shell command for each received telegram. For example: curl or mqtt
#shell=/usr/bin/mosquitto_pub -h localhost -t wmbusmeters/$METER_ID -m "$METER_JSON"
#shell=psql water -c "insert into consumption values ('$METER_ID',$METER_TOTAL_M3,'$METER_TIMESTAMP') "
# The alarmshell is executed when a problem with the receiving radio hardware is detected.
#alarmshell=/usr/bin/mosquitto_pub -h localhost -t wmbusmeters_alarm -m "$ALARM_TYPE $ALARM_MESSAGE"
# The alarmtimeout and expected activity is also used to detect failing receiving radio hardware.
#alarmtimeout=1h
#alarmexpectedactivity=mon-sun(00-23)
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
## Create /etc/wmbusmeters.drivers.d
##

if [ ! -d "$ROOT"/etc/wmbusmeters.drivers.d ]
then
    # Create the drivers directory
    mkdir -p "$ROOT"/etc/wmbusmeters.drivers.d
    chmod -R 755 "$ROOT"/etc/wmbusmeters.drivers.d
    echo "conf dir: created $ROOT/etc/wmbusmeters.drivers.d"
else
    echo "conf dir: $ROOT/etc/wmbusmeters.drivers.d unchanged"
fi
