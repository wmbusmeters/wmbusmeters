#!/bin/sh

PROG="$1"
TEST=testoutput
mkdir -p $TEST

TESTNAME="Test json on stdin with single telegram"
TESTRESULT="ERROR"

cat > $TEST/test_expected_unsorted.txt <<EOF
{"_":"telegram","media":"cold water","meter":"multical21","name":"","id":"76348799","status":"DRY","total_m3":6.408,"target_m3":6.408,"flow_temperature_c":127,"external_temperature_c":19,"current_status":"DRY","time_dry":"22-31 days","time_reversed":"","time_leaking":"","time_bursting":"","timestamp":"1111-11-11T11:11:11Z"}
EOF

jq --sort-keys . $TEST/test_expected_unsorted.txt > $TEST/test_expected.txt

echo '{"_":"decode", "telegram": "2A442D2C998734761B168D2091D37CAC21E1D68CDAFFCD3DC452BD802913FF7B1706CA9E355D6C2701CC24", "key": "28F64A24988064A079AA2C807D6102AE"}' | \
    $PROG --verbose --format=json stdin:jsonl 2> $TEST/test_stderr.txt | jq --sort-keys . > $TEST/test_output.txt

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

TESTNAME="Test json on stdin with multiple telegrams and different keys"

cat > $TEST/test_expected_unsorted.txt <<EOF
{"_":"telegram","media":"cold water","meter":"multical21","name":"","id":"76348799","status":"DRY","total_m3":6.408,"target_m3":6.408,"flow_temperature_c":127,"external_temperature_c":19,"current_status":"DRY","time_dry":"22-31 days","time_reversed":"","time_leaking":"","time_bursting":"","timestamp":"1111-11-11T11:11:11Z"}
{"_":"telegram","media":"other","meter":"lansenpu","name":"","id":"00010206","status":"OK","a_counter":4711,"b_counter":1234,"timestamp":"1111-11-11T11:11:11Z"}
EOF

jq --sort-keys . $TEST/test_expected_unsorted.txt > $TEST/test_expected.txt

cat > $TEST/test_input.txt <<EOF
{"_":"decode", "telegram": "2A442D2C998734761B168D2091D37CAC21E1D68CDAFFCD3DC452BD802913FF7B1706CA9E355D6C2701CC24", "key": "28F64A24988064A079AA2C807D6102AE"}
{"_":"decode", "telegram": "234433300602010014007a8e0000002f2f0efd3a1147000000008e40fd3a341200000000", "key": "NOKEY"}
EOF

cat $TEST/test_input.txt | \
    $PROG --silent --format=json stdin:jsonl 2> $TEST/test_stderr.txt | jq --sort-keys . > $TEST/test_output.txt

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


TESTNAME="Test json on stdin with wrong key (decryption error)"

cat > $TEST/test_expected_unsorted.txt <<EOF
{"_":"telegram","media":"cold water","meter":"multical21","name":"","id":"76348799","error":"decryption failed, please check key","telegram":"2A442D2C998734761B168D2091D37CAC21E1D68CDAFFCD3DC452BD802913FF7B1706CA9E355D6C2701CC24","timestamp":"1111-11-11T11:11:11Z"}
EOF

jq --sort-keys . $TEST/test_expected_unsorted.txt > $TEST/test_expected.txt

echo '{"_":"decode", "telegram": "2A442D2C998734761B168D2091D37CAC21E1D68CDAFFCD3DC452BD802913FF7B1706CA9E355D6C2701CC24", "key": "00000000000000000000000000000000"}' | \
    $PROG --silent --format=json stdin:jsonl 2> $TEST/test_stderr.txt | jq --sort-keys . > $TEST/test_output.txt

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


TESTNAME="Test json on stdin with explicit driver"

cat > $TEST/test_expected_unsorted.txt <<EOF
{"_":"telegram","media":"cold water","meter":"multical21","name":"","id":"76348799","status":"DRY","total_m3":6.408,"target_m3":6.408,"flow_temperature_c":127,"external_temperature_c":19,"current_status":"DRY","time_dry":"22-31 days","time_reversed":"","time_leaking":"","time_bursting":"","timestamp":"1111-11-11T11:11:11Z"}
EOF

jq --sort-keys . $TEST/test_expected_unsorted.txt > $TEST/test_expected.txt

echo '{"_":"decode", "telegram": "2A442D2C998734761B168D2091D37CAC21E1D68CDAFFCD3DC452BD802913FF7B1706CA9E355D6C2701CC24", "key": "28F64A24988064A079AA2C807D6102AE", "driver": "multical21"}' | \
    $PROG --silent --format=json stdin:jsonl 2> $TEST/test_stderr.txt | jq --sort-keys . > $TEST/test_output.txt

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


TESTNAME="Test json on stdin with partial decode warning"

cat > $TEST/test_expected_unsorted.txt <<EOF
{"_":"telegram","media":"heat cost allocation","meter":"hydroclima","name":"","id":"68036198","current_consumption_hca":0,"average_ambient_temperature_c":18.66,"max_ambient_temperature_c":47.51,"average_ambient_temperature_last_month_c":15.78,"average_heater_temperature_last_month_c":17.47,"warning":"telegram only partially decoded (26 of 51 bytes)","telegram":"2e44b0099861036853087a000020002F2F036E0000000F100043106A7D2C4A078F12202CB1242A06D3062100210000","timestamp":"1111-11-11T11:11:11Z"}
EOF

jq --sort-keys . $TEST/test_expected_unsorted.txt > $TEST/test_expected.txt

echo '{"_":"decode", "telegram": "2e44b0099861036853087a000020002F2F036E0000000F100043106A7D2C4A078F12202CB1242A06D3062100210000", "key": "NOKEY"}' | \
    $PROG --silent --format=json stdin:jsonl 2> $TEST/test_stderr.txt | jq --sort-keys . > $TEST/test_output.txt

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


TESTNAME="Test json on stdin with MBUS telegram (auto-detected)"

cat > $TEST/test_expected_unsorted.txt <<EXPECTED
{"_":"telegram","media":"room sensor","meter":"piigth","name":"","id":"10000284","average_temperature_1h_c":23.52,"average_temperature_24h_c":22.79,"relative_humidity_rh":32.8,"relative_humidity_1h_rh":32.5,"relative_humidity_24h_rh":33.4,"temperature_c":23.02,"fabrication_no":"10000284","software_version":"0021","status":"OK","warning":"telegram only partially decoded (18 of 19 bytes)","telegram":"68383868080072840200102941011B0D0000000265FE0842653009820165E70802FB1A480142FB1A45018201FB1A4E010C788402001002FD0F21000F0316","timestamp":"1111-11-11T11:11:11Z"}
EXPECTED

jq --sort-keys . $TEST/test_expected_unsorted.txt > $TEST/test_expected.txt

echo '{"_":"decode", "telegram": "68383868080072840200102941011B0D0000000265FE0842653009820165E70802FB1A480142FB1A45018201FB1A4E010C788402001002FD0F21000F0316", "key": "NOKEY"}' | \
    $PROG --format=json stdin:jsonl 2> $TEST/test_stderr.txt | jq --sort-keys . > $TEST/test_output.txt

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


TESTNAME="Test xml on stdin"

TOTAL_M3=$(echo '<decode><telegram>1844AE4C4455223368077A55000000041389E20100023B0000</telegram></decode>' | $PROG --format=json stdin:jsonl 2> $TEST/test_stderr.txt | jq .total_m3)

if [ "$?" = "0" ]
then
    cat $TEST/test_output.txt | sed 's/"timestamp": "....-..-..T..:..:..Z"/"timestamp": "1111-11-11T11:11:11Z"/' > $TEST/test_responses.txt
    diff $TEST/test_expected.txt $TEST/test_responses.txt
    if [ "$TOTAL_M3" = "123.529" ]
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

TESTNAME="Test xmq on stdin"

TOTAL_M3=$(echo 'decode{telegram=1844AE4C4455223368077A55000000041389E20100023B0000}' | $PROG --format=json stdin:jsonl 2> $TEST/test_stderr.txt | jq .total_m3)

if [ "$?" = "0" ]
then
    cat $TEST/test_output.txt | sed 's/"timestamp": "....-..-..T..:..:..Z"/"timestamp": "1111-11-11T11:11:11Z"/' > $TEST/test_responses.txt
    diff $TEST/test_expected.txt $TEST/test_responses.txt
    if [ "$TOTAL_M3" = "123.529" ]
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
