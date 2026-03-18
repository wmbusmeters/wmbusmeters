#!/bin/sh

PROG="$1"
TEST=testoutput
mkdir -p $TEST

TESTNAME="Test templated field as selected field"
TESTRESULT="ERROR"

echo "0;517" > $TEST/test_expected.txt

$PROG --format=fields --selectfields=power_kw,prev_1_month_kwh 8E44496A03270232880D7AED0080052F2F0406050200008410069A1700000413BC6F2E00840406E701000082046C2131841406C20B000082146C2131043B00000000042D0000000002595D09025D240802FD17000084800106050200008280016C213C948001AE2501000000849001069A1700008290016C213C849001AE25000000002F2F2F2F2F2F2F2F2F2F2F2F W c5isf 32022703 NOKEY > $TEST/test_output.txt 2>&1

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
