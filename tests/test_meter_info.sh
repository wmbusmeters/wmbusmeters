#!/bin/sh

. tests/include.sh

PROG="$1"

mkdir -p testoutput
TEST=testoutput

TESTNAME="Test info in details"
TESTRESULT="ERROR"

$PROG --addtelegramdetails --addtelegramstructured --format=json 31444514e301000001028c20807a2e002025e25dea4f2bc8ec94920260443a95c2bf3856e3e380e889cc8233ca5d9771f289 Golvet ebeco500 000001e3 FCC9B808216D9D1F5FF6E1A3CF7D3084 | jq . --sort-keys | grep -v timestamp > $TEST/test_output.txt 2>&1

cat <<EOF > $TEST/test_expected.txt
{
  "_": "telegram",
  "details": {
    "fields": {
      "external_temperature_c": {
        "change": "Sample",
        "info": "Temperature outside of meter.",
        "quantity": "Temperature",
        "unit": "c"
      },
      "meter_datetime": {
        "info": "Date and time when the meter sent the telegram.",
        "quantity": "Text"
      },
      "on_time_h": {
        "change": "Sample",
        "info": "How long the meter has been powered up.",
        "quantity": "Time",
        "unit": "h"
      },
      "status": {
        "flags": [
          "ALARM",
          "BUSY",
          "ERROR",
          "FLOOR_OC",
          "FLOOR_SC",
          "MAX_LIMIT",
          "MIN_LIMIT",
          "OPEN_WINDOW",
          "PERMANENT_ERROR",
          "POWER_CTRL",
          "POWER_LOW",
          "POWER_OFF",
          "RELAY_OC",
          "RELAY_OT",
          "RELAY_SC",
          "RELAY_TEST",
          "ROOM_OC",
          "ROOM_SC",
          "STARTUP",
          "SYSTEM",
          "TEMPORARY",
          "TEMPORARY_ERROR"
        ],
        "info": "Thermostat error and status flags.",
        "quantity": "Flags"
      },
      "total_kwh": {
        "change": "Increasing",
        "info": "The total {media} energy recorded.",
        "quantity": "Energy",
        "unit": "kwh"
      }
    },
    "flags": {
      "ALARM": "Abnormal condition/alarm. (tpl sts)",
      "BUSY": "Meter busy. (tpl sts)",
      "ERROR": "Meter error; details via error reporting. (tpl sts)",
      "PERMANENT_ERROR": "Permanent meter error. (tpl sts)",
      "POWER_LOW": "Power low. (tpl sts)",
      "TEMPORARY_ERROR": "Temporary meter error. (tpl sts)"
    },
    "info": "Underfloor Heating Thermostat",
    "mvt": "EBE,01,02"
  },
  "driver": "ebeco500",
  "external_temperature_c": 22.8,
  "id": "000001e3",
  "media": "electricity",
  "meter_datetime": "2000-02-01 13:54:50",
  "name": "Golvet",
  "on_time_h": 105.633333,
  "status": "OK",
  "structured": {
    "status_flags": {
      "ALARM": false,
      "BUSY": false,
      "ERROR": false,
      "FLOOR_OC": false,
      "FLOOR_SC": false,
      "MAX_LIMIT": false,
      "MIN_LIMIT": false,
      "OPEN_WINDOW": false,
      "PERMANENT_ERROR": false,
      "POWER_CTRL": false,
      "POWER_LOW": false,
      "POWER_OFF": false,
      "RELAY_OC": false,
      "RELAY_OT": false,
      "RELAY_SC": false,
      "RELAY_TEST": false,
      "ROOM_OC": false,
      "ROOM_SC": false,
      "STARTUP": false,
      "SYSTEM": false,
      "TEMPORARY": false,
      "TEMPORARY_ERROR": false
    },
    "status_lookups_unknowns": {
      "ERROR_FLAGS": 0,
      "TPL_STS": 0
    }
  },
  "total_kwh": 68.6
}
EOF

if [ "$?" = "0" ]
then
    diff $TEST/test_expected.txt $TEST/test_output.txt
    if [ "$?" = "0" ]
    then
        printOK "$TESTNAME"
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
    printERROR "$TESTNAME"
    exit 1
fi
