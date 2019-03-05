#!/bin/bash

PROG="$1"

mkdir -p testoutput

TEST=testoutput
SIM=simulations/simulation_multiple_qcalorics.txt

cat $SIM | grep '^{' > $TEST/test_expected.txt
$PROG --format=json $SIM \
      Element qcaloric '*' '' \
      > $TEST/test_output.txt

if [ "$?" == "0" ]
then
    cat $TEST/test_output.txt | sed 's/"timestamp":"....-..-..T..:..:..Z"/"timestamp":"1111-11-11T11:11:11Z"/' > $TEST/test_responses.txt
    diff $TEST/test_expected.txt $TEST/test_responses.txt
    if [ "$?" == "0" ]
    then
        echo Wildcard \'\*\' id OK
    fi
else
    echo Failure.
    exit 1
fi

cat $SIM | grep '^{' | grep -v 78563414 > $TEST/test_expected.txt

$PROG --format=json $SIM \
      Element qcaloric '78563412,78563413' '' \
      > $TEST/test_output.txt

if [ "$?" == "0" ]
then
    cat $TEST/test_output.txt | sed 's/"timestamp":"....-..-..T..:..:..Z"/"timestamp":"1111-11-11T11:11:11Z"/' > $TEST/test_responses.txt
    diff $TEST/test_expected.txt $TEST/test_responses.txt
    if [ "$?" == "0" ]
    then
        echo Multiple ids2 OK
    fi
else
    echo Failure.
    exit 1
fi

cat $SIM | grep '^{' > $TEST/test_expected.txt

$PROG --format=json $SIM \
      Element qcaloric '78563412,78563413,78563414' '' \
      > $TEST/test_output.txt

if [ "$?" == "0" ]
then
    cat $TEST/test_output.txt | sed 's/"timestamp":"....-..-..T..:..:..Z"/"timestamp":"1111-11-11T11:11:11Z"/' > $TEST/test_responses.txt
    diff $TEST/test_expected.txt $TEST/test_responses.txt
    if [ "$?" == "0" ]
    then
        echo Multiple ids3 OK
    fi
else
    echo Failure.
    exit 1
fi
