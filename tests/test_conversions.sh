#!/bin/bash

PROG="$1"

rm -rf testoutput
mkdir -p testoutput
TEST=testoutput

TESTNAME="Test conversions of units"
TESTRESULT="ERROR"

cat simulations/simulation_conversionsadded.txt | grep '^{' > $TEST/test_expected.txt
$PROG --addconversions=GJ,L,F --format=json simulations/simulation_conversionsadded.txt \
      Hettan   vario451    58234965 ""  \
      MyTapWater multical21 76348799 "" \
      > $TEST/test_output.txt

if [ "$?" == "0" ]
then
    cat $TEST/test_output.txt | sed 's/"timestamp":"....-..-..T..:..:..Z"/"timestamp":"1111-11-11T11:11:11Z"/' > $TEST/test_responses.txt
    diff $TEST/test_expected.txt $TEST/test_responses.txt
    if [ "$?" == "0" ]
    then
        echo "OK: $TESTNAME"
        TESTRESULT="OK"
    fi
fi

if [ "$TESTRESULT" = "ERROR" ]; then echo ERROR: $TESTNAME;  exit 1; fi
