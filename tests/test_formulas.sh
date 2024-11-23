#!/bin/sh

PROG="$1"

rm -rf testoutput
mkdir -p testoutput
TEST=testoutput

TESTNAME="Test cmdline formulas --calculate_... "
TESTRESULT="ERROR"

$PROG --format=json \
      --calculate_sumtemp_c='external_temperature_c+flow_temperature_c' \
      --calculate_addtemp_c='external_temperature_c + 1100 c' \
      --calculate_external_temperature_f='external_temperature_c' \
      23442D2C998734761B168D2087D19EAD217F1779EDA86AB6_710008190000081900007F13 \
      MyTapWater multical21 76348799 "" \
      | jq --sort-keys . > $TEST/test_output.txt

cat <<EOF | jq --sort-keys . > $TEST/test_expected.txt
{"_":"telegram","media":"cold water","meter":"multical21","name":"MyTapWater","id":"76348799","status":"DRY","total_m3":6.408,"target_m3":6.408,"flow_temperature_c":127,"external_temperature_c":19,"current_status":"DRY","time_dry":"22-31 days","time_reversed":"","time_leaking":"","time_bursting":"","sumtemp_c":146,"addtemp_c":1119,"external_temperature_f":66.2,"timestamp":"1111-11-11T11:11:11Z"}
EOF

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

TESTNAME="Test meter config formulas calculate_... "
TESTRESULT="ERROR"

$PROG --useconfig=tests/config13 --overridedevice=simulations/simulation_formulas.txt \
      > $TEST/test_output.txt

cat > $TEST/test_expected.txt <<EOF
6.408;6.463;69
EOF

if [ "$?" = "0" ]
then
    cat $TEST/test_output.txt | sed 's/"timestamp":"....-..-..T..:..:..Z"/"timestamp":"1111-11-11T11:11:11Z"/' > $TEST/test_responses.txt
    diff $TEST/test_expected.txt $TEST/test_responses.txt
    if [ "$?" = "0" ]
    then
        echo "OK: $TESTNAME"
        TESTRESULT="OK"
    fi
fi

if [ "$TESTRESULT" = "ERROR" ]; then echo ERROR: $TESTNAME;  exit 1; fi
