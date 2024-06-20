#!/bin/sh

# Test generating a key that contains a calculated date.
# I.e. we have a target_date in the telegram, then calculate a previous month date like this:
#    name     = 'target_{storage_counter}_{target_date - (storage_counter * 1month)}'
# This will generate key value pairs like:
# "target_2_2021-10-31_kwh":5663
# Given that "target_date":"2021-12-31" ie 2012-12-31 minus two months is 2021-10-31.

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

####################################################################################################################
####################################################################################################################
####################################################################################################################

TESTNAME="Test dyn driver with date in key"
TESTRESULT="ERROR"
cat > $TEST/driver.xmq <<EOF
/* Copyright (C) 2020-2022 Patrick Schwarz (gpl-3.0-or-later)
   Copyright (C) 2022-2024 Fredrik Öhrström (gpl-3.0-or-later) */
driver {
    name           = sensostar
    meter_type     = HeatMeter
    default_fields = name,id,total_kwh,timestamp
    detect {
        mvt = EFE,00,04
    }
    field {
        name     = total
        quantity = Energy
        info     = 'The total heat energy consumption recorded by this meter.'
        match {
            measurement_type = Instantaneous
            vif_range        = AnyEnergyVIF
        }
    }
    field {
        name     = target_water
        quantity = Volume
        info     = 'The total volume of heating media as recorded at the end of the billing period.'
        match {
            measurement_type = Instantaneous
            vif_range        = Volume
            storage_nr       = 1
        }
    }
    field {
        name         = target
        quantity     = PointInTime
        info         = 'The reporting date of the last billing period.'
        display_unit = date
        match {
            measurement_type = Instantaneous
            vif_range        = Date
            storage_nr       = 1
        }
    }
    field {
        name     = target
        quantity = Energy
        info     = 'The energy consumption at the last billing period date.'
        match {
            measurement_type = Instantaneous
            vif_range        = AnyEnergyVIF
            storage_nr       = 1
        }
    }
    field {
        name     = 'target_{storage_counter}_{target_date - (storage_counter * 1month)}'
        quantity = Energy
        match {
            measurement_type = Instantaneous
            vif_range        = AnyEnergyVIF
            storage_nr       = 2,32
        }
    }
}

EOF

cat > $TEST/test_expected.txt <<EOF
{"media":"heat","meter":"sensostar","name":"Hej","id":"02752560","target_kwh":3671,"target_date":"2021-12-31","target_10_2021-02-28_kwh":5610,"target_12_2020-12-31_kwh":5567,"target_14_2020-10-31_kwh":5202,"target_16_2020-08-31_kwh":4754,"target_18_2020-06-30_kwh":4293,"target_20_2020-04-30_kwh":3671,"target_22_2020-02-29_kwh":3018,"target_24_2019-12-31_kwh":2522,"target_26_2019-10-31_kwh":2250,"target_28_2019-08-31_kwh":2248,"target_2_2021-10-31_kwh":5663,"target_30_2019-06-30_kwh":2246,"target_4_2021-08-31_kwh":5622,"target_6_2021-06-30_kwh":5621,"target_8_2021-04-30_kwh":5619,"total_kwh":5699,"timestamp":"1111-11-11T11:11:11Z"}
EOF

$PROG --format=json a444c5146025750200047ac20000202f2f046d2e26c62a040643160000041310f0050001fd1700426cbf2c4406570e00008401061f160000840206f6150000840306f5150000840406f3150000840506ea150000840606bf1500008407065214000084080692120000840906c5100000840a06570e0000840b06ca0b0000840c06da090000840d06ca080000840e06c8080000840f06c608000003fd0c05010002fd0b2111 Hej $TEST/driver.xmq 02752560 NOKEY > $TEST/test_output.txt

performCheck
