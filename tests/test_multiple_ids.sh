#!/bin/sh

PROG="$1"

mkdir -p testoutput
TEST=testoutput

TESTNAME="Test listen to wildcard * id"
TESTRESULT="ERROR"

SIM=simulations/simulation_multiple_qcalorics.txt

cat $SIM | grep '^{' > $TEST/test_expected.txt
$PROG --format=json $SIM \
      Element qcaloric '*' '' \
      > $TEST/test_output.txt

if [ "$?" = "0" ]
then
    cat $TEST/test_output.txt | sed 's/"timestamp":"....-..-..T..:..:..Z"/"timestamp":"1111-11-11T11:11:11Z"/' > $TEST/test_responses.txt
    diff $TEST/test_expected.txt $TEST/test_responses.txt
    if [ "$?" = "0" ]
    then
        echo "OK: $TESTNAME"
        TESTRESULT="OK"
    fi
fi

if [ "$TESTRESULT" = "ERROR" ]; then echo ERROR: $TESTNAME;  exit 1; fi

TESTNAME="Test listen to wildcard suffix 8856* id"
TESTRESULT="ERROR"

cat $SIM | grep '^{' | grep 8856 > $TEST/test_expected.txt

$PROG --format=json $SIM \
      Element qcaloric '8856*' '' \
      > $TEST/test_output.txt


if [ "$?" = "0" ]
then
    cat $TEST/test_output.txt | sed 's/"timestamp":"....-..-..T..:..:..Z"/"timestamp":"1111-11-11T11:11:11Z"/' > $TEST/test_responses.txt
    diff $TEST/test_expected.txt $TEST/test_responses.txt
    if [ "$?" = "0" ]
    then
        echo "OK: $TESTNAME"
        TESTRESULT="OK"
    fi
fi

if [ "$TESTRESULT" = "ERROR" ]; then echo ERROR: $TESTNAME;  exit 1; fi

TESTNAME="Test listen to two comma separted ids"
TESTRESULT="ERROR"

cat $SIM | grep '^{' | grep -v 88563414 > $TEST/test_expected.txt

$PROG --format=json $SIM \
      Element qcaloric '78563412,78563413' '' \
      > $TEST/test_output.txt

if [ "$?" = "0" ]
then
    cat $TEST/test_output.txt | sed 's/"timestamp":"....-..-..T..:..:..Z"/"timestamp":"1111-11-11T11:11:11Z"/' > $TEST/test_responses.txt
    diff $TEST/test_expected.txt $TEST/test_responses.txt
    if [ "$?" = "0" ]
    then
        echo "OK: $TESTNAME"
        TESTRESULT="OK"
    fi
fi

if [ "$TESTRESULT" = "ERROR" ]; then echo ERROR: $TESTNAME;  exit 1; fi

TESTNAME="Test listen to three comma separted ids"
TESTRESULT="ERROR"

cat $SIM | grep '^{' > $TEST/test_expected.txt

$PROG --format=json $SIM \
      Element qcaloric '78563412,78563413,88563414' '' \
      > $TEST/test_output.txt

if [ "$?" = "0" ]
then
    cat $TEST/test_output.txt | sed 's/"timestamp":"....-..-..T..:..:..Z"/"timestamp":"1111-11-11T11:11:11Z"/' > $TEST/test_responses.txt
    diff $TEST/test_expected.txt $TEST/test_responses.txt
    if [ "$?" = "0" ]
    then
        echo "OK: $TESTNAME"
        TESTRESULT="OK"
    fi
fi

if [ "$TESTRESULT" = "ERROR" ]; then echo ERROR: $TESTNAME;  exit 1; fi

TESTNAME="Test listen with negated ids"
TESTRESULT="ERROR"

cat $SIM | grep '^{' | grep -v 88563414 | grep -v 78563413 > $TEST/test_expected.txt

$PROG --format=json $SIM \
      Element qcaloric '*,!88563414,!78563413' '' \
      > $TEST/test_output.txt

if [ "$?" = "0" ]
then
    cat $TEST/test_output.txt | sed 's/"timestamp":"....-..-..T..:..:..Z"/"timestamp":"1111-11-11T11:11:11Z"/' > $TEST/test_responses.txt
    diff $TEST/test_expected.txt $TEST/test_responses.txt
    if [ "$?" = "0" ]
    then
        echo "OK: $TESTNAME"
        TESTRESULT="OK"
    fi
fi

if [ "$TESTRESULT" = "ERROR" ]; then echo ERROR: $TESTNAME;  exit 1; fi

TESTNAME="Test listen with wrong driver and wildcard"
TESTRESULT="ERROR"

cat > $TEST/test_expected.txt <<EOF
(meter) ignoring telegram from 78563412 since it matched a wildcard id rule but driver does not match.
(meter) ignoring telegram from 78563413 since it matched a wildcard id rule but driver does not match.
(meter) ignoring telegram from 88563414 since it matched a wildcard id rule but driver does not match.
EOF

$PROG --verbose --format=json $SIM \
      Element sontex868 '*' '' \
      2>&1 | grep '(meter)' >  $TEST/test_output.txt

if [ "$?" = "0" ]
then
    diff $TEST/test_expected.txt $TEST/test_output.txt
    if [ "$?" = "0" ]
    then
        echo "OK: $TESTNAME"
        TESTRESULT="OK"
    fi
fi

if [ "$TESTRESULT" = "ERROR" ]; then echo ERROR: $TESTNAME;  exit 1; fi
