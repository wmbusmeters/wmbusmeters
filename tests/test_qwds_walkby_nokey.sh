#!/bin/sh

PROG="$1"

mkdir -p testoutput
TEST=testoutput

TESTNAME="Test that the encrypted Qundis walk-by block warns once when no key is supplied."
TESTRESULT="OK"

# Without a meter key the encrypted WalkByDataSet (header byte[4]=0x35) cannot
# be decoded and the walk by values are left out, fail closed. The warning must
# print exactly once, even though the simulation sends the same telegram twice
# (see issue #2051).

$PROG --format=json simulations/simulation_qwds_walkby.txt water qwaterv2 14356101 NOKEY \
      2> $TEST/test_stderr.txt | jq --sort-keys . > $TEST/test_output.txt

cat > $TEST/expected_err.txt <<EOF
(qds) WARNING! no key to decrypt the encrypted WalkByDataSet! Walk by values are not decoded for id: 14356101
EOF

diff $TEST/test_stderr.txt $TEST/expected_err.txt

if [ "$?" != "0" ]
then
    TESTRESULT="ERROR"
fi

# With the correct meter key the block decrypts and no warning must print.

$PROG --format=json simulations/simulation_qwds_walkby.txt water qwaterv2 14356101 000102030405060708090A0B0C0D0E0F \
      2> $TEST/test_stderr.txt | jq --sort-keys . > $TEST/test_output.txt

cat > $TEST/expected_err.txt <<EOF
EOF

diff $TEST/test_stderr.txt $TEST/expected_err.txt

if [ "$?" != "0" ]
then
    TESTRESULT="ERROR"
fi

echo ${TESTRESULT}: $TESTNAME