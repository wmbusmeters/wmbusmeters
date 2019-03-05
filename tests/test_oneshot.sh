#!/bin/bash

PROG="$1"

mkdir -p testoutput

TEST=testoutput
SIM=simulations/simulation_c1.txt

cat $SIM | grep '^{' > $TEST/test_expected.txt

$PROG --oneshot --verbose $SIM MyHeater multical302 '*' '' MyTapWater multical21 76348799 '' > $TEST/test_output.txt

RES=$(cat $TEST/test_output.txt | grep -o "all meters have received at least one update, stopping.")

if [ "$RES" = "all meters have received at least one update, stopping." ]
then
    echo Oneshot OK
else
    echo Fail oneshot check!
    exit 1
fi
