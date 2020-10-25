#!/bin/sh

PROG="$1"

TEST=testaes

rm -rf $TEST
mkdir -p $TEST

TESTNAME="Test wrong keys"
TESTRESULT="ERROR"

cat simulations/simulation_aes.msg | grep '^{' | tr -d '#' > $TEST/test_expected.txt
cat simulations/simulation_aes.msg | grep '^[CT]' | tr -d '#' > $TEST/test_input.txt
cat $TEST/test_input.txt | $PROG --format=json "stdin:rtlwmbus" \
      ApWater apator162   88888888 00000000000000000000000000000001 \
      Vatten  multical21  76348799 28F64A24988064A079AA2C807D6102AF \
      Wasser  supercom587 77777777 5065747220486F6C79737A6577736B6A \
      > $TEST/test_output.txt 2> $TEST/test_stderr.txt

if [ -s $TEST/test_output.txt ]
then
    echo "Bad no stdout expected! But got bytes anyway!"
    echo "ERROR: $TESTNAME"
    TESTRESULT="ERROR"
    exit 1
fi

cat <<EOF > $TEST/test_expected.txt
Started config rtlwmbus on stdin listening on any
(wmbus) decrypted content failed check, did you use the correct decryption key? Ignoring telegram.
(wmbus) decrypted payload crc failed check, did you use the correct decryption key? Ignoring telegram.
(wmbus) decrypted content failed check, did you use the correct decryption key? Ignoring telegram.
EOF

diff $TEST/test_expected.txt $TEST/test_stderr.txt
if [ "$?" = "0" ]
then
    echo "OK: $TESTNAME"
    TESTRESULT="OK"
fi

if [ "$TESTRESULT" = "ERROR" ]; then echo ERROR: $TESTNAME;  exit 1; fi
