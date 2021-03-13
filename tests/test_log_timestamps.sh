#!/bin/sh

PROG="$1"

TEST=testaes

rm -rf $TEST
mkdir -p $TEST

TESTNAME="Test log timestamps"

cat simulations/simulation_aes.msg | grep '^[CT]' | grep 76348799 | tr -d '#' > $TEST/test_input.txt
cat $TEST/test_input.txt | $PROG --format=json --logtimestamps=always --verbose "stdin:rtlwmbus" \
      Vatten  multical21  76348799 28F64A24988064A079AA2C807D6102AE > $TEST/test_output.txt 2> $TEST/test_stderr.txt

if ! grep -q "] Started config" $TEST/test_stderr.txt || \
   ! grep -q "] (config) number" $TEST/test_stderr.txt
then
    echo "ERROR: failed --logtimestamps=always"
    exit 1
fi

cat $TEST/test_input.txt | $PROG --format=json --logtimestamps=important --verbose "stdin:rtlwmbus" \
      Vatten  multical21  76348799 00112233445566778899AABBCCDDEEFF > $TEST/test_output.txt 2> $TEST/test_stderr.txt

if ! grep -q "] Started config" $TEST/test_stderr.txt || \
   ! grep -q "^(config) number" $TEST/test_stderr.txt
then
    echo "ERROR: failed --logtimestamps=important"
    exit 1
fi

cat $TEST/test_input.txt | $PROG --format=json --logtimestamps=never --verbose "stdin:rtlwmbus" \
      Vatten  multical21  76348799 00112233445566778899AABBCCDDEEFF > $TEST/test_output.txt 2> $TEST/test_stderr.txt

if ! grep -q "^Started config" $TEST/test_stderr.txt || \
   ! grep -q "^(config) number" $TEST/test_stderr.txt
then
    echo "ERROR: failed --logtimestamps=never"
    exit 1
fi

echo "OK: $TESTNAME"
