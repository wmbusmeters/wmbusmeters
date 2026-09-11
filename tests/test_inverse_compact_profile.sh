#!/bin/sh

PROG="$1"

mkdir -p testoutput
TEST=testoutput

TESTNAME="Test inverse compact profile"
TESTRESULT="ERROR"

cat <<EOF > $TEST/democp.xmq
/* Test driver democp.xmq for inverse compact profile.
   The compact profile from OMS Spec Vol.2, Annex G,
   Table G.6: a water meter, base value 1013 l on 2008-05-31,
   four monthly increments.
   Table G.7 of the same document prints what it has to expand to — 755, 423, 209 and 65 litres.
*/
driver {
    name = democp
    meter_type = WaterMeter
    default_fields = name,id,total_m3,timestamp
    library { use = meter_date }
    detect { mvt = MAD,01,07 }
    fields {
        field { name = total quantity = Volume
                match { measurement_type = Instantaneous vif_range = Volume storage_nr = 0 } }

        field { name = 'history_{storage_counter}' quantity = Volume
                match { measurement_type = Instantaneous vif_range = Volume
                        storage_nr = 1,4 add_combinable = Synthetic } }

        field { name = 'history_{storage_counter}' quantity = PointInTime display_unit = date
                match { measurement_type = Instantaneous vif_range = Date
                        storage_nr = 1,4 add_combinable = Synthetic } }

    }
}
EOF

$PROG --format=json 264424340102030401077a010000200C1313100000026C1F150D93130A7AFE5802320314024401 MAD $TEST/democp.xmq ANYID NOKEY | jq . --sort-keys | grep -v timestamp > $TEST/test_output.txt 2>&1

cat <<EOF > $TEST/test_expected.txt
{
  "_": "telegram",
  "driver": "democp",
  "history_1_date": "2008-04-30",
  "history_1_m3": 0.755,
  "history_2_date": "2008-03-31",
  "history_2_m3": 0.423,
  "history_3_date": "2008-02-29",
  "history_3_m3": 0.209,
  "history_4_date": "2008-01-31",
  "history_4_m3": 0.065,
  "id": "04030201",
  "media": "water",
  "meter_date": "2008-05-31",
  "name": "MAD",
  "total_m3": 1.013
}
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
