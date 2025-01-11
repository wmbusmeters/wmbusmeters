#!/bin/sh

PROG="$1"

rm -rf testoutput
mkdir -p testoutput

TEST=testoutput
TESTNAME=test_jh_unit

$PROG --format=json --calculate_gurka_jh=power_kw 3e44B409418012051a0d8c20f17a9d000020046d0d3126310c0a481878330c13098405000B3B0000000B300100000a5929360a5d94230f6402000000000000 Watter drivers/src/hydrocalm4.xmq 05128041 NOKEY | jq --sort-keys . > $TEST/test_output.txt 2> $TEST/test_stderr.txt

cat <<EOF > $TEST/test_expected.txt
{
  "_": "telegram",
  "device_datetime": "2025-01-06 17:13",
  "gurka_jh": 1,
  "id": "05128041",
  "media": "heat/cooling load",
  "meter": "hydrocalm4",
  "name": "Watter",
  "power_kw": 0,
  "return_temperature_c": 23.94,
  "status": "OK",
  "supply_temperature_c": 36.29,
  "timestamp": "1111-11-11T11:11:11Z",
  "total_heating_kwh": 938.384667,
  "total_heating_m3": 58.409,
  "volume_flow_m3h": 0
}
EOF

if [ "$?" = "0" ]
then
    cat $TEST/test_output.txt | sed 's/"timestamp": "....-..-..T..:..:..Z"/"timestamp": "1111-11-11T11:11:11Z"/' > $TEST/test_responses.txt
    diff $TEST/test_expected.txt $TEST/test_responses.txt
    if [ "$?" = "0" ]
    then
        echo OK: $TESTNAME
        TESTRESULT="OK"
    else
        if [ "$USE_MELD" = "true" ]
        then
            meld $TEST/test_expected.txt $TEST/test_responses.txt
        fi
    fi
else
    echo "wmbusmeters returned error code: $?"
    cat $TEST/test_output.txt
    cat $TEST/test_stderr.txt
fi

if [ "$TESTRESULT" = "ERROR" ]
then
    echo ERROR: $TESTNAME
    exit 1
fi
