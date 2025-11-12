#!/bin/sh

PROG="$1"

if [ "$PROG" = "" ]
then
    echo Please supply the binary to be tested as the first argument.
    exit 1
fi

TEST=testoutput

rm -rf $TEST
mkdir -p $TEST

TESTNAME="Test config xmq mvt override triggers change of driver from iperl to foo"
TESTRESULT="ERROR"

$PROG --useconfig=tests/config15 | jq . --sort-keys > $TEST/test_output.txt

METER=$(cat $TEST/test_output.txt | grep -o '"meter": "foo"')

if ! grep -q '"meter": "foo"' $TEST/test_output.txt
then
    echo "ERROR: $TESTNAME ($0)"
    cat $TEST/test_output.txt
    echo "Expected driver foo"
    exit 1
fi

echo "OK: $TESTNAME"

TESTNAME="Test config xmq warning when using removed driver iperl"
TESTRESULT="ERROR"

$PROG --useconfig=tests/config16 > $TEST/test_output.txt 2>&1

if ! grep -q 'triggered a removal' $TEST/test_output.txt
then
    echo "ERROR: $TESTNAME ($0)"
    cat $TEST/test_output.txt
    echo "Expected driver iperl to not exist"
    exit 1
fi

echo "OK: $TESTNAME"

TESTNAME="Test overriden iperl driver"
TESTRESULT="ERROR"

$PROG --useconfig=tests/config17| jq . --sort-keys > $TEST/test_output.txt

if grep -q 'max_flow_m3h' $TEST/test_output.txt
then
    echo "ERROR: $TESTNAME ($0)"
    cat $TEST/test_output.txt
    echo "Expected driver iperl to have been overridden with less capable driver without max_flow_m3h"
    exit 1
fi

echo "OK: $TESTNAME"
