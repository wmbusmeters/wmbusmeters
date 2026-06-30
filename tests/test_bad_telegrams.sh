#!/bin/sh

PROG="$1"
TEST=testoutput
mkdir -p $TEST

$PROG 0D442D2C785634120107A2000000 Iffo izarv2 12345678 NOKEY > $TEST/test_output.txt 2>&1

if grep -q "(izar) Decoding PRIOS data failed. Ignoring telegram." $TEST/test_output.txt
then
    echo OK: too short izar telegram
else
    cat $TEST/test_output.txt
    echo ERROR: failed to handle too short izar telegram.
    exit 1
fi

$PROG --analyze=qheat 0E449344414620684737780779AABB > $TEST/test_output.txt 2>&1

if grep -q "013 C?: AA" $TEST/test_output.txt
then
    echo OK: too short qheatv2 telegram forced to be analyzed with qheat.
else
    cat $TEST/test_output.txt
    echo ERROR: failed to handle too short qheat telegram during analyze.
    exit 1
fi

exit 0
