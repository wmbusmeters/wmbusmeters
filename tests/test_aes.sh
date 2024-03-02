#!/bin/sh

PROG="$1"

TEST=testaes

rm -rf $TEST
mkdir -p $TEST

TESTNAME="Test aes encrypted telegrams"
TESTRESULT="ERROR"

cat simulations/serial_aes.msg | grep '^{' | tr -d '#' | jq --sort-keys . > $TEST/test_expected.txt
cat simulations/serial_aes.msg | grep '^[CT]' | tr -d '#' > $TEST/test_input.txt
cat $TEST/test_input.txt | $PROG --format=json "stdin:rtlwmbus" \
      ApWater apator162   88888888 00000000000000000000000000000000 \
      Vatten  multical21  76348799 28F64A24988064A079AA2C807D6102AE 2> $TEST/test_stderr.txt | jq --sort-keys . > $TEST/test_output.txt

cat $TEST/test_output.txt | sed 's/"timestamp": "....-..-..T..:..:..Z"/"timestamp": "1111-11-11T11:11:11Z"/' > $TEST/test_response.txt
diff $TEST/test_expected.txt $TEST/test_response.txt
if [ "$?" = "0" ]
then
    echo "OK: $TESTNAME"
    TESTRESULT="OK"
else
    if [ "$USE_MELD" = "true" ]
    then
        meld $TEST/test_expected.txt $TEST/test_response.txt
    fi
fi

if [ "$TESTRESULT" = "ERROR" ]; then echo ERROR: $TESTNAME;  exit 1; fi

TESTNAME="Test that telegram really is encrypted"
TESTRESULT="ERROR"

cat > $TEST/test_expected.txt <<EOF
(wmbus) WARNING!! telegram should have been fully encrypted, but was not! id: 88888888 mfct: (APA) Apator, Poland (0x601) type: Water meter (0x07) ver: 0x05
(meter) newly created meter (ApWater 88888888.M=APA.V=05.T=07 apator162) did not handle telegram!
(wmbus) WARNING! decrypted payload crc failed check, did you use the correct decryption key? e1d6 payload crc (calculated a528) Permanently ignoring telegrams from id: 76348799 mfct: (KAM) Kamstrup Energi (0x2c2d) type: Cold water meter (0x16) ver: 0x1b
(meter) newly created meter (Vatten 76348799.M=KAM.V=1b.T=16 multical21) did not handle telegram!
(wmbus) WARNING!! telegram should have been fully encrypted, but was not! id: 77777777 mfct: (SON) Sontex, Switzerland (0x4dee) type: Water meter (0x07) ver: 0x3c
(meter) newly created meter (Wasser 77777777.M=SON.V=3c.T=07 supercom587) did not handle telegram!
(wmbus) WARNING!!! telegram should have been encrypted, but was not! id: 77777777 mfct: (SON) Sontex, Switzerland (0x4dee) type: Water meter (0x07) ver: 0x3c
EOF

$PROG --format=json simulations/simulation_aes_removed.msg \
      ApWater apator162   88888888 00000000000000000000000000000000 \
      Vatten  multical21  76348799 28F64A24988064A079AA2C807D6102AE \
      Wasser  supercom587 77777777 5065747220486F6C79737A6577736B69 > $TEST/test_output.txt 2>&1

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
