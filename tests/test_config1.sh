#!/bin/bash

PROG="$1"
TEST=testoutput
mkdir -p $TEST

cat simulations/simulation_c1.txt | grep '^{' > $TEST/test_expected.txt

$PROG --useconfig=tests/config1 > $TEST/test_output.txt

if [ "$?" == "0" ]
then
    cat $TEST/test_output.txt | sed 's/"timestamp":"....-..-..T..:..:..Z"/"timestamp":"1111-11-11T11:11:11Z"/' > $TEST/test_responses.txt
    diff $TEST/test_expected.txt $TEST/test_responses.txt
    if [ "$?" == "0" ]
    then
        echo Config1 OK
    fi
else
    echo Failure.
    exit 1
fi
