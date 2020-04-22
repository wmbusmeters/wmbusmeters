#!/bin/sh

PROG="$1"

mkdir -p testoutput
TEST=testoutput

TESTNAME="Test that normal meterfiles are written"
TESTRESULT="ERROR"

rm -f /tmp/MyTapWater
cat simulations/simulation_c1.txt | grep '^{' | grep 76348799 | tail -n 1 > $TEST/test_expected.txt
$PROG --meterfiles --format=json simulations/simulation_c1.txt MyTapWater multical21 76348799 "" > /dev/null
cat /tmp/MyTapWater | sed 's/"timestamp":"....-..-..T..:..:..Z"/"timestamp":"1111-11-11T11:11:11Z"/' > $TEST/test_response.txt
diff $TEST/test_expected.txt $TEST/test_response.txt
if [ "$?" == "0" ]
then
    echo OK: $TESTNAME
    TESTRESULT="OK"
    rm /tmp/MyTapWater
fi

if [ "$TESTRESULT" = "ERROR" ]; then echo ERROR: $TESTNAME; exit 1; fi

TESTNAME="Test that meterfiles with name-id are written"
TESTRESULT="ERROR"

rm -rf /tmp/testmeters
mkdir /tmp/testmeters
cat simulations/simulation_c1.txt | grep '^{' | grep 76348799 | tail -n 1 > $TEST/test_expected.txt
$PROG --meterfiles=/tmp/testmeters --meterfilesnaming=name-id --format=json simulations/simulation_c1.txt MyTapWater multical21 76348799 "" > /dev/null
cat /tmp/testmeters/MyTapWater-76348799 | sed 's/"timestamp":"....-..-..T..:..:..Z"/"timestamp":"1111-11-11T11:11:11Z"/' > $TEST/test_response.txt
diff $TEST/test_expected.txt $TEST/test_response.txt
if [ "$?" == "0" ]
then
    echo OK: $TESTNAME
    TESTRESULT="OK"
    rm -rf /tmp/testmeters
fi

if [ "$TESTRESULT" = "ERROR" ]; then echo ERROR: $TESTNAME; exit 1; fi

TESTNAME="Test that meterfiles with id are written"
TESTRESULT="ERROR"

rm -rf /tmp/testmeters
mkdir /tmp/testmeters
cat simulations/simulation_c1.txt | grep '^{' | grep 76348799 | tail -n 1 > $TEST/test_expected.txt
$PROG --meterfiles=/tmp/testmeters --meterfilesnaming=id --format=json simulations/simulation_c1.txt MyTapWater multical21 76348799 ""
cat /tmp/testmeters/76348799 | sed 's/"timestamp":"....-..-..T..:..:..Z"/"timestamp":"1111-11-11T11:11:11Z"/' > $TEST/test_response.txt
diff $TEST/test_expected.txt $TEST/test_response.txt
if [ "$?" == "0" ]
then
    echo OK: $TESTNAME
    TESTRESULT="OK"
    rm -rf /tmp/testmeters
fi

if [ "$TESTRESULT" = "ERROR" ]; then echo ERROR: $TESTNAME; exit 1; fi


TESTNAME="Test that meterfiles with timestamps are written"
TESTRESULT="ERROR"

rm -rf /tmp/testmeters
mkdir /tmp/testmeters
cat simulations/simulation_c1.txt | grep '^{' | grep 76348799 | tail -n 1 > $TEST/test_expected.txt
$PROG --meterfiles=/tmp/testmeters --meterfilesnaming=id --meterfilestimestamp=day --format=json simulations/simulation_c1.txt MyTapWater multical21 76348799 ""
cat /tmp/testmeters/76348799_$(date +%Y-%m-%d) | sed 's/"timestamp":"....-..-..T..:..:..Z"/"timestamp":"1111-11-11T11:11:11Z"/' > $TEST/test_response.txt
diff $TEST/test_expected.txt $TEST/test_response.txt
if [ "$?" == "0" ]
then
    echo OK: $TESTNAME
    TESTRESULT="OK"
    rm -rf /tmp/testmeters
fi

if [ "$TESTRESULT" = "ERROR" ]; then echo ERROR: $TESTNAME; exit 1; fi

rm -rf /tmp/testmeters
mkdir /tmp/testmeters
cat simulations/simulation_c1.txt | grep '^{' | grep 76348799 | tail -n 1 > $TEST/test_expected.txt
$PROG --meterfiles=/tmp/testmeters --meterfilesnaming=id --meterfilestimestamp=minute --format=json simulations/simulation_c1.txt MyTapWater multical21 76348799 ""
cat /tmp/testmeters/76348799_$(date +%Y-%m-%d_%H:%M) | sed 's/"timestamp":"....-..-..T..:..:..Z"/"timestamp":"1111-11-11T11:11:11Z"/' > $TEST/test_response.txt
diff $TEST/test_expected.txt $TEST/test_response.txt
if [ "$?" == "0" ]
then
    echo OK: $TESTNAME
    TESTRESULT="OK"
    rm -rf /tmp/testmeters
fi

if [ "$TESTRESULT" = "ERROR" ]; then echo ERROR: $TESTNAME; exit 1; fi
