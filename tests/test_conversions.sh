#!/bin/sh

PROG="$1"

rm -rf testoutput
mkdir -p testoutput
TEST=testoutput

TESTNAME="Test conversions of units using calculations"
TESTRESULT="ERROR"

cat simulations/simulation_conversionsadded.txt | grep '^{' | jq --sort-keys . > $TEST/test_expected.txt
$PROG --format=json --calculate_total_gj=total_kwh \
      --calculate_current_gj=current_kwh \
      --calculate_previous_gj=previous_kwh \
      --calculate_external_temperature_f=external_temperature_c \
      --calculate_flow_temperature_f=flow_temperature_c \
      --calculate_target_l=target_m3 \
      --calculate_total_l=total_m3 \
      simulations/simulation_conversionsadded.txt \
      Hettan vario451 58234965 "" \
      MyTapWater multical21 76348799 "" \
      2> $TEST/test_stderr.txt \
    | jq --sort-keys . > $TEST/test_output.txt

if [ "$?" = "0" ]
then
    cat $TEST/test_output.txt | sed 's/"timestamp": "....-..-..T..:..:..Z"/"timestamp": "1111-11-11T11:11:11Z"/' > $TEST/test_responses.txt
    diff $TEST/test_expected.txt $TEST/test_responses.txt
    if [ "$?" = "0" ]
    then
        echo "OK: $TESTNAME"
        TESTRESULT="OK"
    else
        if [ "$USE_MELD" = "true" ]
        then
            meld $TEST/test_expected.txt $TEST/test_responses.txt
        fi
    fi
fi

if [ "$TESTRESULT" = "ERROR" ]; then echo ERROR: $TESTNAME;  exit 1; fi
