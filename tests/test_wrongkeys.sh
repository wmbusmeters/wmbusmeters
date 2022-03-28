#!/bin/sh

PROG="$1"

TEST=testoutput

rm -rf $TEST
mkdir -p $TEST

TESTNAME="Test wrong keys"
TESTRESULT="ERROR"

cat simulations/serial_aes.msg | grep '^{' | tr -d '#' > $TEST/test_expected.txt
cat simulations/serial_aes.msg | grep '^[CT]' | tr -d '#' > $TEST/test_input.txt
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
(wmbus) WARNING!! decrypted content failed check, did you use the correct decryption key? Permanently ignoring telegrams from id: 88888888 mfct: (APA) Apator, Poland (0x601) type: Water meter (0x07) ver: 0x05
(meter) newly created meter (ApWater 88888888 apator162) did not handle telegram!
(wmbus) WARNING! decrypted payload crc failed check, did you use the correct decryption key? 979f payload crc (calculated 3431) Permanently ignoring telegrams from id: 76348799 mfct: (KAM) Kamstrup Energi (0x2c2d) type: Cold water meter (0x16) ver: 0x1b
(meter) newly created meter (Vatten 76348799 multical21) did not handle telegram!
(wmbus) WARNING!! decrypted content failed check, did you use the correct decryption key? Permanently ignoring telegrams from id: 77777777 mfct: (SON) Sontex, Switzerland (0x4dee) type: Water meter (0x07) ver: 0x3c
(meter) newly created meter (Wasser 77777777 supercom587) did not handle telegram!
EOF

diff $TEST/test_expected.txt $TEST/test_stderr.txt
if [ "$?" = "0" ]
then
    echo "OK: $TESTNAME"
    TESTRESULT="OK"
fi

if [ "$TESTRESULT" = "ERROR" ]; then echo ERROR: $TESTNAME;  exit 1; fi
