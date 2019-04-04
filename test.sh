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
tests/test_config1.sh $PROG
tests/test_logfile.sh $PROG
tests/test_elements.sh $PROG
tests/test_listen_to_all.sh $PROG
tests/test_multiple_ids.sh $PROG
#tests/test_oneshot.sh $PROG broken test
tests/test_wrongkeys.sh $PROG
