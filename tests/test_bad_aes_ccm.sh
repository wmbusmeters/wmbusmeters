#!/bin/sh

. tests/include.sh

PROG="$1"

mkdir -p testoutput
TEST=testoutput

TESTNAME="Test too short telegram that does not have enough bytes for the tag."
TESTRESULT="OK"

$PROG --debug 1d4496ff58a2ffff7fff7a1500102a2f96ff58a2ffff7fff7a1500102a2fff 2>&1 | grep "expected more data" | uniq > $TEST/test_output.txt

cat <<EOF > $TEST/expected_output.txt
(wmbus) parser expected more data! (1282)
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
