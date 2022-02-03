#!/bin/sh

PROG="$1"

mkdir -p testoutput
TEST=testoutput

TESTNAME="Test electricity meters with radio converter plugins"
TESTRESULT="ERROR"

METERS="MyEl ehzp 01135263 NOKEY"

cat simulations/simulation_el_meters.txt | grep '^{' > $TEST/test_expected.txt
$PROG --format=json simulations/simulation_el_meters.txt $METERS > $TEST/test_output.txt 2> $TEST/test_stderr.txt
if [ "$?" = "0" ]
then
    cat $TEST/test_output.txt | sed 's/"timestamp":"....-..-..T..:..:..Z"/"timestamp":"1111-11-11T11:11:11Z"/' > $TEST/test_responses.txt
    diff $TEST/test_expected.txt $TEST/test_responses.txt
    if [ "$?" = "0" ]
    then
        echo OK json: $TESTNAME
        TESTRESULT="OK"
    else
        TESTRESULT="ERROR"
    fi
else
    echo "wmbusmeters returned error code: $?"
    cat $TEST/test_output.txt
    cat $TEST/test_stderr.txt
fi

cat simulations/simulation_el_meters.txt | grep '^|' | sed 's/^|//' > $TEST/test_expected.txt
$PROG --format=fields simulations/simulation_el_meters.txt $METERS  > $TEST/test_output.txt 2> $TEST/test_stderr.txt
if [ "$?" = "0" ]
then
    cat $TEST/test_output.txt | sed 's/[0-9][0-9][0-9][0-9]-[0-9][0-9]-[0-9][0-9] [0-9][0-9]:[0-9][0-9].[0-9][0-9]$/1111-11-11 11:11.11/' > $TEST/test_responses.txt
    diff $TEST/test_expected.txt $TEST/test_responses.txt
    if [ "$?" = "0" ]
    then
        echo OK fields: $TESTNAME
        TESTRESULT="OK"
    else
        TESTRESULT="ERROR"
    fi
else
    echo "wmbusmeters returned error code: $?"
    cat $TEST/test_output.txt
    cat $TEST/test_stderr.txt
fi


if [ "$TESTRESULT" = "ERROR" ]
then
    echo ERROR: $TESTNAME
    exit 1
fi
