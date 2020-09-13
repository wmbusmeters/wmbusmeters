#!/bin/sh

PROG="$1"

mkdir -p testoutput

TEST=testoutput

TESTNAME="Test C1 meters"
TESTRESULT="ERROR"

cat simulations/simulation_c1.txt | grep '^{' > $TEST/test_expected.txt
$PROG --format=json simulations/simulation_c1.txt \
      MyHeater multical302 67676767 "" \
      MyTapWater multical21 76348799 "" \
      Vadden multical21 44556677 "" \
      MyElement qcaloric 78563412 "" \
      Rum cma12w 66666666 "" \
      My403Cooling multical403 78780102 "" \
      > $TEST/test_output.txt 2> $TEST/test_stderr.txt

if [ "$?" = "0" ]
then
    cat $TEST/test_output.txt | sed 's/"timestamp":"....-..-..T..:..:..Z"/"timestamp":"1111-11-11T11:11:11Z"/' > $TEST/test_responses.txt
    diff $TEST/test_expected.txt $TEST/test_responses.txt
    if [ "$?" = "0" ]
    then
        echo OK: $TESTNAME
        TESTRESULT="OK"
    fi
fi

if [ "$TESTRESULT" = "ERROR" ]
then
    echo ERROR: $TESTNAME
    exit 1
fi
