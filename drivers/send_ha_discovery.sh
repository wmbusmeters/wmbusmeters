#!/bin/bash

# Use this script like this:
# ./build/wmbusmeters --format=json --metershell='./drivers/send_ha_discovery.sh "$METER_DRIVER" "$METER_JSON"' 1844AE4C4455223368077A55000000_041389E20100023B0000 Vatten iperl 33225544 NOKEY

DRIVER=$(mktemp /tmp/send_ha_discovery.driver.XXXXXX)
TELEGRAM=$(mktemp /tmp/send_ha_discovery.json.XXXXXX)

cat <<< "$1" > $DRIVER
cat <<< "$2" > $TELEGRAM

echo // Telegram content /////////////////////////////////////////////
./build/xmq $TELEGRAM for-each '/telegram/node()' --shell='echo "${..}  ...  ${.}"'
echo // DRIVER as xmq /////////////////////////////////////////////
./build/xmq $DRIVER to-xmq --compact
echo // DRIVER as json /////////////////////////////////////////////
./build/xmq $DRIVER to-json
echo // DRIVER as xml /////////////////////////////////////////////
./build/xmq $DRIVER to-xml
# Generate a HA meter config. Reads the first telegram but does not acutally use the content.
echo // HA CONFIG //////////////////////////////////////////
./build/xmq $DRIVER transform --stringparam=file=$TELEGRAM drivers/to-ha-discovery.xslq to-json
echo ///////////////////////////////////////////////////////


rm -f $DRIVER_XMQ
rm -f $TELEGRAM_JSON
