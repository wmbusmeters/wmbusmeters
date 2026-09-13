#!/bin/sh

. tests/include.sh

PROG="$1"

mkdir -p testoutput
TEST=testoutput

TESTNAME="Test that the encrypted Qundis walk-by block warns when no key or a wrong key is supplied."
TESTRESULT="OK"

cat <<EOF > $TEST/expected_output_nokey.txt
{
  "_": "telegram",
  "driver": "qwaterv2",
  "id": "14356101",
  "media": "water",
  "meter_datetime": "2026-08-15 10:22",
  "name": "water",
  "status": "MISSING_KEY",
  "timestamp": "1111-11-11T11:11:11Z"
}
EOF

cat <<EOF > $TEST/expected_output_wrongkey.txt
{
  "_": "telegram",
  "driver": "qwaterv2",
  "id": "14356101",
  "media": "water",
  "meter_datetime": "2026-08-15 10:22",
  "name": "water",
  "status": "FAILED_DECODE",
  "timestamp": "1111-11-11T11:11:11Z"
}
EOF

# Without a meter key the encrypted WalkByDataSet (header byte[4]=0x35) cannot
# be decoded and the walk by values are left out, fail closed. With a wrong
# meter key the AES-CBC decryption still "succeeds" (no integrity check) but
# the garbage body is rejected by the ixml grammar, fail closed. In both cases
# the warning must print exactly once, even though the simulation sends the
# same telegram twice (see issue #2051).

$PROG --format=json simulations/simulation_qwds_walkby.txt water qwaterv2 14356101 NOKEY \
      2> $TEST/test_stderr.txt | jq --sort-keys . | sed 's/"timestamp": "....-..-..T..:..:..Z"/"timestamp": "1111-11-11T11:11:11Z"/' > $TEST/test_output.txt

cat > $TEST/expected_err.txt <<EOF
(qds) WARNING! no key to decrypt the encrypted WalkByDataSet! Walk by values are not decoded for id: 14356101
EOF

diff $TEST/test_output.txt $TEST/expected_output_nokey.txt

if [ "$?" != "0" ]
then
    TESTRESULT="ERROR"
fi

diff $TEST/test_stderr.txt $TEST/expected_err.txt

if [ "$?" != "0" ]
then
    TESTRESULT="ERROR"
fi

# With the correct meter key the block decrypts and no warning must print.

$PROG --format=json simulations/simulation_qwds_walkby.txt water qwaterv2 14356101 000102030405060708090A0B0C0D0E0F \
      2> $TEST/test_stderr.txt | jq --sort-keys . | sed 's/"timestamp": "....-..-..T..:..:..Z"/"timestamp": "1111-11-11T11:11:11Z"/' > $TEST/test_output.txt

cat > $TEST/expected_err.txt <<EOF
EOF

diff $TEST/test_stderr.txt $TEST/expected_err.txt

if [ "$?" != "0" ]
then
    TESTRESULT="ERROR"
fi

# With a wrong meter key the AES-CBC decryption still "succeeds" but the body
# becomes garbage that the ixml grammar rejects. The warning must print exactly
# once and the output must stay fail closed, like the no key case.

$PROG --format=json simulations/simulation_qwds_walkby.txt water qwaterv2 14356101 FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF \
      2> $TEST/test_stderr.txt | jq --sort-keys . | sed 's/"timestamp": "....-..-..T..:..:..Z"/"timestamp": "1111-11-11T11:11:11Z"/' > $TEST/test_output.txt

cat > $TEST/expected_err.txt <<EOF
(qds) WARNING! failed to decode the encrypted WalkByDataSet! Did you use the correct decryption key for id: 14356101
EOF

diff $TEST/test_stderr.txt $TEST/expected_err.txt

if [ "$?" != "0" ]
then
    TESTRESULT="ERROR"
fi

diff $TEST/test_output.txt $TEST/expected_output_wrongkey.txt

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
