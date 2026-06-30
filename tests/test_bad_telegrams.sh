#!/bin/sh

PROG="$1"
TEST=testoutput
mkdir -p $TEST

TESTNAME="Test too short izar telegram"
TESTRESULT="ERROR"

$PROG 0D442D2C785634120107A2000000 Iffo izarv2 12345678 NOKEY > $TEST/test_output.txt 2>&1

if grep -q "(izar) Decoding PRIOS data failed. Ignoring telegram." $TEST/test_output.txt
then
    echo OK: too short izar telegram
else
    echo ERROR: failed to handle too short izar telegram.
    exit 1
fi

exit 0
