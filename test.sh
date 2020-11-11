#!/bin/sh

PROG="$1"
TESTINTERNAL=$(dirname $PROG)/testinternals

if [ ! -x $PROG ]
then
    echo No such executable \"$PROG\"
    exit 1
fi

$TESTINTERNAL
if [ "$?" = "0" ]; then
    echo OK: test internals
fi
RC="0"

tests/test_c1_meters.sh $PROG
if [ "$?" != "0" ]; then RC="1"; fi

tests/test_t1_meters.sh $PROG
if [ "$?" != "0" ]; then RC="1"; fi

tests/test_unknown.sh $PROG
if [ "$?" != "0" ]; then RC="1"; fi

tests/test_apas.sh $PROG
if [ "$?" != "0" ]; then RC="1"; fi

tests/test_aes.sh $PROG
if [ "$?" != "0" ]; then RC="1"; fi

tests/test_shell.sh $PROG
if [ "$?" != "0" ]; then RC="1"; fi

tests/test_shell2.sh $PROG
if [ "$?" != "0" ]; then RC="1"; fi

tests/test_meterfiles.sh $PROG
if [ "$?" != "0" ]; then RC="1"; fi

tests/test_config1.sh $PROG
if [ "$?" != "0" ]; then RC="1"; fi

tests/test_logfile.sh $PROG
if [ "$?" != "0" ]; then RC="1"; fi

tests/test_elements.sh $PROG
if [ "$?" != "0" ]; then RC="1"; fi

tests/test_listen_to_all.sh $PROG
if [ "$?" != "0" ]; then RC="1"; fi

tests/test_driver_detection.sh $PROG
if [ "$?" != "0" ]; then RC="1"; fi

tests/test_multiple_ids.sh $PROG
if [ "$?" != "0" ]; then RC="1"; fi

tests/test_conversions.sh $PROG
if [ "$?" != "0" ]; then RC="1"; fi

tests/test_fields.sh $PROG
if [ "$?" != "0" ]; then RC="1"; fi

tests/test_oneshot.sh $PROG broken test
if [ "$?" != "0" ]; then RC="1"; fi

tests/test_wrongkeys.sh $PROG
if [ "$?" != "0" ]; then RC="1"; fi

tests/test_config4.sh $PROG
if [ "$?" != "0" ]; then RC="1"; fi

tests/test_linkmodes.sh $PROG
if [ "$?" != "0" ]; then RC="1"; fi

tests/test_additional_json.sh $PROG
if [ "$?" != "0" ]; then RC="1"; fi

tests/test_rtlwmbus.sh $PROG
if [ "$?" != "0" ]; then RC="1"; fi

tests/test_stdin_and_file.sh $PROG
if [ "$?" != "0" ]; then RC="1"; fi

tests/test_serial_bads.sh $PROG
if [ "$?" != "0" ]; then RC="1"; fi

tests/test_ignore_duplicates.sh $PROG
if [ "$?" != "0" ]; then RC="1"; fi

tests/test_pipe.sh $PROG
if [ "$?" != "0" ]; then RC="1"; fi

if [ "$(uname)" = "Linux" ]
then
    tests/test_alarm.sh $PROG
    if [ "$?" != "0" ]; then RC="1"; fi
fi

exit $RC
