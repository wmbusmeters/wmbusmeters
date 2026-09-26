#!/bin/sh

. tests/include.sh

PROG="$1"
TEST=testoutput
mkdir -p $TEST

TESTNAME="Test alarms"
TESTRESULT="OK"

echo "RUNNING $TESTNAME ..."

# Kill any leftover wmbusmeters processes that might hold the files
pkill -f "wmbusmeters.*config7" 2>/dev/null || true
sleep 0.1

# Clean up files from previous runs
> $TEST/wmbusmeters_telegram_test
> $TEST/wmbusmeters_alarm_test

$PROG --useconfig=tests/config7 --overridedevice=simulations/simulation_alarm.txt 2> $TEST/test_stderr.txt | sed 's/....-..-..T..:..:..Z/1111-11-11T11:11:11Z/' > $TEST/test_output.txt

#echo "STDERR---------------------------------"
#cat $TEST/test_stderr.txt
#echo "STDOUT---------------------------------"
#cat $TEST/test_output.txt
#echo "TMP/OUTPUT-----------------------------"
#cat $TEST/wmbusmeters_telegram_test
#echo "TMP/ALARM------------------------------"
#cat $TEST/wmbusmeters_alarm_test
#echo "---------------------------------------"

cat > $TEST/test_expected.txt <<EOF
[ALARM DeviceInactivity] 4 seconds of inactivity resetting simulations/simulation_alarm.txt simulation (timeout 4s expected mon-sun(00-23) now 1111-11-11 11:11)
(wmbus) successfully reset wmbus device
EOF

cat > $TEST/wmbusmeters_telegram_expected <<EOF
METER =={"_":"telegram","media":"cold water","driver":"kamwater","name":"Water","id":"76348799","min_external_temperature_last_month_c":19,"min_flow_temperature_last_month_c":127,"target_m3":6.408,"total_m3":6.408,"current_status":"DRY","current_status_deprecated_by":"status, update before 2026-12-01","status":"DRY","time_bursting":"","time_dry":"22-31 days","time_leaking":"","time_reversed":"","timestamp":"1111-11-11T11:11:11Z"}==
METER =={"_":"telegram","media":"cold water","driver":"kamwater","name":"Water","id":"76348799","min_external_temperature_last_month_c":19,"min_flow_temperature_last_month_c":127,"target_m3":6.408,"total_m3":6.408,"current_status":"DRY","current_status_deprecated_by":"status, update before 2026-12-01","status":"DRY","time_bursting":"","time_dry":"22-31 days","time_leaking":"","time_reversed":"","timestamp":"1111-11-11T11:11:11Z"}==
EOF

cat > $TEST/wmbusmeters_alarm_expected <<EOF
ALARM_SHELL DeviceInactivity [ALARM DeviceInactivity] 4 seconds of inactivity resetting simulations/simulation_alarm.txt simulation (timeout 4s expected mon-sun(00-23) now 1111-11-11 11:11)
EOF

cat $TEST/test_stderr.txt | sed 's/now ....-..-.. ..:../now 1111-11-11 11:11/' > $TEST/test_responses.txt

REST=$(diff $TEST/test_responses.txt $TEST/test_expected.txt)

if [ ! -z "$REST" ]
then
    printERROR "STDERR check failed: $TESTNAME"
    echo -----------------
    diff $TEST/test_responses.txt $TEST/test_expected.txt
    echo -----------------
    if [ "$USE_MELD" = "true" ]
    then
        meld $TEST/test_responses.txt $TEST/test_expected.txt
    fi
    TESTRESULT="ERROR"
fi

cat $TEST/wmbusmeters_telegram_test | sed 's/"timestamp":"....-..-..T..:..:..Z"/"timestamp":"1111-11-11T11:11:11Z"/' > $TEST/wmbusmeters_telegram_output

REST=$(diff $TEST/wmbusmeters_telegram_expected $TEST/wmbusmeters_telegram_output)

if [ ! -z "$REST" ]
then
    printERROR "TELEGRAMS check failed: $TESTNAME"
    echo -----------------
    diff $TEST/wmbusmeters_telegram_expected $TEST/wmbusmeters_telegram_output
    echo -----------------
    if [ "$USE_MELD" = "true" ]
    then
        meld $TEST/wmbusmeters_telegram_expected $TEST/wmbusmeters_telegram_output
    fi
    TESTRESULT="ERROR"
fi

cat $TEST/wmbusmeters_alarm_test |  sed 's/....-..-.. ..:../1111-11-11 11:11/' > $TEST/wmbusmeters_alarm_output

REST=$(diff $TEST/wmbusmeters_alarm_expected $TEST/wmbusmeters_alarm_output)

if [ ! -z "$REST" ]
then
    printERROR "ALARM SHELLS check failed: $TESTNAME"
    echo -----------------
    diff $TEST/wmbusmeters_alarm_expected $TEST/wmbusmeters_alarm_output
    echo -----------------
    if [ "$USE_MELD" = "true" ]
    then
        meld $TEST/wmbusmeters_alarm_expected $TEST/wmbusmeters_alarm_output
    fi

    TESTRESULT="ERROR"
fi

if [ "$TESTRESULT" = "ERROR" ]; then printERROR "$TESTNAME"; exit 1; else printOK "$TESTNAME"; fi
