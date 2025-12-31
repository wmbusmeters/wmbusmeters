#!/bin/sh

PROG="$1"
TEST=testoutput
mkdir -p $TEST

TESTNAME="Test rtlwmbus linkmodes"
TESTRESULT="ERROR"

cat > $TEST/test_expected.txt <<EOF
Started config rtlwmbus on stdin listening on any
No meters configured. Printing id:s of all telegrams heard!
Received telegram from: 94740459
          manufacturer: (QDS) Qundis, Germany (0x4493)
                  type: Heat Cost Allocator (0x08)
                   ver: 0x35
                device: rtlwmbus[]
                  rssi: 117 dBm
             link mode: c1
                driver: qcaloric
Received telegram from: 88888888
          manufacturer: (APA) Apator, Poland (0x601)
                  type: Water meter (0x07) encrypted
                   ver: 0x05
                device: rtlwmbus[]
                  rssi: 97 dBm
             link mode: t1
                driver: apator162
EOF

cat simulations/serial_rtlwmbus_linkmodes.msg | $PROG stdin:rtlwmbus > $TEST/test_output.txt 2>&1

if [ "$?" = "0" ]
then
    diff $TEST/test_expected.txt $TEST/test_output.txt
    if [ "$?" = "0" ]
    then
        echo OK: $TESTNAME
        TESTRESULT="OK"
    fi
else
    echo "wmbusmeters returned error code: $?"
    cat $TEST/test_output.txt
fi
