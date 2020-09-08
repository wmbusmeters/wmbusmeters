#!/bin/sh

PROG="$1"

rm -rf testoutput
mkdir -p testoutput

TEST=testoutput
SIM=simulations/simulation_c1.txt

$PROG --oneshot --verbose $SIM MyHeater multical302 67676767 NOKEY MyTapWater multical21 76348799 NOKEY > $TEST/test_output.txt

RES=$(cat $TEST/test_output.txt | grep -o "(main) all meters have received at least one update, stopping." | tail -n 1)

if [ "$RES" = "(main) all meters have received at least one update, stopping." ]
then
    echo OK: Test oneshot
else
    echo ERROR Fail oneshot check!
    exit 1
fi
