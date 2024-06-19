#!/bin/sh

PROG="$1"

mkdir -p testoutput
TEST=testoutput

TESTNAME="Test ANYID with explicit driver"
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

TESTNAME="Test ANYID with auto driver"

# Now test that anyid and auto >does< work...
$PROG --format=json 2A442D2C998734761B168D2091D37CAC21576C7802FF207100041308190000441308190000615B7F616713 \
      Vatten auto ANYID NOKEY 2>&1 | jq --sort-keys .  > $TEST/test_output.txt

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

TESTNAME="Test ANYID with auto but no driver found!"

cat <<EOF > $TEST/test_expected.txt
(meter) Vatten: meter detection could not find driver for id: 76348799 mfct: (KIM) Unknown (0x2d2d) type: Cold water meter (0x16) ver: 0x1b
(meter) please consider opening an issue at https://github.com/wmbusmeters/wmbusmeters/
(meter) to add support for this unknown mfct,media,version combination
{"media":"cold water","meter":"auto","name":"Vatten","id":"76348799","timestamp":"1111-11-11T11:11:11Z"}
EOF

# Now test that anyid and auto >does< work...
$PROG --format=json 2A442D2D998734761B168D2091D37CAC21576C7802FF207100041308190000441308190000615B7F616713 \
      Vatten auto ANYID NOKEY > $TEST/test_output.txt 2>&1

if [ "$?" = "0" ]
then
    cat $TEST/test_output.txt | sed 's/"timestamp":"....-..-..T..:..:..Z"/"timestamp":"1111-11-11T11:11:11Z"/' > $TEST/test_responses.txt
    diff $TEST/test_expected.txt $TEST/test_responses.txt
    if [ "$?" = "0" ]
    then
        echo OK: $TESTNAME
        TESTRESULT="OK"
    else
        TESTRESULT="ERROR"
    fi
fi

if [ "$TESTRESULT" = "ERROR" ]
then
    echo ERROR: $TESTNAME
    exit 1
fi
