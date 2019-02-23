#!/bin/bash

PROG="$1"
TESTINTERNAL=$(dirname $PROG)/testinternals

$TESTINTERNAL
if [ "$?" = "0" ]; then
    echo Internal test OK
fi

tests/test_c1_meters.sh $PROG
tests/test_t1_meters.sh $PROG
tests/test_shell.sh $PROG
tests/test_meterfiles.sh $PROG
