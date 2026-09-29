#!/bin/sh

. tests/include.sh

PROG="$1"
TEST=testoutput
mkdir -p $TEST

TESTNAME="Test LSE bridge decapsulation of relayed inner telegrams"
TESTRESULT="ERROR"

cat > $TEST/test_expected.txt <<EOF
No meters configured. Printing id:s of all telegrams heard!
(wmbus) LSE bridge 00406057: decapsulating inner telegram id 88441603 (50 bytes).
Received telegram from: 88441603
          manufacturer: (LSE) Landis Staefa electronic (0x3265)
                  type: Heat Cost Allocator (0x08)
                   ver: 0x34
                driver: qcaloric
(wmbus) LSE bridge inner telegram was not handled.
Received telegram from: 00a0316a
          manufacturer: (LSE) Landis Staefa electronic (0xb265)
                  type: Unknown (0xfe)
                   ver: 0xf1
                driver: unknown!
EOF

$PROG simulations/simulation_lse_bridge.txt > $TEST/test_output.txt 2>&1

if [ "$?" = "0" ]
then
    diff $TEST/test_expected.txt $TEST/test_output.txt
    if [ "$?" = "0" ]
    then
        TESTRESULT="OK"
    fi
else
    echo "wmbusmeters returned error code: $?"
    cat $TEST/test_output.txt
fi

if [ "$TESTRESULT" = "ERROR" ]; then printERROR "$TESTNAME"; exit 1; else printOK "$TESTNAME"; fi