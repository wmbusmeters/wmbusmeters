#!/bin/sh

. tests/include.sh

PROG="$1"

mkdir -p testoutput
TEST=testoutput

TESTNAME="Test meter_shell"
TESTRESULT="OK"

cat > $TEST/telegram_expected.txt <<EOF
{"_":"telegram","details":{"fields":{"max_flow_m3h":{"change":"Instant","info":"The maximum water flow recorded during previous period.","quantity":"Flow","unit":"m3h"},"status":{"info":"Meter status including the tpl status.","quantity":"Text"},"total_m3":{"change":"Increasing","info":"The total water consumption.","quantity":"Volume","unit":"m3"}},"mvt":"SEN,68,07"},"driver":"iperl","id":"33225544","max_flow_m3h":0,"media":"water","name":"WaterWater","status":"OK","timestamp":"1111-11-11T11:11:11Z","total_m3":123.529}
EOF

cat > $TEST/driver_expected.txt <<EOF
driver{name=iperl meter_type=WaterMeter default_fields=name,id,total_m3,max_flow_m3h,timestamp manufacturer=Sensus model='Sensus iPERL'detect{mvt=SEN,68,06 mvt=SEN,68,07 mvt=SEN,7c,07}fields{field{name=status quantity=Text info='Meter status including the tpl status.'attributes=STATUS,INCLUDE_TPL_STATUS}field{name=total quantity=Volume change=Increasing info='The total water consumption.'match{measurement_type=Instantaneous vif_range=Volume}}field{name=max_flow quantity=Flow change=Instant info='The maximum water flow recorded during previous period.'match{measurement_type=Instantaneous vif_range=VolumeFlow}}}}
EOF

$PROG --format=fields --selectfields=total_m3 --telegramdetails=first \
      --metershell='echo "$METER_JSON"; echo "$METER_DRIVER"' \
      1844AE4C4455223368077A55000000_041389E20100023B0000 WaterWater iperl 33225544 NOKEY > $TEST/output.txt 2>&1

if [ "$?" = "0" ]
then
    head -n 1 $TEST/output.txt | jq -c --sort-keys . | sed 's/"timestamp":"....-..-..T..:..:..Z"/"timestamp":"1111-11-11T11:11:11Z"/'> $TEST/telegram_output.txt
    head -n 2 $TEST/output.txt | tail -n 1 > $TEST/driver_output.txt

    diff $TEST/telegram_expected.txt $TEST/telegram_output.txt
    if [ "$?" = "0" ]
    then
        printOK "$TESTNAME got json"
    else
        TESTRESULT=ERROR
        if [ "$USE_MELD" = "true" ]
        then
            meld $TEST/telegram_expected.txt $TEST/telegram_output.txt
        fi
    fi

    diff $TEST/driver_expected.txt $TEST/driver_output.txt
    if [ "$?" = "0" ]
    then
        printOK "$TESTNAME got driver"
    else
        TESTRESULT=ERROR
        if [ "$USE_MELD" = "true" ]
        then
            meld $TEST/driver_expected.txt $TEST/driver_output.txt
        fi
    fi

else
    echo "wmbusmeters returned error code: $?"
    cat $TEST/output.txt
fi

if [ "$TESTRESULT" = "ERROR" ]
then
    printERROR "$TESTNAME"
    exit 1
fi
