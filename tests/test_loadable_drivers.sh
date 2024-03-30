#!/bin/sh

PROG="$1"
TEST=testoutput
mkdir -p $TEST

performCheck() {
if [ "$?" = "0" ]
then
    cat $TEST/test_output.txt | sed 's/"timestamp":"....-..-..T..:..:..Z"/"timestamp":"1111-11-11T11:11:11Z"/' > $TEST/test_response.txt
    diff $TEST/test_expected.txt $TEST/test_response.txt
    if [ "$?" = "0" ]
    then
        echo "OK: $TESTNAME"
        TESTRESULT="OK"
    else
        if [ "$USE_MELD" = "true" ]
        then
            meld $TEST/test_expected.txt $TEST/test_response.txt
        fi
    fi
else
    echo "ERROR: $TESTNAME $0"
    echo "wmbusmeters returned error code: $?"
    cat $TEST/test_output.txt
fi
}

########################################################################################################################
########################################################################################################################
########################################################################################################################

TESTNAME="Test loadable driver 1"
TESTRESULT="ERROR"
cat > $TEST/driver.xmq <<EOF
driver {
    name           = iporl
    meter_type     = WaterMeter
    default_fields = name,id,total_m3,max_flow_m3h,timestamp
    detect {
        mvt = SEN,99,07
    }
    field {
        name        = totalitator
        quantity    = Volume
        match {
            measurement_type = Instantaneous
            vif_range        = Volume
        }
        about {
            en = 'The total water consumption recorded by this meter.'
        }
    }
    field {
        name        = max_flowwor
        quantity    = Flow
        match {
            measurement_type = Instantaneous
            vif_range        = VolumeFlow
        }
        about {
            en = 'The maximum flow recorded during previous period.'
        }
    }
}
EOF

cat > $TEST/test_expected.txt <<EOF
{"media":"water","meter":"iporl","name":"Hej","id":"33225544","max_flowwor_m3h":0,"totalitator_m3":123.529,"timestamp":"1111-11-11T11:11:11Z"}
EOF

$PROG --format=json 1844AE4C4455223399077A55000000_041389E20100023B0000 Hej $TEST/driver.xmq 33225544 NOKEY > $TEST/test_output.txt 2>&1

performCheck
