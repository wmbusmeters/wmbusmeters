#!/bin/bash

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
      > $TEST/test_output.txt

cat $TEST/test_output.txt | grep -v '{"media' > $TEST/test_response.txt

cat <<EOF > $TEST/test_expected.txt
(wmbus) decrypted content failed check, did you use the correct decryption key? Ignoring telegram.
(wmbus) payload crc error!
(wmbus) decrypted content failed check, did you use the correct decryption key? Ignoring telegram.
EOF

diff $TEST/test_expected.txt $TEST/test_response.txt
if [ "$?" == "0" ]
then
    echo "OK: $TESTNAME"
    TESTRESULT="OK"
fi

if [ "$TESTRESULT" = "ERROR" ]; then echo ERROR: $TESTNAME;  exit 1; fi
