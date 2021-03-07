#!/bin/sh

# Create default directories and copy initial config if it doesn't exist
[ ! -d $SNAP_COMMON/logs/meter_readings ] && mkdir -p $SNAP_COMMON/logs/meter_readings
[ ! -d $SNAP_COMMON/etc/wmbusmeters.d ] && mkdir -p $SNAP_COMMON/etc/wmbusmeters.d

if [ ! -e "$SNAP_COMMON/etc/wmbusmeters.conf" ]
then
  cp $SNAP/etc/wmbusmeters.conf $SNAP_COMMON/etc/wmbusmeters.conf
  sed -i "s|^meterfiles=.*|meterfiles=$SNAP_COMMON/logs/meter_readings|g" "$SNAP_COMMON/etc/wmbusmeters.conf"
  sed -i "s|^logfile=.*|logfile=$SNAP_COMMON/logs/wmbusmeters.log|g" "$SNAP_COMMON/etc/wmbusmeters.conf"
  sed -i "/^device=.*/a listento=t1" "$SNAP_COMMON/etc/wmbusmeters.conf"
  sed -i "/^device=.*/a # To use rtl_433 uncomment following line \n#device=rtl433:CMD(LD_LIBRARY_PATH=/var/lib/snapd/lib/gl:/var/lib/snapd/lib/gl32:/var/lib/snapd/void:/snap/wmbusmeters/current/lib/x86_64-linux-gnu:/snap/wmbusmeters/current/usr/lib/x86_64-linux-gnu:/snap/wmbusmeters/current/lib:/snap/wmbusmeters/current/usr/lib:/snap/wmbusmeters/current/usr/lib/arm-linux-gnueabihf /snap/wmbusmeters/current/usr/bin/rtl_433 -F csv -f 868.95M)" "$SNAP_COMMON/etc/wmbusmeters.conf"
  sed -i "/^device=.*/a # To use rtl_wmbus uncomment following line \n#device=rtlwmbus:CMD(LD_LIBRARY_PATH=/var/lib/snapd/lib/gl:/var/lib/snapd/lib/gl32:/var/lib/snapd/void:/snap/wmbusmeters/current/lib/x86_64-linux-gnu:/snap/wmbusmeters/current/usr/lib/x86_64-linux-gnu:/snap/wmbusmeters/current/lib:/snap/wmbusmeters/current/usr/lib:/snap/wmbusmeters/current/usr/lib/arm-linux-gnueabihf /snap/wmbusmeters/current/usr/bin/rtl_sdr -f 868.95M -s 1600000 - 2>/dev/null | /snap/wmbusmeters/current/usr/bin/rtl_wmbus)" "$SNAP_COMMON/etc/wmbusmeters.conf"
fi

# Launch the snap
$SNAP/usr/bin/wmbusmeters --useconfig=$SNAP_COMMON