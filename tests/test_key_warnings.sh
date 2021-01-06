#!/bin/sh

PROG="$1"

mkdir -p testoutput
TEST=testoutput

TESTNAME="Test that failed decryption warning only prints once."
TESTRESULT="OK"

$PROG --format=json simulations/simulation_bad_keys.txt room fhkvdataiii 03065716 NOKEY > $TEST/test_output.txt 2> $TEST/test_stderr.txt

if [ "$(wc -l $TEST/test_output.txt | cut -f 1 -d ' ')" != "2" ]
then
    echo Expected two lines of decoded telegrams output.
    TESTRESULT="ERROR"
fi

if [ "$(wc -l $TEST/test_stderr.txt | cut -f 1 -d ' ')" != "1" ]
then
    echo Expected one line of warning of failed keys.
    TESTRESULT="ERROR"
fi

if [ "$(grep -o "Permanently ignoring telegrams" $TEST/test_stderr.txt)" != "Permanently ignoring telegrams" ]
then
    echo "Expected a warning about permanently ignoring a telegram! But got:"
    echo "------------------------------------------"
    cat $TEST/test_stderr.txt
    echo "------------------------------------------"
    TESTRESULT="ERROR"
fi

echo ${TESTRESULT}: $TESTNAME
