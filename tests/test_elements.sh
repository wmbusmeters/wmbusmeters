#!/bin/sh

PROG="$1"
TEST=testoutput
mkdir -p $TEST/meter_readings3

TESTNAME="Test single meter conf file matches any id"
TESTRESULT="ERROR"

rm -f $TEST/meter_readings3/*
cat simulations/simulation_multiple_qcalorics.txt | grep '^{' > $TEST/test_expected.txt

$PROG --useconfig=tests/config3 > $TEST/test_output.txt

if [ "$?" = "0" ]
then
    FILES=$(ls $TEST/meter_readings3/ | tr '\n' ' ')
    if [ ! "$FILES" = "78563412 78563413 88563414 " ]
    then
        echo Failure, not the expected files in meter readings directory.
        exit 1
    fi
    cat $TEST/meter_readings3/* | sed 's/"timestamp":"....-..-..T..:..:..Z"/"timestamp":"1111-11-11T11:11:11Z"/' > $TEST/test_responses.txt
    diff $TEST/test_expected.txt $TEST/test_responses.txt
    if [ "$?" = "0" ]
    then
        echo OK: $TESTNAME
        TESTRESULT="OK"
    fi
fi

if [ "$TESTRESULT" = "ERROR" ]
then
    echo ERROR: $TESTNAME
    exit 1
fi
