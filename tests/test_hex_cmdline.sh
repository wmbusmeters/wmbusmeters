#!/bin/sh

PROG="$1"
TEST=testoutput
mkdir -p $TEST

TESTNAME="Test hex on commandline"
TESTRESULT="ERROR"

cat > $TEST/test_expected.txt <<EOF
No meters configured. Printing id:s of all telegrams heard!
Received telegram from: 76348799
          manufacturer: (KAM) Kamstrup Energi (0x2c2d)
                  type: Cold water meter (0x16) encrypted
                   ver: 0x1b
                driver: multical21
EOF

$PROG 2A442D2C998734761B168D2091D37CAC21E1D68CDAFFCD3DC452BD802913FF7B1706CA9E355D6C2701CC24 > $TEST/test_output.txt 2>&1

if [ "$?" = "0" ]
then
    diff $TEST/test_expected.txt $TEST/test_output.txt
    if [ "$?" = "0" ]
    then
        echo OK: $TESTNAME
        TESTRESULT="OK"
    else
        if [ "$USE_MELD" = "true" ]
        then
            meld $TEST/test_expected.txt $TEST/test_output.txt
        fi
    fi
else
    echo "ERROR: $TESTNAME"
    echo "wmbusmeters returned error code: $?"
    cat $TEST/test_output.txt
    exit 1
fi

cat > $TEST/test_expected_unsorted.txt <<EOF
{"_":"telegram","media":"cold water","meter":"multical21","name":"MyWater","id":"76348799","status":"DRY","total_m3":6.408,"target_m3":6.408,"flow_temperature_c":127,"external_temperature_c":19,"current_status":"DRY","time_dry":"22-31 days","time_reversed":"","time_leaking":"","time_bursting":"","timestamp":"1111-11-11T11:11:11Z"}
EOF

jq --sort-keys . $TEST/test_expected_unsorted.txt > $TEST/test_expected.txt

TESTNAME="Test hex on commandline with meter"

$PROG --format=json 2A442D2C998734761B168D2091D37CAC21E1D68CDAFFCD3DC452BD802913FF7B1706CA9E355D6C2701CC24 \
      MyWater auto 76348799 28F64A24988064A079AA2C807D6102AE  2> $TEST/test_stderr.txt | jq --sort-keys . > $TEST/test_output.txt

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
        exit 1
    fi
else
    echo "ERROR: $TESTNAME"
    echo "wmbusmeters returned error code: $?"
    cat $TEST/test_output.txt
    cat $TEST/test_stderr.txt
    exit 1
fi

TESTNAME="Test invalid hex on commandline"

cat > $TEST/test_expected.txt <<EOF
Hex string must have an even length of hexadecimal characters.
EOF

$PROG 2A442D2C998734761B168D2091D37CAC21E1D68CDAFFCD3DC452BD802913FF7B1706CA9E355D6C2701CC2 2> $TEST/test_output.txt 1>&2

if [ "$?" = "1" ]
then
    diff $TEST/test_expected.txt $TEST/test_output.txt
    if [ "$?" = "0" ]
    then
        echo OK: $TESTNAME
        TESTRESULT="OK"
    else
        echo "ERROR: $TESTNAME"
        exit 1
    fi
else
    echo "wmbusmeters returned error code 0 but we expected failure!"
    echo "======================="
    cat $TEST/test_output.txt
    echo "======================="
    cat $TEST/test_stderr.txt
fi


TESTNAME="Test hex on stdin"

cat > $TEST/test_expected_unsorted.txt <<EOF
{"_":"telegram","media":"other","meter":"lansenpu","name":"MyCounter","id":"00010206","status":"OK","a_counter":4711,"b_counter":1234,"timestamp":"1111-11-11T11:11:11Z"}
{"_":"telegram","media":"cold water","meter":"multical21","name":"MyWater","id":"76348799","status":"DRY","total_m3":6.408,"target_m3":6.408,"flow_temperature_c":127,"external_temperature_c":19,"current_status":"DRY","time_dry":"22-31 days","time_reversed":"","time_leaking":"","time_bursting":"","timestamp":"1111-11-11T11:11:11Z"}
{"_":"telegram","media":"other","meter":"lansenpu","name":"MyCounter","id":"00010206","status":"OK","a_counter":4711,"b_counter":1234,"timestamp":"1111-11-11T11:11:11Z"}
EOF

jq --sort-keys . $TEST/test_expected_unsorted.txt > $TEST/test_expected.txt

echo 234433300602010014007a8e0000002f2f0efd3a1147000000008e40fd3a3412000000002A442D2C998734761B168D2091D37CAC21E1D68CDAFFCD3DC452BD802913FF7B1706CA9E355D6C2701CC24234433300602010014007a8e0000002f2f0efd3a1147000000008e40fd3a341200000000999 | \
    $PROG --silent --ignoreduplicates=false --format=json stdin:hex MyCounter auto 00010206 NOKEY MyWater auto 76348799 28F64A24988064A079AA2C807D6102AE 2> $TEST/test_stderr.txt | jq --sort-keys . > $TEST/test_output.txt

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
        exit 1
    fi
else
    echo "ERROR: $TESTNAME"
    echo "wmbusmeters returned error code: $?"
    cat $TEST/test_output.txt
    cat $TEST/test_stderr.txt
    exit 1
fi
