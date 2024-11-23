#!/bin/sh

PROG="$1"
TEST=testoutput
mkdir -p $TEST

TESTNAME="Test alarms"
TESTRESULT="OK"

echo "RUNNING $TESTNAME ..."

> /tmp/wmbusmeters_telegram_test
> /tmp/wmbusmeters_alarm_test

$PROG --useconfig=tests/config7 --overridedevice=simulations/simulation_alarm.txt 2> $TEST/test_stderr.txt | sed 's/....-..-..T..:..:..Z/1111-11-11T11:11:11Z/' > $TEST/test_output.txt

echo "STDERR---------------------------------"
cat $TEST/test_stderr.txt
echo "STDOUT---------------------------------"
cat $TEST/test_output.txt
echo "TMP/OUTPUT-----------------------------"
cat /tmp/wmbusmeters_telegram_test
echo "TMP/ALARM------------------------------"
cat /tmp/wmbusmeters_alarm_test
echo "---------------------------------------"

cat > $TEST/test_expected.txt <<EOF
[ALARM DeviceInactivity] 4 seconds of inactivity resetting simulations/simulation_alarm.txt simulation (timeout 4s expected mon-sun(00-23) now 1111-11-11 11:11)
(wmbus) successfully reset wmbus device
EOF

cat > /tmp/wmbusmeters_telegram_expected <<EOF
METER =={"_":"telegram","media":"cold water","meter":"multical21","name":"Water","id":"76348799","external_temperature_c":19,"flow_temperature_c":127,"target_m3":6.408,"total_m3":6.408,"current_status":"DRY","status":"DRY","time_bursting":"","time_dry":"22-31 days","time_leaking":"","time_reversed":"","timestamp":"1111-11-11T11:11:11Z"}==
METER =={"_":"telegram","media":"cold water","meter":"multical21","name":"Water","id":"76348799","external_temperature_c":19,"flow_temperature_c":127,"target_m3":6.408,"total_m3":6.408,"current_status":"DRY","status":"DRY","time_bursting":"","time_dry":"22-31 days","time_leaking":"","time_reversed":"","timestamp":"1111-11-11T11:11:11Z"}==
EOF

cat > /tmp/wmbusmeters_alarm_expected <<EOF
ALARM_SHELL DeviceInactivity [ALARM DeviceInactivity] 4 seconds of inactivity resetting simulations/simulation_alarm.txt simulation (timeout 4s expected mon-sun(00-23) now 1111-11-11 11:11)
EOF

cat $TEST/test_stderr.txt | sed 's/now ....-..-.. ..:../now 1111-11-11 11:11/' > $TEST/test_responses.txt

REST=$(diff $TEST/test_responses.txt $TEST/test_expected.txt)

if [ ! -z "$REST" ]
then
    echo ERROR STDERR: $TESTNAME
    echo -----------------
    diff $TEST/test_responses.txt $TEST/test_expected.txt
    echo -----------------
    TESTRESULT="ERROR"
fi

cat /tmp/wmbusmeters_telegram_test | sed 's/"timestamp":"....-..-..T..:..:..Z"/"timestamp":"1111-11-11T11:11:11Z"/' > /tmp/wmbusmeters_telegram_output

REST=$(diff /tmp/wmbusmeters_telegram_expected /tmp/wmbusmeters_telegram_output)

if [ ! -z "$REST" ]
then
    echo ERROR TELEGRAMS: $TESTNAME
    echo -----------------
    diff /tmp/wmbusmeters_telegram_expected /tmp/wmbusmeters_telegram_output
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
