#!/bin/sh

PROG="$1"
DRIVERS="$2"

mkdir -p testoutput
TEST=testoutput

echo "Testing drivers"

TESTNAME="Test driver tests"
TESTRESULT="ERROR"

ALL_DRIVERS=$(cd src; echo driver_*cc)

if [ "$DRIVERS" = "" ]
then
    for i in $ALL_DRIVERS
    do
        if grep -q '// Test:' src/$i
        then
            DRIVERS="$DRIVERS $i"
        fi
    done
fi

for i in $DRIVERS
do
    TESTNAME=$i
    rm -f $TEST/*
    METERS=$(cat src/$i | grep '// Test:' | cut -f 2 -d ':' | tr -d '\n')
    sed -n '/\/\/ Test:/,$p' src/$i | grep -e '// telegram' -e '// {' -e '// |' | sed 's|// ||g' > $TEST/simulation.txt
    cat $TEST/simulation.txt | grep '^{' | jq --sort-keys . > $TEST/test_expected_json.txt
    $PROG --format=json --ignoreduplicates=false $TEST/simulation.txt $METERS 2> $TEST/test_stderr_json.txt | jq --sort-keys . > $TEST/test_output_json.txt
    if [ "$?" = "0" ]
    then
        cat $TEST/test_output_json.txt | sed 's/"timestamp": "....-..-..T..:..:..Z"/"timestamp": "1111-11-11T11:11:11Z"/' > $TEST/test_response_json.txt
        diff $TEST/test_expected_json.txt $TEST/test_response_json.txt
        if [ "$?" = "0" ]
        then
            echo OK json: $TESTNAME
            TESTRESULT="OK"
        else
            TESTRESULT="ERROR"
        fi
    else
        echo "wmbusmeters returned error code: $?"
        cat $TEST/test_output_json.txt
        cat $TEST/test_stderr_json.txt
    fi

    cat $TEST/simulation.txt | grep '^|' | sed 's/^|//' > $TEST/test_expected_fields.txt
    $PROG --format=fields --ignoreduplicates=false $TEST/simulation.txt $METERS  > $TEST/test_output_fields.txt 2> $TEST/test_stderr_fields.txt
    if [ "$?" = "0" ]
    then
        cat $TEST/test_output_fields.txt | sed 's/[0-9][0-9][0-9][0-9]-[0-9][0-9]-[0-9][0-9] [0-9][0-9]:[0-9][0-9].[0-9][0-9]$/1111-11-11 11:11.11/' > $TEST/test_response_fields.txt
        diff $TEST/test_expected_fields.txt $TEST/test_response_fields.txt
        if [ "$?" = "0" ]
        then
            echo OK fields: $TESTNAME
            TESTRESULT="OK"
        else
            TESTRESULT="ERROR"
        fi
    else
        echo "wmbusmeters returned error code: $?"
        cat $TEST/test_output_fields.txt
        cat $TEST/test_stderr_fields.txt
    fi

    if [ "$TESTRESULT" = "ERROR" ]
    then
        echo ERROR: $TESTNAME
        exit 1
    fi

done
