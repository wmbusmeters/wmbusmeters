#!/bin/sh

PROG="$1"
TEST=testoutput
rm -f $TEST/thelog2.txt
rm -rf $TEST/meter_readings2
mkdir -p $TEST/meter_readings2

TESTNAME="Test logfile"
TESTRESULT="ERRORS"

RES=$($PROG --useconfig=tests/config2 2>&1)

if [ ! "$RES" = "" ]
then
    echo ERROR: $TESTNAME
    echo Expected no output on stdout and stderr
    echo but got------------------
    echo $RES
    echo ---------------------
    exit 1
fi

cat simulations/simulation_t1.txt | grep '^{' | grep 12345699 | tail -n 1 | jq --sort-keys . > $TEST/test_expected.txt
cat $TEST/meter_readings2/MoreWater | jq --sort-keys . | sed 's/"timestamp": "....-..-..T..:..:..Z"/"timestamp": "1111-11-11T11:11:11Z"/' > $TEST/test_response.txt
diff $TEST/test_expected.txt $TEST/test_response.txt
if [ ! "$?" = "0" ]
then
    echo ERROR: $TESTNAME
    echo Expected to find meter MoreWater in readings, but did not.
    exit 1
fi

cat simulations/simulation_t1.txt | grep '^{' | grep supercom | grep 12345678 | tail -n 1 | jq --sort-keys . > $TEST/test_expected.txt
cat $TEST/meter_readings2/MyWarmWater | jq --sort-keys . | sed 's/"timestamp": "....-..-..T..:..:..Z"/"timestamp": "1111-11-11T11:11:11Z"/' > $TEST/test_response.txt
diff $TEST/test_expected.txt $TEST/test_response.txt
if [ ! "$?" = "0" ]
then
    echo ERROR: $TESTNAME
    echo Expected to find meter MyWarmWater in readings, but did not.
    exit 1
fi

cat simulations/simulation_t1.txt | grep '^{' | grep 11111111 | tail -n 1 | jq --sort-keys . > $TEST/test_expected.txt
cat $TEST/meter_readings2/MyColdWater | jq --sort-keys . | sed 's/"timestamp": "....-..-..T..:..:..Z"/"timestamp": "1111-11-11T11:11:11Z"/' > $TEST/test_response.txt
diff $TEST/test_expected.txt $TEST/test_response.txt
if [ ! "$?" = "0" ]
then
    echo ERROR: $TESTNAME
    echo Expected to find meter MyColdWater in readings, but did not.
    exit 1
fi

RES=$(cat $TEST/thelog2.txt | tr '\n' ' '  | tr -d ' ' | sed 's/|+1 /|+0 /g' )

# The sed replacement reduces the risk of a failing test when the second counter happens to flip within the test run.

EXP=$(printf 'telegram=|A244EE4D785634123C067A8F000000_0C1348550000426CE1F14C130000000082046C21298C0413330000008D04931E3A3CFE3300000033000000330000003300000033000000330000003300000033000000330000003300000033000000330000004300000034180000046D0D0B5C2B03FD6C5E150082206C5C290BFD0F0200018C4079678885238310FD3100000082106C01018110FD610002FD66020002FD170000|+0 telegram=|A244EE4D111111113C077AAC000000_0C1389490000426CE1F14C130000000082046C21298C0413010000008D04931E3A3CFE0100000001000000010000000100000001000000010000000100000001000000010000000100000001000000010000001600000031130000046D0A0C5C2B03FD6C60150082206C5C290BFD0F0200018C4079629885238310FD3100000082106C01018110FD610002FD66020002FD170000|+0 telegram=|1E44AE4C9956341268077A36001000_2F2F0413181E0000023B00002F2F2F2F|+0 telegram=|1844AE4C4455223368077A55000000_041389E20100023B0000|+0(wmbus)telegramlengthbyte(thefirst)0x31(49)isprobablywrong.Expected0x34(52)basedonthelengthofthetelegram.' | tr '\n' ' '  | tr -d ' ')

if [ "$RES" != "$EXP" ]
then
    echo ERROR: $TESTNAME
    echo Expected:
    echo $EXP
    echo But got:
    echo $RES
    exit 1
fi

echo OK: $TESTNAME
