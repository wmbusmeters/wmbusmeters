#!/bin/bash

if nc 2>&1 | grep -q apple
then
    exit 0
fi

PROG="$1"

TEST=testoutput

rm -rf $TEST
mkdir -p $TEST

TESTNAME="Test using netcat to feed rawtty stream using tcp."
TESTRESULT="ERROR"

PORT=$(( $RANDOM % 10000 + 30000 ))

rm -f $TEST/response
$PROG --silent --oneshot --exitafter=5s --format=fields --selectfields=total_m3 "rawtty:CMD(nc -lk $PORT)"  MyWater iperl 33225544 NOKEY >> $TEST/response 2>&1 &

sleep 0.5

echo -n "1844AE4C4455223368077A55000000041389E20100023B0000" | xxd -r -p | nc -q1 localhost $PORT

GOT=$(cat $TEST/response | tr -d '\n')
EXPECTED="123.529"

if [ "$GOT" = "$EXPECTED" ]
then
    echo "OK: $TESTNAME"
    TESTRESULT="OK"
else
    echo "ERROR: GOT $GOT but expected $EXPECTED"
fi

if [ "$TESTRESULT" = "ERROR" ]; then echo ERROR: $TESTNAME;  exit 1; fi
