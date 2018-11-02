#!/bin/bash

PROG="$1"

cat simulation_c1.txt | grep '^{' > test_expected.txt
$PROG --robot=json simulation_c1.txt \
      MyHeater multical302 12345678 "" \
      MyTapWater multical21 76348799 "" \
      MyElectricity omnipower 15947107 "" \
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

cat simulation_t1.txt | grep '^{' > test_expected.txt
$PROG --robot=json simulation_t1.txt \
      MyWarmWater supercom587 12345678 "" \
      MyColdWater supercom587 11111111 "" \
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
