#!/bin/sh

PROG="$1"

mkdir -p testoutput
TEST=testoutput

TESTNAME="Test that failed decryption warning only prints once."
TESTRESULT="OK"

$PROG --format=json simulations/simulation_bad_keys.txt room fhkvdataiv 03065716 NOKEY \
      2> $TEST/test_stderr.txt | jq --sort-keys . > $TEST/test_output.txt

cat > $TEST/expected_err.txt <<EOF
(wmbus) WARNING! no key to decrypt payload! Permanently ignoring telegrams from id: 03065716 mfct: (TCH) Techem (0x5068) type: Heat Cost Allocator (0x08) ver: 0x94
(meter) newly created meter (room 03065716 fhkvdataiv) did not handle telegram!
EOF

diff $TEST/test_stderr.txt $TEST/expected_err.txt

if [ "$?" != "0" ]
then
    TESTRESULT="ERROR"
fi

$PROG --format=json simulations/simulation_bad_keys.txt room fhkvdataiv 03065716 00112233445566778899AABBCCDDEEFF \
      2> $TEST/test_stderr.txt | jq --sort-keys . > $TEST/test_output.txt

cat > $TEST/expected_err.txt <<EOF
(wmbus) WARNING!! decrypted content failed check, did you use the correct decryption key? Permanently ignoring telegrams from id: 03065716 mfct: (TCH) Techem (0x5068) type: Heat Cost Allocator (0x08) ver: 0x94
(meter) newly created meter (room 03065716 fhkvdataiv) did not handle telegram!
EOF

diff $TEST/test_stderr.txt $TEST/expected_err.txt

if [ "$?" != "0" ]
then
    TESTRESULT="ERROR"
fi

echo ${TESTRESULT}: $TESTNAME
