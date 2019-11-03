#!/bin/bash

PROG="$1"

mkdir -p testoutput
TEST=testoutput

TESTNAME="Test T1 meters"
TESTRESULT="ERROR"

cat simulations/simulation_t1.txt | grep '^{' > $TEST/test_expected.txt
$PROG --format=json simulations/simulation_t1.txt \
      MyWarmWater supercom587 12345678 "" \
      MyColdWater supercom587 11111111 "" \
      MoreWater   iperl       12345699 "" \
      WaterWater  iperl       33225544 "" \
      Wasser      apator162   20202020 "" \
      MyTapWatera apator162   21202020 "" \
      MyTapWaterb apator162   22202020 "" \
      MyElectricity1 amiplus  10101010 "" \
      Duschen     mkradio3    34333231 "" \
      HeatMeter   vario451    58234965 "" \
      HeatMeter   eurisii     88018801 "" \
      Tempoo      lansenth    00010203 "" \
      Rummet      rfmamb      11772288 "" \
      > $TEST/test_output.txt
if [ "$?" == "0" ]
then
    cat $TEST/test_output.txt | sed 's/"timestamp":"....-..-..T..:..:..Z"/"timestamp":"1111-11-11T11:11:11Z"/' > $TEST/test_responses.txt
    diff $TEST/test_expected.txt $TEST/test_responses.txt
    if [ "$?" == "0" ]
    then
        echo OK: $TESTNAME
        TESTRESULT="OK"
    fi
fi

if [ "$TESTRESULT" = "ERROR" ]
then
    echo ERROR: $TESTNAME
    exit 1
fi
