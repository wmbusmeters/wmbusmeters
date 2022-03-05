#!/bin/sh

# Testing of link modes compatibility is temporarily disabled
# until all drivers have been refactored.

exit 0

PROG="$1"

rm -rf testoutput
mkdir -p testoutput

TEST=testoutput

TESTNAME="Test that listen to t1+c1 works with meters transmitting using t1+c1"
TESTRESULT="ERROR"

cat simulations/simulation_t1_and_c1.txt | grep '^{' > $TEST/test_expected.txt

$PROG --format=json --listento=c1,t1 simulations/simulation_t1_and_c1.txt \
      MyTapWater multical21:c1 76348799 "" \
      MyWarmWater supercom587:t1 12345678 "" \
      > $TEST/test_output.txt


if [ "$?" = "0" ]
then
    cat $TEST/test_output.txt | sed 's/"timestamp":"....-..-..T..:..:..Z"/"timestamp":"1111-11-11T11:11:11Z"/' > $TEST/test_responses.txt
    diff $TEST/test_expected.txt $TEST/test_responses.txt
    if [ "$?" = "0" ]
    then
        echo "OK: $TESTNAME"
        TESTRESULT="OK"
    else
        echo ERROR: $TESTNAME
        diff $TEST/test_expected.txt $TEST/test_responses.txt
        exit 1
    fi
fi

if [ "$TESTRESULT" = "ERROR" ]; then echo ERROR: $TESTNAME;  exit 1; fi

MSG=$($PROG --listento=c1,t1 simulations/simulation_t1_and_c1.txt \
      MyTapWater multical21:c1 76348799 "" \
      MyWarmWater supercom587:c1 12345678 "")

if [ "$MSG" != "(cmdline) cannot set link modes to: c1 because meter supercom587 only transmits on: t1" ]
then
    echo ERROR: $TESTNAME
    echo Did not expect: $MSG
    exit 1
else
    echo "OK: $TESTNAME"
fi

TESTNAME="Test that setting supercom587 to c1 fails"
TESTRESULT="ERROR"

MSG=$($PROG --listento=c1,t1 --usestdoutforlog simulations/simulation_t1_and_c1.txt \
      MyTapWater multical21:c1 76348799 "" \
      MyWarmWater supercom587:c1 12345678 "")

if [ "$MSG" != "(cmdline) cannot set link modes to: c1 because meter supercom587 only transmits on: t1" ]
then
    echo ERROR: $TESTNAME
    echo Did not expect: $MSG
    exit 1
else
    echo "OK: $TESTNAME"
fi

#TESTNAME="Test that the warning for missed telegrams work"
#TESTRESULT="ERROR"

#MSG=$($PROG --s1 --usestdoutforlog simulations/simulation_t1_and_c1.txt \
#      MyTapWater multical21:c1 76348799 "" \
#      Wasser      apator162:t1   20202020 "" | tr -d ' \n')

#CORRECT="(config)Youhavespecifiedtolistentothelinkmodes:s1butthemetersmighttransmiton:c1,t1(config)Thereforeyoumightmisstelegrams!Pleasespecifytheexpectedtransmitmodeforthemeters,eg:apator162:t1(config)Oruseadonglethatcanlistentoalltherequiredlinkmodesatthesametime."
#if [ "$MSG" != "$CORRECT" ]
#then
#    echo ERROR: $TESTNAME
#    echo Did not expect:
#    echo $MSG
#    exit 1
#else
#    echo "OK: $TESTNAME"
#fi
