#!/bin/bash

PROG="$1"
TEST=testoutput
mkdir -p $TEST/meter_readings3

rm -f $TEST/meter_readings3/Element
cat simulations/simulation_multiple_qcalorics.txt | grep '^{' > $TEST/test_expected.txt

$PROG --useconfig=tests/config3 > $TEST/test_output.txt

if [ "$?" == "0" ]
then
    cat $TEST/meter_readings3/Element | sed 's/"timestamp":"....-..-..T..:..:..Z"/"timestamp":"1111-11-11T11:11:11Z"/' > $TEST/test_responses.txt
    diff $TEST/test_expected.txt $TEST/test_responses.txt
    if [ "$?" == "0" ]
    then
        echo Elements OK
    fi
else
    echo Failure.
    exit 1
fi
