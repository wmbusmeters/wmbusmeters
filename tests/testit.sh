#!/bin/bash

TEST=build/test
mkdir -p $TEST

PROG="$1"
ARGS="$2"
HEX="$3"
JSON="$4"
FIELDS="$5"

OK=true

rm -f $TEST/test_output.txt $TEST/test_expected.txt

$PROG --format=json $HEX $ARGS \
    | jq . --sort-keys \
    | sed 's/"timestamp": "....-..-..T..:..:..Z"/"timestamp": "1111-11-11T11:11:11Z"/' \
    > $TEST/test_output.txt

echo "$JSON" | jq . --sort-keys > $TEST/test_expected.txt

if ! diff $TEST/test_expected.txt $TEST/test_output.txt
then
    if [ "$USE_MELD" = "true" ]
    then
        meld $TEST/test_expected.txt $TEST/test_output.txt
    fi
    OK=false
fi

rm -f $TEST/test_output.txt $TEST/test_expected.txt

$PROG --format=fields $HEX $ARGS \
    | sed 's/....-..-.. ..:..:../1111-11-11 11:11.11/' \
    > $TEST/test_output.txt

echo "$FIELDS" > $TEST/test_expected.txt

if ! diff $TEST/test_expected.txt $TEST/test_output.txt
then
    if [ "$USE_MELD" = "true" ]
    then
        meld $TEST/test_expected.txt $TEST/test_output.txt
    fi
    OK=false
fi

if [ "$OK" = "true" ]
then
    echo "OK: $ARGS"
else
    echo "ERROR: $ARGS $TELEGRAM"
fi
