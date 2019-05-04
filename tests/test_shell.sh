#!/bin/bash

PROG="$1"

mkdir -p testoutput

TEST=testoutput

$PROG --shell='echo "$METER_JSON"' simulations/simulation_shell.txt MWW supercom587 12345678 "" > $TEST/test_output.txt
if [ "$?" == "0" ]
then
    cat $TEST/test_output.txt | sed 's/"timestamp":"....-..-..T..:..:..Z"/"timestamp":"1111-11-11T11:11:11Z"/' > $TEST/test_responses.txt
    echo '{"media":"warm water","meter":"supercom587","name":"MWW","id":"12345678","total_m3":5.548,"timestamp":"1111-11-11T11:11:11Z"}' > $TEST/test_expected.txt
    diff $TEST/test_expected.txt $TEST/test_responses.txt
    if [ "$?" == "0" ]
    then
        echo SHELL OK
    fi
else
    echo Failure.
    exit 1
fi
