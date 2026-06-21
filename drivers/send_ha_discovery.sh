#!/bin/bash

# Use this script like this:
# ./build/wmbusmeters --format=json --metershell='./drivers/send_ha_discovery.sh "$METER_DRIVER" "$METER_JSON"' 1844AE4C4455223368077A55000000_041389E20100023B0000 Vatten iperl 33225544 NOKEY

DRIVER=$(mktemp /tmp/send_ha_discovery.driver.XXXXXX)
TELEGRAM=$(mktemp /tmp/send_ha_discovery.json.XXXXXX)

DEBUG=false
if [ "$1" = "--debug" ]
then
    shift
    DEBUG=true
fi

if [ -z "$1" ] || [ -z "$2" ]
then
    echo "Usage: send_ha_discovery.sh {--debug} [driver.xmq] [telegram.json]"
    echo "       send_ha_discovery.sh {--debug} [driver as string] [telegram as string]"
    echo "You can also specify 1 to extract test telegram 1 from driver."
    exit 0
fi

if [ -f "$1" ]
then
    # Copy source file into file.
    cat "$1" > $DRIVER
else
    # Copy parameter string into file.
    cat <<< "$1" > $DRIVER
fi

if [ -f "$2" ]
then
    # Copy source file into file.
    cat "$2" > $TELEGRAM
else
    # Check if parameters is a number, eg 1 or 2 or 3 etc.
    if [ "$2" -eq "$2" ]
    then
        ARGS=$(xmq drivers/src/iperl.xmq select '(//args)[1]' to-text)
        HEX=$(xmq drivers/src/iperl.xmq select '(//telegram)[1]' to-text)
        wmbusmeters --format=json $HEX $ARGS > $TELEGRAM
    else
        # Copy parameter string into file.
        cat <<< "$2" > $TELEGRAM
    fi
fi

if [ "$DEBUG" = "true" ]
then
    echo "// Telegram content /////////////////////////////////////////////"
    ./build/xmq $TELEGRAM for-each '/telegram/node()' --shell='echo "${..}  ...  ${.}"'

    echo "// DRIVER as xmq /////////////////////////////////////////////"
    ./build/xmq $DRIVER to-xmq --compact

    echo "// DRIVER as json /////////////////////////////////////////////"
    ./build/xmq $DRIVER to-json

    echo "// DRIVER as xml /////////////////////////////////////////////"
    ./build/xmq $DRIVER to-xml
fi

# Generate a HA meter config from the driver and the first telegram.
# The first telegram is needed because drivers can handle many different meters
# and contain a lot of fields that are not relevant for this meter.

echo "// HA CONFIG //////////////////////////////////////////"

echo ./build/xmq $DRIVER transform --stringparam=file=$TELEGRAM drivers/to-ha-discovery.xslq

echo ///////////////////////////////////////////////////////

cat $TELEGRAM

#rm -f $DRIVER
#rm -f $TELEGRAM
