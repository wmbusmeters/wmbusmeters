#!/bin/sh

PROG="$1"

rm -rf testoutput
mkdir -p testoutput

TEST=testoutput

$PROG 434493157856341233038c2075900f002c25b30a000021924d4f2fb66e017a75002007109058475f4bc91df878b80a1b0f98b629024aac727942bfc549233c0140829b93 MyTapWater q40 12345678 NOKEY > $TEST/test_output.txt 2>&1

EXPECT='Not a valid meter driver "q40"'
RES=$(cat $TEST/test_output.txt | grep -o "$EXPECT" | tail -n 1)

if [ "$RES" = "$EXPECT" ]
then
    echo OK: Test non-existant driver
else
    cat $TEST/test_output.txt
    echo ERROR Failed non-existant driver check!
    exit 1
fi
