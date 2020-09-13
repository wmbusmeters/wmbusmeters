#!/bin/sh

PROG="$1"

mkdir -p testoutput
TEST=testoutput

TESTNAME="Test naughty non-compliant apator162 meters"
TESTRESULT="ERROR"

METERS="Wasser      apator162   20202020 NOKEY
      MyTapWatera apator162   21202020 NOKEY
      MyTapWaterb apator162   22202020 NOKEY
      MyTapWaterc apator162   23202020 NOKEY
      MyTapWaterd apator162   24202020 NOKEY
      MyTapWatere apator162   25202020 NOKEY
      MyTapWatere apator162   26202020 NOKEY
      MyTapWatere apator162   27202020 NOKEY"

cat simulations/simulation_apas.txt | grep '^{' > $TEST/test_expected.txt
$PROG --format=json simulations/simulation_apas.txt $METERS  > $TEST/test_output.txt 2> $TEST/test_stderr.txt
if [ "$?" = "0" ]
then
    cat $TEST/test_output.txt | sed 's/"timestamp":"....-..-..T..:..:..Z"/"timestamp":"1111-11-11T11:11:11Z"/' > $TEST/test_responses.txt
    diff $TEST/test_expected.txt $TEST/test_responses.txt
    if [ "$?" = "0" ]
    then
        echo OK json: $TESTNAME
        TESTRESULT="OK"
    fi
fi

cat simulations/simulation_apas.txt | grep '^|' | sed 's/^|//' > $TEST/test_expected.txt
$PROG --format=fields simulations/simulation_apas.txt $METERS  > $TEST/test_output.txt 2> $TEST/test_stderr.txt
if [ "$?" = "0" ]
then
    cat $TEST/test_output.txt | sed 's/[0-9][0-9][0-9][0-9]-[0-9][0-9]-[0-9][0-9] [0-9][0-9]:[0-9][0-9].[0-9][0-9]$/1111-11-11 11:11.11/' > $TEST/test_responses.txt
    diff $TEST/test_expected.txt $TEST/test_responses.txt
    if [ "$?" = "0" ]
    then
        echo OK fields: $TESTNAME
        TESTRESULT="OK"
    fi
fi


if [ "$TESTRESULT" = "ERROR" ]
then
    echo ERROR: $TESTNAME
    exit 1
fi
