#!/bin/bash

PROG="$1"

TEST=testoutput

rm -rf $TEST
mkdir -p $TEST

cat tests/rtlwmbus_water.sh | grep '^#{' | tr -d '#' > $TEST/test_expected.txt
$PROG --format=json "rtlwmbus:tests/rtlwmbus_water.sh" \
      ApWater apator162 88888888 00000000000000000000000000000000 \
      | grep -v "(rtlwmbus) child process exited! Command was:" \
      > $TEST/test_output.txt

cat $TEST/test_output.txt | sed 's/"timestamp":"....-..-..T..:..:..Z"/"timestamp":"1111-11-11T11:11:11Z"/' > $TEST/test_response.txt
diff $TEST/test_expected.txt $TEST/test_response.txt
if [ "$?" == "0" ]
then
    echo RTLWMBUS OK
else
    echo Failure.
    exit 1
fi
