#!/bin/sh

PROG="$1"
TEST=testoutput
mkdir -p $TEST

TESTNAME="Test rtlwmbus with crc errors"
TESTRESULT="ERROR"

cat > $TEST/test_expected.txt <<EOF
Started config rtlwmbus on stdin listening on any
No meters configured. Printing id:s of all telegrams heard!
Received telegram from: 03727567
          manufacturer: (TCH) Techem Service (0x5068)
                  type: Heat Cost Allocator (0x08) encrypted
                   ver: 0x94
                device: rtlwmbus[]
                  rssi: 94 dBm
                driver: fhkvdataiv
EOF

cat simulations/simulation_rtlwmbus_errors.txt | $PROG stdin:rtlwmbus > $TEST/test_output.txt 2>&1

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
