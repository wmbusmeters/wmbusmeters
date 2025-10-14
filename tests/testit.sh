#!/bin/bash

TEST=build/test
mkdir -p $TEST

PROG="$1"
ARGS="$2"
HEX="$3"
JSON="$4"
FIELDS="$5"

OK=true

rm -f $TEST/test_output.txt $TEST/test_expected.txt $TEST/simulation_tmp.txt

echo "$HEX" | sed 's/^/telegram=/g' | sed 's/,/\ntelegram=/g' > $TEST/simulation_tmp.txt

$PROG --format=json $TEST/simulation_tmp.txt $ARGS \
    | tail -n 1 \
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

$PROG --format=fields $TEST/simulation_tmp.txt $ARGS \
    | tail -n 1 \
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
    echo "OK: db test $(echo "$ARGS" | cut -f 1,2 -d ' ')"
else
    echo "ERROR: $ARGS $TELEGRAM"
fi
