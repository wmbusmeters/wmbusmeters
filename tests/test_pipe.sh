#!/bin/sh

PROG="$1"

if [ "$PROG" = "" ]
then
    echo Please supply the binary to be tested as the first argument.
    exit 1
fi

OUTPUT=$( \
for i in $(seq 1 1000); \
do \
    echo "T1;1;1;2019-04-03 19:00:42.000;97;148;88888888;0x2e44333003020100071b7a634820252f2f0265840842658308820165950802fb1aae0142fb1aae018201fb1aa9012f" \
        | $PROG --silent stdin:rtlwmbus rum lansenth 00010203 NOKEY \
        | grep -q 21.8 ; [ "$?" = "0" ] || echo ERR ; \
done)

if [ -n "$OUTPUT" ]
then
    echo ERROR: reading all stdin before closing down
    echo $OUTPUT
    exit 1
else
    echo OK: reading all stdin before closing down
fi

exit 0
