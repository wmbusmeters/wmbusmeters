#!/bin/sh

. tests/include.sh

PROG="$1"
TEST=testoutput
mkdir -p $TEST

TESTNAME="Test addtelegramstructure=true which expands status into an object"
TESTRESULT="ERROR"

cat > $TEST/test_expected_unsorted.txt <<EOF
{"_":"telegram","media":"cold water","driver":"kamwater","name":"MyTapWater","id":"76348799","min_external_temperature_last_month_c":19,"min_flow_temperature_last_month_c":127,"target_m3":6.408,"total_m3":6.408,"current_status":"DRY","status": "DRY","structured":{"status_flags":{"DRY":true,"REVERSE":false,"LEAK":false,"BURST":false,"BUSY":false,"ERROR":false,"ALARM":false,"POWER_LOW":false,"PERMANENT_ERROR":false,"TEMPORARY_ERROR":false}},"time_bursting":"","time_dry":"22-31 days","time_leaking":"","time_reversed":"","timestamp":"1111-11-11T11:11:11Z"}
EOF

jq --sort-keys . $TEST/test_expected_unsorted.txt > $TEST/test_expected.txt

$PROG --format=json --addtelegramstructured 2A442D2C998734761B168D2091D37CAC21576C78_02FF207100041308190000441308190000615B7F616713 \
      MyTapWater multical21 76348799 NOKEY 2> $TEST/test_stderr.txt | jq --sort-keys . > $TEST/test_output.txt

if [ "$?" = "0" ]
then
    cat $TEST/test_output.txt | sed 's/"timestamp": "....-..-..T..:..:..Z"/"timestamp": "1111-11-11T11:11:11Z"/' > $TEST/test_responses.txt
    diff $TEST/test_expected.txt $TEST/test_responses.txt
    if [ "$?" = "0" ]
    then
        printOK "$TESTNAME"
        TESTRESULT="OK"
    else
        printERROR "$TESTNAME"
        exit 1
    fi
else
    printERROR "$TESTNAME"
    echo "wmbusmeters returned error code: $?"
    cat $TEST/test_output.txt
    cat $TEST/test_stderr.txt
    exit 1
fi

TESTNAME="Test format=json still prints status as a plain string"
TESTRESULT="ERROR"

cat > $TEST/test_expected_unsorted.txt <<EOF
{"_":"telegram","media":"cold water","driver":"kamwater","name":"MyTapWater","id":"76348799","min_external_temperature_last_month_c":19,"min_flow_temperature_last_month_c":127,"target_m3":6.408,"total_m3":6.408,"current_status":"DRY","status":"DRY","time_bursting":"","time_dry":"22-31 days","time_leaking":"","time_reversed":"","timestamp":"1111-11-11T11:11:11Z"}
EOF

jq --sort-keys . $TEST/test_expected_unsorted.txt > $TEST/test_expected.txt

$PROG --format=json 2A442D2C998734761B168D2091D37CAC21576C78_02FF207100041308190000441308190000615B7F616713 \
      MyTapWater multical21 76348799 NOKEY 2> $TEST/test_stderr.txt | jq --sort-keys . > $TEST/test_output.txt

if [ "$?" = "0" ]
then
    cat $TEST/test_output.txt | sed 's/"timestamp": "....-..-..T..:..:..Z"/"timestamp": "1111-11-11T11:11:11Z"/' > $TEST/test_responses.txt
    diff $TEST/test_expected.txt $TEST/test_responses.txt
    if [ "$?" = "0" ]
    then
        printOK "$TESTNAME"
        TESTRESULT="OK"
    else
        printERROR "$TESTNAME"
        exit 1
    fi
else
    printERROR "$TESTNAME"
    echo "wmbusmeters returned error code: $?"
    cat $TEST/test_output.txt
    cat $TEST/test_stderr.txt
    exit 1
fi
