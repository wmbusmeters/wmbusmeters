#!/bin/sh

. tests/include.sh

PROG="$1"
TEST=testoutput
mkdir -p $TEST

# bar.sh: iperl (SEN,68,07) telegram whose TPL status word is 0x86.
#   0x86 = ERROR(bit3) + POWER_LOW(bit1) + 0x80 (vendor/unmapped hi bit).
#
# Checks three things the iperl driver must do correctly:
#   1. top-level "status" string:  "ERROR POWER_LOW TPL_STS_128"
#   2. structured.status_flags:  ERROR=true, POWER_LOW=true, rest false
#   3. structured.status_lookups_unknowns: {"TPL_STS":128}  (unmapped 0x80 vendor bit)
#   4. details.fields.status: quantity "Flags" + full 6-name flags array
#
TESTNAME="Test bar.sh iperl: TPL status flags + lookups_unknowns (ERROR POWER_LOW, TPL_STS_128)"
TESTRESULT="ERROR"

cat > $TEST/test_expected_unsorted.txt <<EOF
{"_":"telegram","driver":"iperl","id":"33225544","max_flow_m3h":0,"media":"water","name":"","status":"ERROR POWER_LOW TPL_STS_128","structured":{"status_flags":{"ALARM":false,"BUSY":false,"ERROR":true,"PERMANENT_ERROR":false,"POWER_LOW":true,"TEMPORARY_ERROR":false},"status_lookups_unknowns":{"TPL_STS":128}},"timestamp":"1111-11-11T11:11:11Z","total_m3":123.529,"details":{"fields":{"max_flow_m3h":{"change":"Sample","info":"The maximum water flow recorded during previous period.","quantity":"Flow","unit":"m3h"},"status":{"flags":["ALARM","BUSY","ERROR","PERMANENT_ERROR","POWER_LOW","TEMPORARY_ERROR"],"info":"Meter status including the tpl status.","quantity":"Flags"},"total_m3":{"change":"Increasing","info":"The total water consumption.","quantity":"Volume","unit":"m3"}},"flags":{"ALARM":"Abnormal condition/alarm. (tpl sts)","BUSY":"Meter busy. (tpl sts)","ERROR":"Meter error; details via error reporting. (tpl sts)","PERMANENT_ERROR":"Permanent meter error. (tpl sts)","POWER_LOW":"Power low. (tpl sts)","TEMPORARY_ERROR":"Temporary meter error. (tpl sts)"},"mvt":"SEN,68,07"}}
EOF

jq --sort-keys . $TEST/test_expected_unsorted.txt > $TEST/test_expected.txt

# --analyze dumps the decode trace to stdout before the JSON object;
# sed -n '/^{/,$p' keeps only from the first "{" to end-of-stream.
$PROG --addtelegramdetails --addtelegramstructured --analyze \
      1844AE4C4455223368077A55860000_041389E20100023B0000 2> $TEST/test_stderr.txt \
      | sed -n '/^{/,$p' | jq --sort-keys . > $TEST/test_output.txt

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
        if [ "$USE_MELD" = "true" ]
        then
            meld $TEST/test_expected.txt $TEST/test_responses.txt
        fi
        exit 1
    fi
else
    printERROR "$TESTNAME"
    echo "wmbusmeters returned error code: $?"
    cat $TEST/test_output.txt
    cat $TEST/test_stderr.txt
    exit 1
fi
