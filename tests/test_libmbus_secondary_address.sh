#!/bin/sh

PROG="$1"

mkdir -p testoutput

TEST=testoutput

########################################################
TESTNAME="Using libmbus secondary address fully specified format"
TESTRESULT="ERROR"

OUT=$($PROG --pollinterval=1s --format=fields --selectfields=temperature_c 68383868080072840200102941011B0D0000000265FE0842653009820165E70802FB1A480142FB1A45018201FB1A4E010C788402001002FD0F21000F0316 MyTempMeter piigth 100002842941011B NOKEY)

if [ "$OUT" != "23.02" ]
then
    echo "ERROR: Test 1 $TESTNAME"
    echo "Expected answer 23.02"
    exit 1
fi

OUT=$($PROG --pollinterval=1s --format=fields --selectfields=temperature_c 68383868080072840200102941011B0D0000000265FE0842653009820165E70802FB1A480142FB1A45018201FB1A4E010C788402001002FD0F21000F0316 MyTempMeter piigth 10000284.M=PII.V=01.T=1B NOKEY)

if [ "$OUT" != "23.02" ]
then
    echo "ERROR: Test 2 $TESTNAME"
    echo "Expected answer 23.02"
    exit 1
fi

OUT=$($PROG --pollinterval=1s --format=fields --selectfields=temperature_c 68383868080072840200102941011B0D0000000265FE0842653009820165E70802FB1A480142FB1A45018201FB1A4E010C788402001002FD0F21000F0316 MyTempMeter piigth 100002842941011C NOKEY)

if [ "$OUT" != "" ]
then
    echo "ERROR: Test 3 $TESTNAME"
    echo "Did not expect answer! $OUT"
    exit 1
fi

echo "OK: $TESTNAME"
