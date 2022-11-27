#!/bin/sh

PROG="$1"

rm -rf testoutput
mkdir -p testoutput

TEST=testoutput
SIM=simulations/simulation_oneshot.txt

$PROG --oneshot --verbose $SIM MyHeater multical302 67676767 NOKEY MyTapWater multical21 76348799 NOKEY > $TEST/test_output.txt 2> $TEST/test_stderr.txt

RES=$(cat $TEST/test_stderr.txt | grep -o "(main) all meters have received at least one update, stopping." | tail -n 1)

if [ "$RES" = "(main) all meters have received at least one update, stopping." ]
then
    echo OK: Test oneshot
else
    echo ERROR Fail oneshot check!
    exit 1
fi
