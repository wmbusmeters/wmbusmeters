#!/bin/sh

PROG="$1"

mkdir -p testoutput
TEST=testoutput

TESTNAME="Test list-envs and list-fields"
TESTRESULT="ERROR"

$PROG --listenvs=amiplus | sort > $TEST/test_output.txt 2>&1

cat <<EOF | sort > $TEST/test_expected.txt
METER_JSON
METER_ID
METER_NAME
METER_MEDIA
METER_TYPE
METER_TIMESTAMP
METER_TIMESTAMP_UTC
METER_TIMESTAMP_UT
METER_TIMESTAMP_LT
METER_DEVICE
METER_DEVICE_DATE_TIME
METER_RSSI_DBM
METER_TOTAL_ENERGY_CONSUMPTION_KWH
METER_CURRENT_POWER_CONSUMPTION_KW
METER_TOTAL_ENERGY_PRODUCTION_KWH
METER_CURRENT_POWER_PRODUCTION_KW
METER_VOLTAGE_AT_PHASE_1_Volt
METER_VOLTAGE_AT_PHASE_2_Volt
METER_VOLTAGE_AT_PHASE_3_Volt
METER_TOTAL_ENERGY_CONSUMPTION_TARIFF_1_KWH
METER_TOTAL_ENERGY_CONSUMPTION_TARIFF_2_KWH
METER_TOTAL_ENERGY_CONSUMPTION_TARIFF_3_KWH
METER_TOTAL_ENERGY_PRODUCTION_TARIFF_1_KWH
METER_TOTAL_ENERGY_PRODUCTION_TARIFF_2_KWH
METER_TOTAL_ENERGY_PRODUCTION_TARIFF_3_KWH
EOF

if [ "$?" = "0" ]
then
    diff $TEST/test_expected.txt $TEST/test_output.txt
    if [ "$?" = "0" ]
    then
        echo OK: $TESTNAME
        TESTRESULT="OK"
    else
        if [ "$USE_MELD" = "true" ]
        then
            meld $TEST/test_expected.txt $TEST/test_output.txt
        fi
    fi
fi

if [ "$TESTRESULT" = "ERROR" ]
then
    echo ERROR: $TESTNAME
    exit 1
fi

$PROG --listfields=amiplus | sort > $TEST/test_output.txt 2>&1

cat <<EOF | sort > $TEST/test_expected.txt
                                   id  The meter id number.
                                 name  Your name for the meter.
                                media  What does the meter measure?
                                meter  Meter driver.
                            timestamp  Timestamp when wmbusmeters received the telegram. Local time for hr/fields UTC for json.
                         timestamp_ut  Unix timestamp when wmbusmeters received the telegram.
                         timestamp_lt  Local time when wmbusmeters received the telegram.
                        timestamp_utc  UTC time when wmbusmeters received the telegram.
                               device  The wmbus device that received the telegram.
                             rssi_dbm  The rssi for the received telegram as reported by the device.
         total_energy_consumption_kwh  The total energy consumption recorded by this meter.
         current_power_consumption_kw  Current power consumption.
          total_energy_production_kwh  The total energy production recorded by this meter.
          current_power_production_kw  Current power production.
                 voltage_at_phase_1_v  Voltage at phase L1.
                 voltage_at_phase_2_v  Voltage at phase L2.
                 voltage_at_phase_3_v  Voltage at phase L3.
                     device_date_time  Device date time.
total_energy_consumption_tariff_1_kwh  The total energy consumption recorded by this meter on tariff 1.
total_energy_consumption_tariff_2_kwh  The total energy consumption recorded by this meter on tariff 2.
total_energy_consumption_tariff_3_kwh  The total energy consumption recorded by this meter on tariff 3.
 total_energy_production_tariff_1_kwh  The total energy production recorded by this meter on tariff 1.
 total_energy_production_tariff_2_kwh  The total energy production recorded by this meter on tariff 2.
 total_energy_production_tariff_3_kwh  The total energy production recorded by this meter on tariff 3.
EOF

if [ "$?" = "0" ]
then
    diff $TEST/test_expected.txt $TEST/test_output.txt
    if [ "$?" = "0" ]
    then
        echo OK: $TESTNAME
        TESTRESULT="OK"
    else
        if [ "$USE_MELD" = "true" ]
        then
            meld $TEST/test_expected.txt $TEST/test_output.txt
        fi
    fi
fi

if [ "$TESTRESULT" = "ERROR" ]
then
    echo ERROR: $TESTNAME
    exit 1
fi
