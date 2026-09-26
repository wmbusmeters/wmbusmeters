#!/bin/sh

. tests/include.sh

PROG="$1"
TEST=testoutput
mkdir -p $TEST

TESTNAME="Test driver deprecated by"
TESTRESULT="ERROR"

cat > $TEST/driver.xmq <<EOF
driver {
    name           = foo
    deprecated_by  = 'foo2 upgrade before 2027-01-01'
    meter_type     = WaterMeter
    default_fields = name,id,total_m3,max_flow_m3h,timestamp
    fields {
        field {
            name     = total
            deprecated_by = 'totality get rid of this'
            quantity = Volume
            change   = Increasing
            info     = 'The total water consumption.'
            match {
                measurement_type = Instantaneous
                vif_range        = Volume
            }
        }
    }
}
EOF

cat > $TEST/test_expected_unsorted.txt <<EOF
{
  "_": "telegram",
  "details": {
    "fields": {
      "total_m3": {
        "change": "Increasing",
        "deprecated_by": "totality get rid of this",
        "info": "The total water consumption.",
        "quantity": "Volume",
        "unit": "m3"
      }
    },
    "mvt": "SEN,68,07"
  },
  "driver": "foo",
  "driver_deprecated_by": "foo2 upgrade before 2027-01-01",
  "id": "33225544",
  "media": "water",
  "name": "WaterWater",
  "timestamp": "1111-11-11T11:11:11Z",
  "total_m3": 123.529,
  "total_m3_deprecated_by": "totality get rid of this"
}
EOF

jq --sort-keys . $TEST/test_expected_unsorted.txt > $TEST/test_expected.txt

$PROG --addtelegramdetails --format=json 1844AE4C4455223368077A55000000_041389E20100023B0000 WaterWater $TEST/driver.xmq 33225544 NOKEY 2> $TEST/test_stderr.txt | jq --sort-keys . > $TEST/test_output.txt

if [ "$?" = "0" ]
then
    cat $TEST/test_output.txt | sed 's/"timestamp": "....-..-..T..:..:..Z"/"timestamp": "1111-11-11T11:11:11Z"/' > $TEST/test_responses.txt
    diff $TEST/test_expected.txt $TEST/test_responses.txt
    if [ "$?" = "0" ]
    then
        printOK "$TESTNAME"
        TESTRESULT="OK"
    else
        printERROR "$TESTNAME"
        exit 1
    fi
else
    printERROR "$TESTNAME"
    echo "wmbusmeters returned error code: $?"
    cat $TEST/test_output.txt
    cat $TEST/test_stderr.txt
    exit 1
fi
