#!/bin/sh

PROG="$1"
TESTINTERNAL=$(dirname $PROG)/testinternals

if [ ! -x $PROG ]
then
    echo No such executable \"$PROG\"
    exit 1
fi

if ! command -v jq > /dev/null
then
    echo "You have to install jq !"
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

tests/test_s1_meters.sh $PROG
if [ "$?" != "0" ]; then RC="1"; fi

tests/test_non_existant_driver.sh $PROG
if [ "$?" != "0" ]; then RC="1"; fi

tests/test_mbus.sh $PROG
if [ "$?" != "0" ]; then RC="1"; fi

tests/test_anyid.sh $PROG
if [ "$?" != "0" ]; then RC="1"; fi

./tests/test_list_envs.sh $PROG
if [ "$?" != "0" ]; then RC="1"; fi

#tests/test_unknown.sh $PROG
#if [ "$?" != "0" ]; then RC="1"; fi

tests/test_apas.sh $PROG
if [ "$?" != "0" ]; then RC="1"; fi

tests/test_izars.sh $PROG
if [ "$?" != "0" ]; then RC="1"; fi

tests/test_aes.sh $PROG
if [ "$?" != "0" ]; then RC="1"; fi

tests/test_key_warnings.sh $PROG
if [ "$?" != "0" ]; then RC="1"; fi

tests/test_shell.sh $PROG
if [ "$?" != "0" ]; then RC="1"; fi

tests/test_shell2.sh $PROG
if [ "$?" != "0" ]; then RC="1"; fi

tests/test_shell_env.sh $PROG
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

tests/test_calculate.sh $PROG
if [ "$?" != "0" ]; then RC="1"; fi

tests/test_formulas.sh $PROG
if [ "$?" != "0" ]; then RC="1"; fi

tests/test_formula_errors.sh $PROG
if [ "$?" != "0" ]; then RC="1"; fi

tests/test_calculate_dates.sh $PROG
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

tests/test_cmdline.sh $PROG
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

./tests/test_match_dll_and_tpl_id.sh $PROG
if [ "$?" != "0" ]; then RC="1"; fi

./tests/test_unix_timestamp.sh $PROG
if [ "$?" != "0" ]; then
    # This can spuriously fail if it crosses a second change...
    # Lets try again.
    ./tests/test_unix_timestamp.sh $PROG
    if [ "$?" != "0" ]; then
        RC="1";
    fi
fi

./tests/test_log_timestamps.sh $PROG
if [ "$?" != "0" ]; then RC="1"; fi

./tests/test_meter_extras.sh $PROG
if [ "$?" != "0" ]; then RC="1"; fi

./tests/test_hex_cmdline.sh $PROG
if [ "$?" != "0" ]; then RC="1"; fi

./tests/test_removing_dll_crcs.sh $PROG
if [ "$?" != "0" ]; then RC="1"; fi

./tests/test_broken.sh $PROG
if [ "$?" != "0" ]; then RC="1"; fi

./tests/test_rtlwmbus_crc_errors.sh $PROG
if [ "$?" != "0" ]; then RC="1"; fi

./tests/test_rtlwmbus_timestamps.sh $PROG
if [ "$?" != "0" ]; then RC="1"; fi

./tests/test_drivers.sh $PROG
if [ "$?" != "0" ]; then RC="1"; fi

./tests/test_analyze.sh $PROG
if [ "$?" != "0" ]; then RC="1"; fi

./tests/test_loadable_drivers.sh $PROG
if [ "$?" != "0" ]; then RC="1"; fi

./tests/test_bad_driver.sh $PROG
if [ "$?" != "0" ]; then RC="1"; fi

if [ -x ../additional_tests.sh ]
then
    (cd ..; ./additional_tests.sh $PROG)
fi

# Only run the netcat tests if netcat is installed.
if command -v nc > /dev/null 2> /dev/null
then
    IS_NC_OPENBSD=$(nc -help 2>&1 | grep -o OpenBSD)
    # These tests only work with netcat-openbsd.
    if [ "$IS_NC_OPENBSD" = "OpenBSD" ]
    then
        ./tests/test_nc1.sh $PROG
        if [ "$?" != "0" ]; then RC="1"; fi

        ./tests/test_nc2.sh $PROG
        if [ "$?" != "0" ]; then RC="1"; fi

        ./tests/test_nc3.sh $PROG
        if [ "$?" != "0" ]; then RC="1"; fi
    fi
fi

echo Slower tests...

tests/test_pipe.sh $PROG
if [ "$?" != "0" ]; then RC="1"; fi

tests/test_config_oneshot_exitafter.sh $PROG
if [ "$?" != "0" ]; then RC="1"; fi

tests/test_config_overrides.sh $PROG
if [ "$?" != "0" ]; then RC="1"; fi

if [ "$(uname)" = "Linux" ]
then
    tests/test_alarm.sh $PROG
    if [ "$?" != "0" ]; then RC="1"; fi
fi

if [ "$RC" = "0" ]
then
    echo "All tests ok!"
else
    echo "Some tests failed!"
fi

exit $RC
