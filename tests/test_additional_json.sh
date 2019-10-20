#!/bin/bash

PROG="$1"
TEST=testoutput
mkdir -p $TEST

cat simulations/simulation_additional_json.txt | grep '^{' > $TEST/test_expected.txt
$PROG --format=json --json_floor=5 --json_address="RoodRd 42" simulations/simulation_additional_json.txt \
      MyTapWater multical21 76348799 "" \
      > $TEST/test_output.txt

if [ "$?" == "0" ]
then
    cat $TEST/test_output.txt | sed 's/"timestamp":"....-..-..T..:..:..Z"/"timestamp":"1111-11-11T11:11:11Z"/' > $TEST/test_responses.txt
    diff $TEST/test_expected.txt $TEST/test_responses.txt
    if [ "$?" == "0" ]
    then
        echo Additional json from cmdline OK
    else
        echo Failure!
        exit 1
    fi
else
    echo Failure.
    exit 1
fi


$PROG --format=json --json_floor=5 --json_house="alfa beta" --shellenvs --listento=c1 simulations/simulation_additional_json.txt \
      Vatten multical21 76348799 "" | grep METER_  > $TEST/test_output.txt

ENVS=$(cat $TEST/test_output.txt | tr '\n' ' ')

cat > $TEST/test_expected.txt <<EOF
METER_JSON
METER_TYPE
METER_ID
METER_TOTAL_M3
METER_TARGET_M3
METER_MAX_FLOW_M3H
METER_FLOW_TEMPERATURE_C
METER_EXTERNAL_TEMPERATURE_C
METER_CURRENT_STATUS
METER_TIME_DRY
METER_TIME_REVERSED
METER_TIME_LEAKING
METER_TIME_BURSTING
METER_TIMESTAMP
METER_floor
METER_house
EOF

diff $TEST/test_expected.txt $TEST/test_output.txt
if [ "$?" == "0" ]
then
    echo Additional json in shell envs OK
else
    echo Failure!
    exit 1
fi

$PROG --useconfig=tests/config6 > $TEST/test_output.txt

if [ "$?" == "0" ]
then
    INFO=$(cat /tmp/wmbusmeters_meter_additional_json_test | sed 's/"timestamp":"....-..-..T..:..:..Z"/"timestamp":"1111-11-11T11:11:11Z"/')
    EXPECTED=$(echo 'METER =={"media":"warm water","meter":"supercom587","name":"Water","id":"12345678","total_m3":5.548,"timestamp":"1111-11-11T11:11:11Z","floor":"5","address":"RoodRd 42"}== ==RoodRd 42== ==5==')
    if [ "$INFO" = "$EXPECTED" ]
    then
        echo Additional json from config  OK
    else
        echo $INFO
        echo $EXPECTED
        exit 1
    fi
else
    echo Failure.
    exit 1
fi
