#!/bin/bash

PROG="$1"
TESTINTERNAL=$(dirname $PROG)/testinternals

$TESTINTERNAL
if [ "$?" = "0" ]; then
    echo Internal test OK
fi

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
        echo C1 OK
    fi
else
    Failure.
fi

cat simulation_t1.txt | grep '^{' > test_expected.txt
$PROG --robot=json simulation_t1.txt \
      MyWarmWater supercom587 12345678 "" \
      MyColdWater supercom587 11111111 "" \
      MoreWater   iperl       12345699 "" \
      > test_output.txt
if [ "$?" == "0" ]
then
    cat test_output.txt | sed 's/"timestamp":"....-..-..T..:..:..Z"/"timestamp":"1111-11-11T11:11:11Z"/' > test_responses.txt
    diff test_expected.txt test_responses.txt
    if [ "$?" == "0" ]
    then
        echo T1 OK
    fi
else
    Failure.
fi

$PROG --shell='echo "$METER_JSON"' simulation_shell.txt MWW supercom587 12345678 "" > test_output.txt
if [ "$?" == "0" ]
then
    cat test_output.txt | sed 's/"timestamp":"....-..-..T..:..:..Z"/"timestamp":"1111-11-11T11:11:11Z"/' > test_responses.txt
    echo '{"media":"warm water","meter":"supercom587","name":"MWW","id":"12345678","total_m3":5.548000,"timestamp":"1111-11-11T11:11:11Z"}' > test_expected.txt
    diff test_expected.txt test_responses.txt
    if [ "$?" == "0" ]
    then
        echo SHELL OK
    fi
else
    Failure.
fi

rm -f /tmp/MyTapWater
cat simulation_c1.txt | grep '^{' | grep 76348799 | grep 23 > test_expected.txt
$PROG --meterfiles --robot=json simulation_c1.txt MyTapWater multical21 76348799 ""
cat /tmp/MyTapWater | sed 's/"timestamp":"....-..-..T..:..:..Z"/"timestamp":"1111-11-11T11:11:11Z"/' > test_response.txt
diff test_expected.txt test_response.txt
if [ "$?" == "0" ]
then
    echo Meterfiles OK
    rm /tmp/MyTapWater
fi

rm -rf /tmp/testmeters
mkdir /tmp/testmeters
cat simulation_c1.txt | grep '^{' | grep 76348799 | grep 23 > test_expected.txt
$PROG --meterfiles=/tmp/testmeters --robot=json simulation_c1.txt MyTapWater multical21 76348799 ""
cat /tmp/testmeters/MyTapWater | sed 's/"timestamp":"....-..-..T..:..:..Z"/"timestamp":"1111-11-11T11:11:11Z"/' > test_response.txt
diff test_expected.txt test_response.txt
if [ "$?" == "0" ]
then
    echo Meterfiles dir OK
    rm -rf /tmp/testmeters
fi
