#!/bin/sh

PROG="$1"

TEST=testoutput

rm -rf $TEST
mkdir -p $TEST

TESTNAME="Test timestamps from rtlwmbus"
TESTRESULT="ERROR"

cat simulations/serial_rtlwmbus_with_timestamps.msg | grep '^\#' | tr -d '#' > $TEST/test_expected.txt
cat simulations/serial_rtlwmbus_with_timestamps.msg | grep '^[CT]' > $TEST/test_input.txt
cat $TEST/test_input.txt | $PROG --format=json "stdin:rtlwmbus" \
      Rummet1 lansenth 00010203 "" \
      Rummet2 rfmamb 11772288 "" \
      2> $TEST/test_stderr.txt | jq -r .timestamp > $TEST/test_output.txt

diff $TEST/test_expected.txt $TEST/test_output.txt
if [ "$?" = "0" ]
then
    echo "OK: $TESTNAME"
    TESTRESULT="OK"
else
    if [ "$USE_MELD" = "true" ]
    then
        meld $TEST/test_expected.txt $TEST/test_output.txt
    fi
fi

if [ "$TESTRESULT" = "ERROR" ]; then echo ERROR: $TESTNAME;  exit 1; fi
