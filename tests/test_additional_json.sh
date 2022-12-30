#!/bin/sh

PROG="$1"
TEST=testoutput
mkdir -p $TEST

TESTNAME="Test additional json from cmdline"
TESTRESULT="ERROR"

cat simulations/simulation_additional_json.txt | grep '^{' > $TEST/test_expected.txt
$PROG --format=json --json_floor=5 --json_address="RoodRd 42" --field_city="Stockholm" simulations/simulation_additional_json.txt \
      MyTapWater multical21 76348799 "" \
      > $TEST/test_output.txt 2> $TEST/test_stderr.txt

if [ "$?" = "0" ]
then
    cat $TEST/test_output.txt | sed 's/"timestamp":"....-..-..T..:..:..Z"/"timestamp":"1111-11-11T11:11:11Z"/' > $TEST/test_responses.txt
    diff $TEST/test_expected.txt $TEST/test_responses.txt
    if [ "$?" = "0" ]
    then
        echo "OK: $TESTNAME"
        TESTRESULT="OK"
    else
        if [ "$USE_MELD" = "true" ]
        then
            meld $TEST/test_expected.txt $TEST/test_response.txt
        fi
    fi
fi

if [ "$TESTRESULT" = "ERROR" ]; then echo ERROR: $TESTNAME;  exit 1; fi



TESTNAME="Test additional shell envs from cmdline"
TESTRESULT="ERROR"

$PROG --json_floor=5 --json_house="alfa beta" --listenvs=multical21 2> $TEST/test_stderr.txt | sort > $TEST/test_output.txt

ENVS=$(cat $TEST/test_output.txt | tr '\n' ' ')

cat <<EOF | sort > $TEST/test_expected.txt
METER_JSON
METER_ID
METER_NAME
METER_MEDIA
METER_TYPE
METER_TIMESTAMP
METER_TIMESTAMP_UTC
METER_TIMESTAMP_UT
METER_TIMESTAMP_LT
METER_DEVICE
METER_RSSI_DBM
METER_STATUS
METER_TOTAL_M3
METER_TARGET_M3
METER_FLOW_TEMPERATURE_C
METER_EXTERNAL_TEMPERATURE_C
METER_MIN_EXTERNAL_TEMPERATURE_C
METER_MAX_FLOW_M3H
METER_CURRENT_STATUS
METER_TIME_DRY
METER_TIME_REVERSED
METER_TIME_LEAKING
METER_TIME_BURSTING
EOF

diff $TEST/test_expected.txt $TEST/test_output.txt
if [ "$?" = "0" ]
then
    echo "OK: $TESTNAME"
    TESTRESULT="OK"
else
    if [ "$USE_MELD" = "true" ]
    then
        meld $TEST/test_expected.txt $TEST/test_output.txt
    fi
fi

if [ "$TESTRESULT" = "ERROR" ]; then echo ERROR: $TESTNAME;  exit 1; fi

TESTNAME="Test additional json from wmbusmeters.conf and from meter file"
TESTRESULT="ERROR"

$PROG --debug --useconfig=tests/config6 --overridedevice=simulations/simulation_shell.txt  > $TEST/test_output.txt 2> $TEST/test_stderr.txt

if [ "$?" = "0" ]
then
    INFO=$(cat /tmp/wmbusmeters_meter_additional_json_test | sed 's/"timestamp":"....-..-..T..:..:..Z"/"timestamp":"1111-11-11T11:11:11Z"/')
    EXPECTED=$(echo 'METER =={"media":"warm water","meter":"supercom587","name":"Water","id":"12345678","software_version":"010002","total_m3":5.548,"status":"OK","timestamp":"1111-11-11T11:11:11Z","floor":"5","elevator":"ABC","address":"RoodRd 42","city":"Stockholm"}== ==RoodRd 42== ==Stockholm== ==5== ==ABC==')
    if [ "$INFO" = "$EXPECTED" ]
    then
        echo "OK: $TESTNAME"
        TESTRESULT="OK"
    else
        echo "INFO    =>$INFO<"
        echo "EXPECTED=>$EXPECTED<"
    fi
fi

if [ "$TESTRESULT" = "ERROR" ]; then echo ERROR: $TESTNAME;  exit 1; fi
