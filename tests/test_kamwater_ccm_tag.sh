#!/bin/sh

. tests/include.sh

PROG="$1"

mkdir -p testoutput
TEST=testoutput

TESTNAME="Test that a security mode 10 aes-ccm telegram warns once on a corrupted tag, stays silent with a valid tag or without a key."
TESTRESULT="OK"

cat <<EOF > $TEST/expected_output_badtag.txt
{
  "_": "telegram",
  "accoustic_noise": "FFFFFFFFFFFF",
  "driver": "kamwater",
  "id": "53820306",
  "media": "cold water",
  "name": "water",
  "status": "TEMPORARY_ERROR FAILED_DECODE",
  "target_m3": 0,
  "timestamp": "1111-11-11T11:11:11Z",
  "total_m3": 0
}
EOF

cat <<EOF > $TEST/expected_output_validtag.txt
{
  "_": "telegram",
  "accoustic_noise": "FFFFFFFFFFFF",
  "driver": "kamwater",
  "id": "53820306",
  "media": "cold water",
  "name": "water",
  "status": "TEMPORARY_ERROR",
  "target_m3": 0,
  "timestamp": "1111-11-11T11:11:11Z",
  "total_m3": 0
}
EOF

cat <<EOF > $TEST/expected_output_nokey.txt
{
  "_": "telegram",
  "driver": "kamwater",
  "id": "53820306",
  "media": "cold water",
  "name": "water",
  "status": "TEMPORARY_ERROR",
  "timestamp": "1111-11-11T11:11:11Z"
}
EOF

# With a correct meter key a security mode 10 (OMS security profile D)
# telegram whose aes-ccm tag does not verify (radio corrupted) must be
# marked FAILED_DECODE and warn exactly once, even though the simulation
# sends the same telegram twice. With a valid tag the decode proceeds and
# no warning prints. Without any meter key the telegram is left as is
# (no tag check, no warning, no FAILED_DECODE), the TEMPORARY_ERROR comes
# from the tpl status byte which is not encrypted.

$PROG --format=json simulations/simulation_kamwater_ccm_tag_bad.txt water kamwater 53820306 BF902DD6DFADAFE83E4AE6832B5FA14C \
      2> $TEST/test_stderr.txt | jq --sort-keys . | sed 's/"timestamp": "....-..-..T..:..:..Z"/"timestamp": "1111-11-11T11:11:11Z"/' > $TEST/test_output.txt

cat > $TEST/expected_err.txt <<EOF
(wmbus) WARNING!!! aes-ccm authentication tag did not verify, wrong key or corrupted telegram? Marking telegrams from id: 53820306 mfct: (KAW) Kamstrup, Denmark (0x2c37) type: Cold water meter (0x16) ver: 0x3c
EOF

diff $TEST/test_output.txt $TEST/expected_output_badtag.txt

if [ "$?" != "0" ]
then
    TESTRESULT="ERROR"
fi

diff $TEST/test_stderr.txt $TEST/expected_err.txt

if [ "$?" != "0" ]
then
    TESTRESULT="ERROR"
fi

# With a valid tag the telegram decodes and no warning must print.

$PROG --format=json simulations/simulation_kamwater_ccm_tag_ok.txt water kamwater 53820306 BF902DD6DFADAFE83E4AE6832B5FA14C \
      2> $TEST/test_stderr.txt | jq --sort-keys . | sed 's/"timestamp": "....-..-..T..:..:..Z"/"timestamp": "1111-11-11T11:11:11Z"/' > $TEST/test_output.txt

cat > $TEST/expected_err.txt <<EOF
EOF

diff $TEST/test_output.txt $TEST/expected_output_validtag.txt

if [ "$?" != "0" ]
then
    TESTRESULT="ERROR"
fi

diff $TEST/test_stderr.txt $TEST/expected_err.txt

if [ "$?" != "0" ]
then
    TESTRESULT="ERROR"
fi

# Without any meter key the security mode 10 telegram is left as is: no
# tag verification, no warning, no FAILED_DECODE and no decoded payload
# fields. The status is only the tpl status byte.

$PROG --format=json simulations/simulation_kamwater_ccm_tag_bad.txt water kamwater 53820306 NOKEY \
      2> $TEST/test_stderr.txt | jq --sort-keys . | sed 's/"timestamp": "....-..-..T..:..:..Z"/"timestamp": "1111-11-11T11:11:11Z"/' > $TEST/test_output.txt

cat > $TEST/expected_err.txt <<EOF
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

if [ "$TESTRESULT" = "OK" ]
then
    printOK "$TESTNAME"
else
    printERROR "$TESTNAME"
fi