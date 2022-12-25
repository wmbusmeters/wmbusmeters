#!/bin/bash

PROG="$1"

TEST=testoutput

rm -rf $TEST
mkdir -p $TEST

TESTNAME="Test using netcat to feed rtlwmbus stream using tcp."
TESTRESULT="ERROR"

PORT=$(( $RANDOM % 10000 + 30000 ))

rm -f $TEST/response
$PROG --silent --oneshot --format=fields --selectfields=total_m3 "rtlwmbus:CMD(nc -lkv $PORT)"  MyWater iperl 33225544 NOKEY >> $TEST/response 2>&1 &

sleep 0.5

echo "T1;1;1;2019-04-03 19:00:42.000;97;148;33225544;0x1844AE4C4455223368077A55000000041389E20100023B0000" | nc -q1 localhost $PORT

sleep 0.5

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
