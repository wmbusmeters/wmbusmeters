#!/bin/sh

PROG="$1"

mkdir -p testoutput

TEST=testoutput

TESTNAME="Test match on dll id"
TESTRESULT="ERROR"

cat simulations/simulation_dll_tpl_diff.txt | grep '^{' | jq --sort-keys . > $TEST/test_expected.txt
$PROG --format=json simulations/simulation_dll_tpl_diff.txt \
      Vatten hydrus 81818181 NOKEY \
      > $TEST/test_output.txt 2> $TEST/test_stderr.txt

if [ "$?" = "0" ]
then
    cat $TEST/test_output.txt | jq --sort-keys . | sed 's/"timestamp": "....-..-..T..:..:..Z"/"timestamp": "1111-11-11T11:11:11Z"/' > $TEST/test_responses.txt
    diff $TEST/test_expected.txt $TEST/test_responses.txt
    if [ "$?" = "0" ]
    then
        echo OK: $TESTNAME
        TESTRESULT="OK"
    else
            if [ "$USE_MELD" = "true" ]
            then
                meld $TEST/test_expected.txt $TEST/test_responses.txt
            fi
    fi
else
    echo "wmbusmeters returned error code: $?"
    cat $TEST/test_output.txt
    cat $TEST/test_stderr.txt
fi

if [ "$TESTRESULT" = "ERROR" ]
then
    echo ERROR: $TESTNAME
    exit 1
fi


TESTNAME="Test match on tpl id"
TESTRESULT="ERROR"

cat simulations/simulation_dll_tpl_diff.txt | grep '^{' | jq --sort-keys . > $TEST/test_expected.txt
$PROG --format=json simulations/simulation_dll_tpl_diff.txt \
      Vatten hydrus 56465646 NOKEY \
      > $TEST/test_output.txt 2> $TEST/test_stderr.txt

if [ "$?" = "0" ]
then
    cat $TEST/test_output.txt | jq --sort-keys . | sed 's/"timestamp": "....-..-..T..:..:..Z"/"timestamp": "1111-11-11T11:11:11Z"/' > $TEST/test_responses.txt
    diff $TEST/test_expected.txt $TEST/test_responses.txt
    if [ "$?" = "0" ]
    then
        echo OK: $TESTNAME
        TESTRESULT="OK"
    else
            if [ "$USE_MELD" = "true" ]
            then
                meld $TEST/test_expected.txt $TEST/test_responses.txt
            fi
    fi
else
    echo "wmbusmeters returned error code: $?"
    cat $TEST/test_output.txt
    cat $TEST/test_stderr.txt
fi

if [ "$TESTRESULT" = "ERROR" ]
then
    echo ERROR: $TESTNAME
    exit 1
fi
