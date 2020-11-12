#!/bin/sh

PROG="$1"

if [ "$PROG" = "" ]
then
    echo Please supply the binary to be tested as the first argument.
    exit 1
fi

TEST=testoutput
LOGFILE=$TEST/logfile

TESTNAME="Test detection of drivers for all telegrams"
TESTRESULT="ERROR"

mkdir -p $TEST
rm -f $LOGFILE

$PROG simulations/simulation_driver_detection.txt 2> $LOGFILE

if [ "$?" != "0" ]
then
    echo "ERROR: $TESTNAME"
    exit 1
fi

RES=$(grep unknown $LOGFILE)

if [ ! -z "$RES"  ]
then
    echo "ERROR: $TESTNAME"
    echo "Found unknown driver!"
    echo -----------------
    cat $LOGFILE
    echo -----------------
    exit 1
else
    echo OK: $TESTNAME
fi
