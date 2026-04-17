#!/bin/sh

PROG="$1"
TEST=testoutput
mkdir -p $TEST

TESTNAME="Test json from serial/command with single telegram"
TESTRESULT="ERROR"

cat > $TEST/test_expected_unsorted.txt <<EOF
{"_":"telegram","media":"cold water","meter":"kamwater","name":"","id":"76348799","min_external_temperature_last_month_c":19,"flow_temperature_c":127,"target_m3":6.408,"total_m3":6.408,"current_status":"DRY","status":"DRY","time_bursting":"","time_dry":"22-31 days","time_leaking":"","time_reversed":"","timestamp":"1111-11-11T11:11:11Z"}
EOF

jq --sort-keys . $TEST/test_expected_unsorted.txt > $TEST/test_expected.txt

echo '{"_":"decode", "telegram": "2A442D2C998734761B168D2091D37CAC21E1D68CDAFFCD3DC452BD802913FF7B1706CA9E355D6C2701CC24", "key": "28F64A24988064A079AA2C807D6102AE"}' > $TEST/test_input.jsonl

$PROG --verbose --format=json "jsonl:t1:CMD(cat $TEST/test_input.jsonl)" 2> $TEST/test_stderr.txt | jq --sort-keys . > $TEST/test_output.txt

if [ "$?" = "0" ]
then
    cat $TEST/test_output.txt | sed 's/"timestamp": "....-..-..T..:..:..Z"/"timestamp": "1111-11-11T11:11:11Z"/' > $TEST/test_responses.txt
    diff $TEST/test_expected.txt $TEST/test_responses.txt
    if [ "$?" = "0" ]
    then
        echo OK: $TESTNAME
        TESTRESULT="OK"
    else
        echo "ERROR: $TESTNAME"
        echo "Expected:"
        cat $TEST/test_expected.txt
        echo "Got:"
        cat $TEST/test_responses.txt
        exit 1
    fi
else
    echo "ERROR: $TESTNAME"
    echo "wmbusmeters returned error code: $?"
    cat $TEST/test_output.txt
    cat $TEST/test_stderr.txt
    exit 1
fi

TESTNAME="Test json on serial/command with multiple telegrams and different keys"

cat > $TEST/test_expected_unsorted.txt <<EOF
{"_":"telegram","media":"cold water","meter":"kamwater","name":"","id":"76348799","min_external_temperature_last_month_c":19,"flow_temperature_c":127,"target_m3":6.408,"total_m3":6.408,"current_status":"DRY","status":"DRY","time_bursting":"","time_dry":"22-31 days","time_leaking":"","time_reversed":"","timestamp":"1111-11-11T11:11:11Z"}
{"_":"telegram","media":"other","meter":"lansenpu","name":"","id":"00010206","status":"OK","a_counter":4711,"b_counter":1234,"timestamp":"1111-11-11T11:11:11Z"}
EOF

jq --sort-keys . $TEST/test_expected_unsorted.txt > $TEST/test_expected.txt

cat > $TEST/test_input.txt <<EOF
{"_":"decode", "telegram": "2A442D2C998734761B168D2091D37CAC21E1D68CDAFFCD3DC452BD802913FF7B1706CA9E355D6C2701CC24", "key": "28F64A24988064A079AA2C807D6102AE"}
{"_":"decode", "telegram": "234433300602010014007a8e0000002f2f0efd3a1147000000008e40fd3a341200000000", "key": "NOKEY"}
EOF

$PROG --silent --format=json "jsonl:t1:CMD(cat $TEST/test_input.txt)" 2> $TEST/test_stderr.txt | jq --sort-keys . > $TEST/test_output.txt

if [ "$?" = "0" ]
then
    cat $TEST/test_output.txt | sed 's/"timestamp": "....-..-..T..:..:..Z"/"timestamp": "1111-11-11T11:11:11Z"/' > $TEST/test_responses.txt
    diff $TEST/test_expected.txt $TEST/test_responses.txt
    if [ "$?" = "0" ]
    then
        echo OK: $TESTNAME
        TESTRESULT="OK"
    else
        echo "ERROR: $TESTNAME"
        echo "Expected:"
        cat $TEST/test_expected.txt
        echo "Got:"
        cat $TEST/test_responses.txt
        exit 1
    fi
else
    echo "ERROR: $TESTNAME"
    echo "wmbusmeters returned error code: $?"
    cat $TEST/test_output.txt
    cat $TEST/test_stderr.txt
    exit 1
fi

TESTNAME="Test json on serial/command with wrong key (decryption error)"

cat > $TEST/test_expected_unsorted.txt <<EOF
{"_":"telegram","media":"cold water","meter":"kamwater","name":"","id":"76348799","error":"decryption failed, please check key","telegram":"2A442D2C998734761B168D2091D37CAC21E1D68CDAFFCD3DC452BD802913FF7B1706CA9E355D6C2701CC24","timestamp":"1111-11-11T11:11:11Z"}
EOF

jq --sort-keys . $TEST/test_expected_unsorted.txt > $TEST/test_expected.txt

echo '{"_":"decode", "telegram": "2A442D2C998734761B168D2091D37CAC21E1D68CDAFFCD3DC452BD802913FF7B1706CA9E355D6C2701CC24", "key": "00000000000000000000000000000000"}' > $TEST/test_input.txt

$PROG --silent --format=json "jsonl:t1:CMD(cat $TEST/test_input.txt)" 2> $TEST/test_stderr.txt | jq --sort-keys . > $TEST/test_output.txt

if [ "$?" = "0" ]
then
    cat $TEST/test_output.txt | sed 's/"timestamp": "....-..-..T..:..:..Z"/"timestamp": "1111-11-11T11:11:11Z"/' > $TEST/test_responses.txt
    diff $TEST/test_expected.txt $TEST/test_responses.txt
    if [ "$?" = "0" ]
    then
        echo OK: $TESTNAME
        TESTRESULT="OK"
    else
        echo "ERROR: $TESTNAME"
        echo "Expected:"
        cat $TEST/test_expected.txt
        echo "Got:"
        cat $TEST/test_responses.txt
        exit 1
    fi
else
    echo "ERROR: $TESTNAME"
    echo "wmbusmeters returned error code: $?"
    cat $TEST/test_output.txt
    cat $TEST/test_stderr.txt
    exit 1
fi
