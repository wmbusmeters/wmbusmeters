#!/bin/bash

PROG="$1"
TEST=testoutput
rm -f $TEST/thelog2.txt
rm -rf $TEST/meter_readings2
mkdir -p $TEST/meter_readings2

$PROG --useconfig=tests/config2

ERRORS=false

cat simulations/simulation_t1.txt | grep '^{' | grep 12345699 | tail -n 1 > $TEST/test_expected.txt
cat $TEST/meter_readings2/MoreWater | sed 's/"timestamp":"....-..-..T..:..:..Z"/"timestamp":"1111-11-11T11:11:11Z"/' > $TEST/test_response.txt
diff $TEST/test_expected.txt $TEST/test_response.txt
if [ ! "$?" = "0" ]
then
    ERRORS=true
fi

cat simulations/simulation_t1.txt | grep '^{' | grep 12345678 | tail -n 1 > $TEST/test_expected.txt
cat $TEST/meter_readings2/MyWarmWater | sed 's/"timestamp":"....-..-..T..:..:..Z"/"timestamp":"1111-11-11T11:11:11Z"/' > $TEST/test_response.txt
diff $TEST/test_expected.txt $TEST/test_response.txt
if [ ! "$?" = "0" ]
then
    ERRORS=true
fi

cat simulations/simulation_t1.txt | grep '^{' | grep 11111111 | tail -n 1 > $TEST/test_expected.txt
cat $TEST/meter_readings2/MyColdWater | sed 's/"timestamp":"....-..-..T..:..:..Z"/"timestamp":"1111-11-11T11:11:11Z"/' > $TEST/test_response.txt
diff $TEST/test_expected.txt $TEST/test_response.txt
if [ ! "$?" = "0" ]
then
    ERRORS=true
fi

RES=$(cat $TEST/thelog2.txt | grep -v "logging started" | grep -v "waiting for telegrams" | grep -v "shutting down")

if [ ! "$RES" = "" ]
then
    ERRORS=true
    cat $TEST/thelog2.txt
fi

if [ "$ERRORS" = "true" ]
then
    echo Failed config2 tests
else
    echo Config2 with logfile and meterfilesdir OK
fi
