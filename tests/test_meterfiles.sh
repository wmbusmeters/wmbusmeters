#!/bin/sh

. tests/include.sh

PROG="$1"
PROG_ABS=$(readlink -f "$PROG")

mkdir -p testoutput
TEST=testoutput

TESTNAME="Test that normal meterfiles are written"
TESTRESULT="ERROR"

rm -f $TEST/MyTapWater
cat simulations/simulation_c1.txt | grep '^{' | grep 76348799 | tail -n 1 | jq --sort-keys . > $TEST/test_expected.txt
$PROG --meterfiles=$TEST --format=json simulations/simulation_c1.txt MyTapWater multical21 76348799 "" \
      2> $TEST/test_stderr.txt
cat $TEST/MyTapWater | jq --sort-keys .  | sed 's/"timestamp": "....-..-..T..:..:..Z"/"timestamp": "1111-11-11T11:11:11Z"/' > $TEST/test_response.txt
diff $TEST/test_expected.txt $TEST/test_response.txt
if [ "$?" = "0" ]
then
    printOK "$TESTNAME"
    TESTRESULT="OK"
    rm -f $TEST/MyTapWater
fi

if [ "$TESTRESULT" = "ERROR" ]; then printERROR "$TESTNAME"; exit 1; fi

TESTNAME="Test that meterfiles with name-id are written"
TESTRESULT="ERROR"

rm -rf $TEST/testmeters
mkdir $TEST/testmeters
cat simulations/simulation_c1.txt | grep '^{' | grep 76348799 | tail -n 1 | jq --sort-keys . > $TEST/test_expected.txt
$PROG --meterfiles=$TEST/testmeters --meterfilesnaming=name-id --format=json \
      simulations/simulation_c1.txt MyTapWater multical21 76348799 "" \
      > /dev/null 2> $TEST/test_stderr.txt
cat $TEST/testmeters/MyTapWater-76348799 | jq --sort-keys . | sed 's/"timestamp": "....-..-..T..:..:..Z"/"timestamp": "1111-11-11T11:11:11Z"/' > $TEST/test_response.txt
diff $TEST/test_expected.txt $TEST/test_response.txt
if [ "$?" = "0" ]
then
    printOK "$TESTNAME"
    TESTRESULT="OK"
    rm -rf $TEST/testmeters
fi

if [ "$TESTRESULT" = "ERROR" ]; then printERROR "$TESTNAME"; exit 1; fi

TESTNAME="Test that meterfiles with id are written"
TESTRESULT="ERROR"

rm -rf $TEST/testmeters
mkdir $TEST/testmeters
cat simulations/simulation_c1.txt | grep '^{' | grep 76348799 | tail -n 1 | jq --sort-keys . > $TEST/test_expected.txt
$PROG --meterfiles=$TEST/testmeters --meterfilesnaming=id --format=json \
      simulations/simulation_c1.txt MyTapWater multical21 76348799 "" \
      2> $TEST/test_stderr.txt
cat $TEST/testmeters/76348799 | jq --sort-keys . | sed 's/"timestamp": "....-..-..T..:..:..Z"/"timestamp": "1111-11-11T11:11:11Z"/' > $TEST/test_response.txt
diff $TEST/test_expected.txt $TEST/test_response.txt
if [ "$?" = "0" ]
then
    printOK "$TESTNAME"
    TESTRESULT="OK"
    rm -rf $TEST/testmeters
fi

if [ "$TESTRESULT" = "ERROR" ]; then printERROR "$TESTNAME"; exit 1; fi


TESTNAME="Test that meterfiles with timestamps are written"
TESTRESULT="ERROR"

rm -rf $TEST/testmeters
mkdir $TEST/testmeters
cat simulations/simulation_c1.txt | grep '^{' | grep 76348799 | tail -n 1 | jq --sort-keys . > $TEST/test_expected.txt
$PROG --meterfiles=$TEST/testmeters --meterfilesnaming=id --meterfilestimestamp=month --format=json \
      simulations/simulation_c1.txt MyTapWater multical21 76348799 "" \
      2> $TEST/test_stderr.txt
cat $TEST/testmeters/76348799_$(date +%Y-%m) | jq --sort-keys . | sed 's/"timestamp": "....-..-..T..:..:..Z"/"timestamp": "1111-11-11T11:11:11Z"/' > $TEST/test_response.txt
diff $TEST/test_expected.txt $TEST/test_response.txt
if [ "$?" = "0" ]
then
    printOK "$TESTNAME"
    TESTRESULT="OK"
    rm -rf $TEST/testmeters
fi

if [ "$TESTRESULT" = "ERROR" ]; then printERROR "$TESTNAME"; exit 1; fi

rm -rf $TEST/testmeters
mkdir $TEST/testmeters
cat simulations/simulation_c1.txt | grep '^{' | grep 76348799 | tail -n 1 | jq --sort-keys . > $TEST/test_expected.txt
$PROG --meterfiles=$TEST/testmeters --meterfilesnaming=id --meterfilestimestamp=day --format=json \
      simulations/simulation_c1.txt MyTapWater multical21 76348799 "" \
      2> $TEST/test_stderr.txt
cat $TEST/testmeters/76348799_$(date +%Y-%m-%d) | jq --sort-keys . | sed 's/"timestamp": "....-..-..T..:..:..Z"/"timestamp": "1111-11-11T11:11:11Z"/' > $TEST/test_response.txt
diff $TEST/test_expected.txt $TEST/test_response.txt
if [ "$?" = "0" ]
then
    printOK "$TESTNAME"
    TESTRESULT="OK"
    rm -rf $TEST/testmeters
fi

if [ "$TESTRESULT" = "ERROR" ]; then printERROR "$TESTNAME"; exit 1; fi

rm -rf $TEST/testmeters
mkdir $TEST/testmeters
cat simulations/simulation_c1.txt | grep '^{' | grep 76348799 | tail -n 1 | jq --sort-keys . > $TEST/test_expected.txt
$PROG --meterfiles=$TEST/testmeters --meterfilesnaming=id --meterfilestimestamp=minute --format=json \
      simulations/simulation_c1.txt MyTapWater multical21 76348799 "" \
      2> $TEST/test_stderr.txt
cat $TEST/testmeters/76348799_$(date +%Y-%m-%d_%H:%M) | jq --sort-keys . | sed 's/"timestamp": "....-..-..T..:..:..Z"/"timestamp": "1111-11-11T11:11:11Z"/' > $TEST/test_response.txt
diff $TEST/test_expected.txt $TEST/test_response.txt
if [ "$?" = "0" ]
then
    printOK "$TESTNAME"
    TESTRESULT="OK"
    rm -rf $TEST/testmeters
fi

if [ "$TESTRESULT" = "ERROR" ]; then printERROR "$TESTNAME"; exit 1; fi
