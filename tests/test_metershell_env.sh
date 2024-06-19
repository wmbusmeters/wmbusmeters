#!/bin/bash

if [ "$(uname)" = "Darwin" ]
then
    # Skip this test.
    exit 0
fi

PROG="$1"

mkdir -p testoutput
TEST=testoutput

TESTNAME="Test metershell environment"
TESTRESULT="ERROR"

export MY_ENV_TEST="hello world"

$PROG --metershell='echo MY_ENV_TEST="$MY_ENV_TEST"' --shell='echo -n ""' simulations/simulation_metershell.txt MWW supercom587 12345678 "" > $TEST/test_output.txt 2> $TEST/test_stderr.txt
if [ "$?" = "0" ]
then
    echo 'MY_ENV_TEST=hello world' > $TEST/test_expected.txt
    diff $TEST/test_expected.txt $TEST/test_output.txt
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
