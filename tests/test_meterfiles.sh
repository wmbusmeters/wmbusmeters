#!/bin/bash

PROG="$1"

mkdir -p testoutput

TEST=testoutput

rm -f /tmp/MyTapWater
cat simulations/simulation_c1.txt | grep '^{' | grep 76348799 | tail -n 1 > $TEST/test_expected.txt
$PROG --meterfiles --robot=json simulations/simulation_c1.txt MyTapWater multical21 76348799 "" > /dev/null
cat /tmp/MyTapWater | sed 's/"timestamp":"....-..-..T..:..:..Z"/"timestamp":"1111-11-11T11:11:11Z"/' > $TEST/test_response.txt
diff $TEST/test_expected.txt $TEST/test_response.txt
if [ "$?" == "0" ]
then
    echo Meterfiles OK
    rm /tmp/MyTapWater
fi

rm -rf /tmp/testmeters
mkdir /tmp/testmeters
cat simulations/simulation_c1.txt | grep '^{' | grep 76348799 | tail -n 1 > $TEST/test_expected.txt
$PROG --meterfiles=/tmp/testmeters --robot=json simulations/simulation_c1.txt MyTapWater multical21 76348799 "" > /dev/null
cat /tmp/testmeters/MyTapWater | sed 's/"timestamp":"....-..-..T..:..:..Z"/"timestamp":"1111-11-11T11:11:11Z"/' > $TEST/test_response.txt
diff $TEST/test_expected.txt $TEST/test_response.txt
if [ "$?" == "0" ]
then
    echo Meterfiles dir OK
    rm -rf /tmp/testmeters
fi
