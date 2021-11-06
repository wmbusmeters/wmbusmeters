#!/bin/sh

PROG="$1"
TEST=testoutput
mkdir -p $TEST

TESTNAME="Test broken telegrams"
TESTRESULT="ERROR"

cat > $TEST/test_expected.txt <<EOF
(dvparser) warning: unexpected end of data
(dvparser) found new format "046D036E51706CE1F14302FF2C0259D40902FD66A000" with hash 48a9, remembering!
(dvparser) warning: unexpected end of data
(dvparser) warning: unexpected end of data
EOF

$PROG --format=fields --selectfields=id,current_consumption_hca,device_date_time --debug simulations/simulation_broken.txt HCA auto 27293981 NOKEY 2>&1 | grep dvparser > $TEST/test_output.txt

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
