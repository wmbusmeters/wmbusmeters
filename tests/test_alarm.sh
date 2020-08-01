#!/bin/sh

PROG="$1"
TEST=testoutput
mkdir -p $TEST

TESTNAME="Test alarms"
TESTRESULT="OK"

echo "RUNNING $TESTNAME ..."

> /tmp/wmbusmeters_telegram_test
> /tmp/wmbusmeters_alarm_test

$PROG --useconfig=tests/config7 --device=simulations/simulation_alarm.txt | sed 's/....-..-.. ..:../1111-11-11 11:11/' > $TEST/test_output.txt

cat > $TEST/test_expected.txt <<EOF
(alarm) PROTOCOL_ERROR Hit timeout(1 s) and 1111-11-11 11:11 is within expected activity (mon-sun(00-23))! Now 2 seconds since last telegram was received! Resetting device DEVICE_SIMULATOR simulations/simulation_alarm.txt!
(wmbus) successfully reset wmbus device
EOF

cat > /tmp/wmbusmeters_telegram_expected <<EOF
METER =={"media":"cold water","meter":"multical21","name":"Water","id":"76348799","total_m3":6.408,"target_m3":6.408,"max_flow_m3h":0,"flow_temperature_c":127,"external_temperature_c":19,"current_status":"DRY","time_dry":"22-31 days","time_reversed":"","time_leaking":"","time_bursting":"","timestamp":"1111-11-11T11:11:11Z"}==
METER =={"media":"cold water","meter":"multical21","name":"Water","id":"76348799","total_m3":6.408,"target_m3":6.408,"max_flow_m3h":0,"flow_temperature_c":127,"external_temperature_c":19,"current_status":"DRY","time_dry":"22-31 days","time_reversed":"","time_leaking":"","time_bursting":"","timestamp":"1111-11-11T11:11:11Z"}==
EOF

cat > /tmp/wmbusmeters_alarm_expected <<EOF
ALARM_SHELL PROTOCOL_ERROR Hit timeout(1 s) and 1111-11-11 11:11 is within expected activity (mon-sun(00-23))! Now 2 seconds since last telegram was received! Resetting device DEVICE_SIMULATOR simulations/simulation_alarm.txt!
EOF

REST=$(diff $TEST/test_output.txt $TEST/test_expected.txt)

if [ ! -z "$REST" ]
then
    echo ERROR STDOUT: $TESTNAME
    echo -----------------
    diff $TEST/test_output.txt $TEST/test_expected.txt
    echo -----------------
    TESTRESULT="ERROR"
fi

cat /tmp/wmbusmeters_telegram_test | sed 's/"timestamp":"....-..-..T..:..:..Z"/"timestamp":"1111-11-11T11:11:11Z"/' > /tmp/wmbusmeters_telegram_output

REST=$(diff /tmp/wmbusmeters_telegram_expected /tmp/wmbusmeters_telegram_output)

if [ ! -z "$REST" ]
then
    echo ERROR TELEGRAMS: $TESTNAME
    echo -----------------
    diff /tmp/wmbusmeters_telegram_expected /tmp/wmbusmeters_telegram
    echo -----------------
    TESTRESULT="ERROR"
fi

cat /tmp/wmbusmeters_alarm_test |  sed 's/....-..-.. ..:../1111-11-11 11:11/' > /tmp/wmbusmeters_alarm_output

REST=$(diff /tmp/wmbusmeters_alarm_expected /tmp/wmbusmeters_alarm_output)

if [ ! -z "$REST" ]
then
    echo ERROR ALARM SHELLS: $TESTNAME
    echo -----------------
    diff /tmp/wmbusmeters_alarm_expected /tmp/wmbusmeters_alarm_output
    echo -----------------
    TESTRESULT="ERROR"
fi

if [ "$TESTRESULT" = "ERROR" ]; then echo ERROR: $TESTNAME;  exit 1; else echo "OK: $TESTNAME"; fi
