#!/bin/sh

PROG="$1"
TEST=testoutput
mkdir -p $TEST

TESTNAME="Test shell in config file"
TESTRESULT="ERROR"

$PROG --useconfig=tests/config5 > $TEST/test_output.txt

if [ "$?" = "0" ]
then
    INFO=$(cat /tmp/wmbusmeters_meter_shell_test | sed 's/"timestamp":"....-..-..T..:..:..Z"/"timestamp":"1111-11-11T11:11:11Z"/')
    EXPECTED=$(echo 'TESTING SHELL {"media":"warm water","meter":"supercom587","name":"Vatten","id":"12345678","total_m3":5.548,"timestamp":"1111-11-11T11:11:11Z"}')
    if [ "$INFO" = "$EXPECTED" ]
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
