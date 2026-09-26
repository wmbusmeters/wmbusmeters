#!/bin/bash

TEST=build/test

PROG="$1"
DRIVER="$2"
ARGS="$3"
HEX="$4"
JSON="$5"
FIELDS="$6"

export TZ=UTC

DRIVER_NAME=$(echo -n "$DRIVER" | cut -f 2 -d / | cut -f 1 -d .)

rm -f $TEST/test_output.txt $TEST/test_expected.txt $TEST/simulation_tmp.txt

echo "$HEX" | sed 's/^/telegram=/g' | sed 's/,/\ntelegram=/g' > $TEST/simulation_tmp.txt

AUTO_DRIVER_1=$($PROG $TEST/simulation_tmp.txt 2>&1 | grep driver: | head -n 1 | cut -f 2 -d : | tr -s ' ' | cut -f 2 -d ' ' | tr -d ' ' )

AUTO_DRIVER_2=$($PROG $TEST/simulation_tmp.txt 2>&1 | grep driver: | head -n 1 | cut -f 2 -d : | tr -s ' ' | cut -f 3 -d ' ' | tr -d ' ' )

if [ "$AUTO_DRIVER_1" != "$DRIVER_NAME" ] && [ "$AUTO_DRIVER_2" != "$DRIVER_NAME" ]
then
    cat $TEST/simulation_tmp.txt
    $PROG $TEST/simulation_tmp.txt
    echo "Error: telegram should have reported auto driver $DRIVER_NAME but reported $AUTO_DRIVER_1 $AUTO_DRIVER_2"
    exit 1
fi

$PROG --driver=$DRIVER --format=json $TEST/simulation_tmp.txt $ARGS 2> $TEST/test_error.txt \
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
fi

rm -f $TEST/test_output.txt $TEST/test_expected.txt

$PROG --driver=$DRIVER --format=fields $TEST/simulation_tmp.txt $ARGS 2> $TEST/test_error.txt \
    | sed 's/....-..-.. ..:..:../1111-11-11 11:11.11/' \
    | tail -n 1 \
     > $TEST/test_output.txt

echo "$FIELDS" > $TEST/test_expected.txt

if ! diff $TEST/test_expected.txt $TEST/test_output.txt
then
    if [ "$USE_MELD" = "true" ]
    then
        meld $TEST/test_expected.txt $TEST/test_output.txt
    fi
fi
