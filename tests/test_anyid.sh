#!/bin/sh

PROG="$1"

mkdir -p testoutput
TEST=testoutput

TESTNAME="Test ANYID"
TESTRESULT="ERROR"

cat <<EOF | jq --sort-keys . > $TEST/test_expected.txt
{"media":"cold water","meter":"multical21","name":"Vatten","id":"76348799","status":"DRY","total_m3":6.408,"target_m3":6.408,"flow_temperature_c":127,"external_temperature_c":19,"current_status":"DRY","time_dry":"22-31 days","time_reversed":"","time_leaking":"","time_bursting":"","timestamp":"1111-11-11T11:11:11Z"}
EOF

$PROG --format=json 2A442D2C998734761B168D2091D37CAC21576C7802FF207100041308190000441308190000615B7F616713 \
      Vatten multical21 ANYID NOKEY 2>&1 | jq --sort-keys .  > $TEST/test_output.txt
if [ "$?" = "0" ]
then
    cat $TEST/test_output.txt | sed 's/"timestamp": "....-..-..T..:..:..Z"/"timestamp": "1111-11-11T11:11:11Z"/' > $TEST/test_responses.txt
    diff $TEST/test_expected.txt $TEST/test_responses.txt
    if [ "$?" = "0" ]
    then
        echo OK: $TESTNAME
        TESTRESULT="OK"
    else
        TESTRESULT="ERROR"
    fi
fi

# Now test that anyid and auto does not work...
$PROG --verbose 2A442D2C998734761B168D2091D37CAC21576C7802FF207100041308190000441308190000615B7F616713 \
      Vatten auto ANYID NOKEY  > $TEST/test_output.txt 2>&1

if ! grep -o  "ignoring telegram from 76348799" $TEST/test_output.txt > /dev/null
then
    echo "Expected telegram to be ignored with auto + ANYID is used!"
    TESTRESULT="ERROR"
else
    echo "OK: Ignoring auto + ANYID"
fi

if [ "$TESTRESULT" = "ERROR" ]
then
    echo ERROR: $TESTNAME
    exit 1
fi
