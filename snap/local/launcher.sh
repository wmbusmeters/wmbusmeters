#!/bin/sh

# Create default directories and copy initial config if it doesn't exist
[ ! -d $SNAP_COMMON/logs/meter_readings ] && mkdir -p $SNAP_COMMON/logs/meter_readings
[ ! -d $SNAP_COMMON/etc/wmbusmeters.d ] && mkdir -p $SNAP_COMMON/etc/wmbusmeters.d
[ ! -d $SNAP_COMMON/etc/wmbusmeters.drivers.d ] && mkdir -p $SNAP_COMMON/etc/wmbusmeters.drivers.d

if [ ! -e "$SNAP_COMMON/etc/wmbusmeters.conf" ]
then
  cp $SNAP/etc/wmbusmeters.conf $SNAP_COMMON/etc/wmbusmeters.conf
  sed -i "s|^meterfiles=.*|meterfiles=$SNAP_COMMON/logs/meter_readings|g" "$SNAP_COMMON/etc/wmbusmeters.conf"
  sed -i "s|^logfile=.*|logfile=$SNAP_COMMON/logs/wmbusmeters.log|g" "$SNAP_COMMON/etc/wmbusmeters.conf"
fi

# Launch the snap
$SNAP/usr/bin/wmbusmeters --useconfig=$SNAP_COMMON
