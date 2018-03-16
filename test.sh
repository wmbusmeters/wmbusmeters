#!/bin/bash

PROG="$1"

cat simulation.txt | grep '^{' > test_expected.txt
$PROG --robot=json simulation.txt \
      MyTapWater water 76348799 "" \
      MyHeater heat 12345678 "" \
      MyElectricity electricity 15947107 "" \
      > test_output.txt
if [ "$?" == "0" ]
then
    cat test_output.txt | sed 's/"timestamp":"....-..-..T..:..:..Z"/"timestamp":"1111-11-11T11:11:11Z"/' > test_responses.txt
    diff test_expected.txt test_responses.txt
    if [ "$?" == "0" ]
    then
        echo OK
    fi
else
    Failure.
fi
