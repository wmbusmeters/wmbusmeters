#!/bin/sh

PROG="$1"

mkdir -p testoutput
TEST=testoutput

TESTNAME="Test address type filtering"
TESTRESULT="OK"

$PROG --format=fields simulations/simulation_qheat_bad.txt HEAT qheat '67911475.T=04' NOKEY > $TEST/test_output.txt 2>&1

cat $TEST/test_output.txt | sed 's/[0-9][0-9][0-9][0-9]-[0-9][0-9]-[0-9][0-9] [0-9][0-9]:[0-9][0-9].[0-9][0-9]$/1111-11-11 11:11.11/' > $TEST/test_responses.txt

cat > $TEST/test_expected.txt <<EOF
HEAT;67911475;49;2024-01-31;11;1111-11-11 11:11.11
EOF

if ! diff $TEST/test_expected.txt $TEST/test_responses.txt
then
    if [ "$USE_MELD" = "true" ]
    then
        meld $TEST/test_expected.txt $TEST/test_responses.txt
    fi
    echo "ERROR: $TESTNAME"
else
    echo OK: $TESTNAME
fi
