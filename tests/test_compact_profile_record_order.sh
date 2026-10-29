#!/bin/sh

. tests/include.sh

PROG="$1"

mkdir -p testoutput
TEST=testoutput

TESTNAME="Test compact profile with base records after the profile"
TESTRESULT="ERROR"

cat <<EOF2 > $TEST/demorecord.xmq
/* Test driver demorecord.xmq for an Apator TAF7 telegram. It carries two inverse compact
   profiles on storage 3 (import and export, 46 points of 8 s each). The base values come
   before the profiles, the actuality duration C1 01 74 03 (3 s) comes last of all.
   The time axis of every profile point needs that actuality duration.
*/
driver {
    name = demorecord
    meter_type = ElectricityMeter
    default_fields = name,id,timestamp
    detect { mvt = APA,01,02 }
    fields {
        field { name = 'consumption_at_{storage_counter-3counter}' quantity = Energy
                match { measurement_type = Instantaneous vif_range = AnyEnergyVIF
                        storage_nr = 4,5 add_combinable = Synthetic } }
        field { name = 'consumption_at_{storage_counter-3counter}' quantity = Energy
                match { measurement_type = Instantaneous vif_range = AnyEnergyVIF
                        storage_nr = 19,21 add_combinable = Synthetic } }
        field { name = 'consumption_at_{storage_counter-3counter}' quantity = Energy
                match { measurement_type = Instantaneous vif_range = AnyEnergyVIF
                        storage_nr = 49 add_combinable = Synthetic } }
        field { name = 'production_at_{storage_counter-3counter}' quantity = Energy
                match { measurement_type = Instantaneous vif_range = AnyEnergyVIF
                        storage_nr = 4,5 add_combinable = Synthetic add_combinable = BackwardFlow } }
        field { name = 'actuality_duration_at_{storage_counter-3counter}' quantity = Time display_unit = s
                match { measurement_type = Instantaneous vif_range = ActualityDuration
                        storage_nr = 4,5 add_combinable = Synthetic } }
        field { name = 'actuality_duration_at_{storage_counter-3counter}' quantity = Time display_unit = s
                match { measurement_type = Instantaneous vif_range = ActualityDuration
                        storage_nr = 49 add_combinable = Synthetic } }
    }
}
EOF2

$PROG --format=json C34401063412151601028C2003900F002C25FE0000003DAD298601B799D07A0310A007102F2F0E020958140000000E823C560601000000CD016DE20330CE0103804501000000CD01831330410800000000000000000000000000000000060808070808000000000000000000000000000000000000000000000000CE01833C651000000000CD0183BC1330410800010000000001000100000000000000000000000000000000000000000000000000000000000000000000000000C10174032F2F2F2F2F Apator $TEST/demorecord.xmq 16151234 NOKEY | jq "del(.timestamp)" --sort-keys > $TEST/test_output.txt 2>&1

cat <<EOF2 > $TEST/test_expected.txt
{
  "_": "telegram",
  "actuality_duration_at_1_s": 11,
  "actuality_duration_at_2_s": 19,
  "actuality_duration_at_46_s": 371,
  "consumption_at_16_kwh": 14.58,
  "consumption_at_17_kwh": 14.574,
  "consumption_at_18_kwh": 14.566,
  "consumption_at_1_kwh": 14.58,
  "consumption_at_2_kwh": 14.58,
  "consumption_at_46_kwh": 14.535,
  "driver": "demorecord",
  "id": "16151234",
  "media": "electricity",
  "name": "Apator",
  "production_at_1_kwh": 1.065,
  "production_at_2_kwh": 1.064
}
EOF2

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
