#!/bin/sh

PROG="$1"

if [ "$PROG" = "" ]
then
    echo Please supply the binary to be tested as the first argument.
    exit 1
fi

TEST=testoutput

rm -rf $TEST
mkdir -p $TEST

TESTNAME="Test extra help for library field in driver"
TESTRESULT="ERROR"

$PROG --useconfig=tests/config18 --listfields=iperl | grep total_m3 > $TEST/test_output.txt

if ! grep -q 'Extra help!' $TEST/test_output.txt
then
    echo "ERROR: $TESTNAME ($0)"
    cat $TEST/test_output.txt
    echo "Expected \"Extra help!\" to be appended to total_m3 field"
    exit 1
fi

echo "OK: $TESTNAME"
