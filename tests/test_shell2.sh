#!/bin/sh

PROG="$1"
TEST=testoutput
mkdir -p $TEST

TESTNAME="Test shell in config file"
TESTRESULT="ERROR"

$PROG --useconfig=tests/config5 2> $TEST/test_stderr.txt > $TEST/test_output.txt

if [ "$?" = "0" ]
then
    INFO=$(cat /tmp/wmbusmeters_meter_shell_test | sed 's/"timestamp":"....-..-..T..:..:..Z"/"timestamp":"1111-11-11T11:11:11Z"/')
    EXPECTED='TESTING SHELL {"_":"telegram","id":"12345678","media":"warm water","meter":"supercom587","name":"Vatten","software_version":"010002","status":"OK","timestamp":"1111-11-11T11:11:11Z","total_m3":5.548}'
    if [ "$INFO" = "$EXPECTED" ]
    then
        echo OK: $TESTNAME
        TESTRESULT="OK"
    else
        echo "Expected: $EXPECTED"
        echo "Got     : $INFO"
    fi
fi

if [ "$TESTRESULT" = "ERROR" ]
then
    echo ERROR: $TESTNAME
    exit 1
fi
