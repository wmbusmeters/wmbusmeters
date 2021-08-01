#!/bin/sh

PROG="$1"

mkdir -p testoutput
TEST=testoutput

TESTNAME="Test unix timestamp in fields"
TESTRESULT="ERROR"

METERS="Votten      aventieswm  61070071 A004EB23329A477F1DD2D7820B56EB3D"

cat simulations/simulation_unix_timestamp.txt | grep '^{' > $TEST/test_expected.txt

NOW=$(date +%s)
cat simulations/simulation_unix_timestamp.txt | grep '^|' | sed 's/^|//' | sed "s/UT/${NOW}/" > $TEST/test_expected.txt
$PROG --format=fields --selectfields=total_m3,timestamp_ut,name,id simulations/simulation_t1.txt $METERS  > $TEST/test_output.txt 2> $TEST/test_stderr.txt
if [ "$?" = "0" ]
then
    cat $TEST/test_output.txt | sed 's/[0-9][0-9][0-9][0-9]-[0-9][0-9]-[0-9][0-9] [0-9][0-9]:[0-9][0-9].[0-9][0-9]$/1111-11-11 11:11.11/' > $TEST/test_responses.txt
    diff $TEST/test_expected.txt $TEST/test_responses.txt
    if [ "$?" = "0" ]
    then
        echo OK fields: $TESTNAME
        TESTRESULT="OK"
    else
        TESTRESULT="ERROR"
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
