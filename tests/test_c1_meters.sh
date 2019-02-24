#!/bin/bash

PROG="$1"

mkdir -p testoutput

TEST=testoutput

cat simulations/simulation_c1.txt | grep '^{' > $TEST/test_expected.txt
$PROG --robot=json simulations/simulation_c1.txt \
      MyHeater multical302 12345678 "" \
      MyTapWater multical21 76348799 "" \
      Vadden multical21 44556677 "" \
      MyElectricity omnipower 15947107 "" \
      > $TEST/test_output.txt

if [ "$?" == "0" ]
then
    cat $TEST/test_output.txt | sed 's/"timestamp":"....-..-..T..:..:..Z"/"timestamp":"1111-11-11T11:11:11Z"/' > $TEST/test_responses.txt
    diff $TEST/test_expected.txt $TEST/test_responses.txt
    if [ "$?" == "0" ]
    then
        echo C1 OK
    fi
else
    Failure.
fi
