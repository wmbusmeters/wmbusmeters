#!/bin/bash

PROG="$1"

mkdir -p testoutput

TEST=testoutput

cat simulations/simulation_t1.txt | grep '^{' > $TEST/test_expected.txt
$PROG --format=json simulations/simulation_t1.txt \
      MyWarmWater supercom587 12345678 "" \
      MyColdWater supercom587 11111111 "" \
      MoreWater   iperl       12345699 "" \
      WaterWater  iperl       33225544 "" \
      Wasser      apator162   20202020 "" \
      MyElectricity amiplus  10101010 "" \
      > $TEST/test_output.txt
if [ "$?" == "0" ]
then
    cat $TEST/test_output.txt | sed 's/"timestamp":"....-..-..T..:..:..Z"/"timestamp":"1111-11-11T11:11:11Z"/' > $TEST/test_responses.txt
    diff $TEST/test_expected.txt $TEST/test_responses.txt
    if [ "$?" == "0" ]
    then
        echo T1 OK
    fi
else
    echo Failure.
    exit 1
fi
