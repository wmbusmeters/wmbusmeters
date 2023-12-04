#!/bin/sh

PROG="$1"

mkdir -p testoutput
TEST=testoutput

TESTNAME="Test metershell invocation"
TESTRESULT="ERROR"

$PROG --metershell='echo "METER SETUP FOR: $METER_ID"' --shell='echo "METER DATA FOR $METER_ID"' simulations/simulation_metershell.txt MWW supercom587 12345678 "" \
      2> $TEST/test_stderr.txt > $TEST/test_output.txt
if [ "$?" = "0" ]
then
    cat $TEST/test_output.txt | sed 's/....-..-.. ..:..:..$/1111-11-11 11:11:11/g' > $TEST/test_responses.txt

    # Test that metershell is only ran once, but shell for every telegram
    echo "METER SETUP FOR: 12345678" > $TEST/test_expected.txt
    echo "METER DATA FOR 12345678" >> $TEST/test_expected.txt
    echo "METER DATA FOR 12345678" >> $TEST/test_expected.txt

    diff $TEST/test_expected.txt $TEST/test_responses.txt
    if [ "$?" = "0" ]
    then
        echo OK: $TESTNAME
        TESTRESULT="OK"
    fi
fi

if [ "$TESTRESULT" = "ERROR" ]
then
    echo ERROR: $TESTNAME
    exit 1
fi
