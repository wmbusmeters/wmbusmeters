#!/bin/sh

. tests/include.sh

PROG="$1"

mkdir -p testoutput
TEST=testoutput

TESTNAME="Test too short telegram with unknown format signature."
TESTRESULT="OK"

$PROG --debug 0c44464646454746464679ff00 2>&1 | grep "format signature" | uniq > $TEST/test_output.txt

cat <<EOF > $TEST/expected_output.txt
(wmbus) 011   : ff00 format signature (unknown)
EOF

diff $TEST/test_output.txt $TEST/expected_output.txt

if [ "$?" != "0" ]
then
    TESTRESULT="ERROR"
fi

if [ "$TESTRESULT" = "OK" ]
then
    printOK "$TESTNAME"
else
    printERROR "$TESTNAME"
fi

TESTNAME="Test too short telegram with no encrypted bytes and missing key."
TESTRESULT="OK"

$PROG --debug 0e44961558890080501b7aff001025 2>&1 | grep "received less bytes than expected" | uniq > $TEST/test_output.txt

cat <<EOF > $TEST/expected_output.txt
(TPL) warning: aes-cbc-iv decryption received less bytes than expected for decryption! Got 0 bytes but expected at least 16 bytes since num encr blocks was 1.
EOF

diff $TEST/test_output.txt $TEST/expected_output.txt

if [ "$?" != "0" ]
then
    TESTRESULT="ERROR"
fi

if [ "$TESTRESULT" = "OK" ]
then
    printOK "$TESTNAME"
else
    printERROR "$TESTNAME"
fi
