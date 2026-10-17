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
