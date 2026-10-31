#!/bin/sh

. tests/include.sh

PROG="$1"

mkdir -p testoutput
TEST=testoutput

TESTNAME="Test relative Type M date and time"
TESTRESULT="ERROR"

cat <<EOF2 > $TEST/demotm.xmq
/* Test driver demotm.xmq for the Type M date and time (CP_LVAR) of EN 13757-3:2018 table A.12.
   An Apator TAF7 telegram carries the base time of its compact profiles as
   CD 01 6D E2 03 30: storage 3, LVAR E2h = 2 bytes, header 30h = resolution 1 s
   and time offset -16, ie. a duration relative to the transmission of +3 s.
*/
driver {
    name = demotm
    meter_type = ElectricityMeter
    default_fields = name,id,base_time_offset_s,timestamp
    detect { mvt = APA,01,02 }
    fields {
        field { name = base_time_offset quantity = Time display_unit = s
                match { measurement_type = Instantaneous vif_range = DateTime storage_nr = 3 } }
    }
}
EOF2

$PROG --format=json C34401063412151601028C2003900F002C25FE0000003DAD298601B799D07A0310A007102F2F0E020958140000000E823C560601000000CD016DE20330CE0103804501000000CD01831330410800000000000000000000000000000000060808070808000000000000000000000000000000000000000000000000CE01833C651000000000CD0183BC1330410800010000000001000100000000000000000000000000000000000000000000000000000000000000000000000000C10174032F2F2F2F2F Apator $TEST/demotm.xmq 16151234 NOKEY | jq "del(.timestamp)" --sort-keys > $TEST/test_output.txt 2>&1

cat <<EOF2 > $TEST/test_expected.txt
{
  "_": "telegram",
  "base_time_offset_s": 3,
  "driver": "demotm",
  "id": "16151234",
  "media": "electricity",
  "name": "Apator"
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
