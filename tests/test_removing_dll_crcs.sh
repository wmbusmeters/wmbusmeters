#!/bin/sh

PROG="$1"
TEST=testoutput
mkdir -p $TEST

TESTNAME="Test auto removal of dll crcs in simulated telegrams"
TESTRESULT="ERROR"

cat > $TEST/test_expected.txt <<EOF
No meters configured. Printing id:s of all telegrams heard!
Received telegram from: 000bc37c
          manufacturer: (APT) Unknown (0x8614)
                  type: Gas meter (0x03)
                   ver: 0x03
                driver: apator08
Received telegram from: 27293981
          manufacturer: (SON) Sontex, Switzerland (0x4dee)
                  type: Heat Cost Allocator (0x08)
                   ver: 0x16
                driver: sontex868
EOF

$PROG simulations/simulation_dll_crcs.txt > $TEST/test_output.txt 2>&1

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
