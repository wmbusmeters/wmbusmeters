#!/bin/sh

PROG="$1"

if [ "$PROG" = "" ]
then
    echo Please supply the binary to be tested as the first argument.
    exit 1
fi

TEST=testoutput

rm -rf $TEST
mkdir -p $TEST

TESTNAME="Test config override with oneshot"
TESTRESULT="ERROR"

cat simulations/serial_aes.msg | grep '^{' | tr -d '#' > $TEST/test_expected.txt
cat simulations/serial_aes.msg | grep '^[CT]' | tr -d '#' > $TEST/test_input.txt

cat $TEST/test_input.txt | $PROG --useconfig=tests/config9 --device=stdin:rtlwmbus --oneshot > $TEST/test_output.txt 2> $TEST/test_stderr.txt

if ! grep -q "(main) all meters have received at least one update, stopping." $TEST/test_stderr.txt
then
    echo "ERROR: $TESTNAME ($0)"
    echo "Expected stderr to print \"all meters have received at least one update\""
    exit 1
fi

cat $TEST/test_output.txt | sed 's/"timestamp":"....-..-..T..:..:..Z"/"timestamp":"1111-11-11T11:11:11Z"/' > $TEST/test_response.txt
diff $TEST/test_expected.txt $TEST/test_response.txt
if [ "$?" = "0" ]
then
    echo "OK: $TESTNAME"
    TESTRESULT="OK"
else
    echo "ERROR: $TESTNAME"
    exit 1
fi

TESTNAME="Test config override with exitafter"
TESTRESULT="ERROR"

cat simulations/serial_aes.msg | grep '^{' | tr -d '#' > $TEST/test_expected.txt
cat simulations/serial_aes.msg | grep '^[CT]' | tr -d '#' > $TEST/test_input.txt
# Read from stdin
$PROG --useconfig=tests/config9 --device=stdin:rtlwmbus --oneshot --exitafter=1s  > $TEST/test_output.txt 2> $TEST/test_stderr.txt

if ! grep -q "(serial) exit after 2 seconds" $TEST/test_stderr.txt
then
    echo "ERROR: $TESTNAME ($0)"
    echo "Expected stderr to print \"exit after 2 seconds\""
    exit 1
else
    echo "OK: $TESTNAME"
    TESTRESULT=OK
fi

if [ "$TESTRESULT" = "ERROR" ]
then
    echo "ERROR: $TESTNAME ($0)"
    exit 1
fi
