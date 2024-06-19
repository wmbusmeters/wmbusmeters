#!/bin/sh

PROG="$1"

mkdir -p testoutput

TEST=testoutput

TESTNAME="Test mbus"
TESTRESULT="ERROR"

cat simulations/simulation_mbus.txt | grep '^{' | jq --sort-keys . > $TEST/test_expected.txt
$PROG --format=json simulations/simulation_mbus.txt \
      MyUltra ultraheat 70444600 NOKEY \
      MySenso sensostar 10484075 NOKEY \
      MyKamHeat kamheat 77447744 NOKEY \
      2> $TEST/test_stderr.txt | jq --sort-keys . > $TEST/test_output.txt

if [ "$?" = "0" ]
then
    cat $TEST/test_output.txt | sed 's/"timestamp": "....-..-..T..:..:..Z"/"timestamp": "1111-11-11T11:11:11Z"/' > $TEST/test_responses.txt
    diff $TEST/test_expected.txt $TEST/test_responses.txt
    if [ "$?" = "0" ]
    then
        echo OK: $TESTNAME
        TESTRESULT="OK"
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
