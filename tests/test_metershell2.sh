#!/bin/sh

PROG="$1"
TEST=testoutput
mkdir -p $TEST

TESTNAME="Test metershell in config file"
TESTRESULT="ERROR"

$PROG --useconfig=tests/config14 2> $TEST/test_stderr.txt > $TEST/test_output.txt

if [ "$?" = "0" ]
then
    INFO=$(cat /tmp/wmbusmeters_metershell1_test)
    EXPECTED='TESTING METERSHELL 12345678'
    if [ "$INFO" = "$EXPECTED" ]
    then
        echo "OK: $TESTNAME (--metershell output)"
        TESTRESULT="OK"
    else
        echo "Expected: $EXPECTED"
        echo "Got     : $INFO"
    fi

    INFO=$(cat /tmp/wmbusmeters_metershell2_test)
    EXPECTED='TESTING SHELL 12345678'
    if [ "$INFO" = "$EXPECTED" ]
    then
        echo "OK: $TESTNAME (--shell output)"
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
