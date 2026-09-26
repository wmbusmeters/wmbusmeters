#!/bin/sh

. tests/include.sh

PROG="$1"
TEST=testoutput
mkdir -p $TEST

TESTNAME="Test field templates reuse a lookup via pre_shift_right"
TESTRESULT="ERROR"

# Mimics kamwater's real status word: a single 16 bit value packs four
# duration-bucket sub-fields (dry/reversed/leaking/bursting), each a 3 bit
# index (0=none .. 7=22-31 days) at a different bit offset. Instead of
# duplicating the same 8-entry lookup table once per field, declare it once
# as a template and let each field only override pre_shift_right.
cat > $TEST/driver.xmq <<EOF
driver {
    name           = templatetest
    meter_type     = WaterMeter
    default_fields = name,id,time_dry,time_reversed,time_leaking,time_bursting,timestamp
    detect {
        mvt = KAM,1b,16
    }
    templates {
        template_field {
            name     = duration_bucket
            quantity = Text
            lookup {
                name      = DURATION
                map_type  = IndexToString
                mask_bits = 0x0007
                map { name = ''           value = 0 test = Set }
                map { name = '1-8 hours'  value = 1 test = Set }
                map { name = '9-24 hours' value = 2 test = Set }
                map { name = '2-3 days'   value = 3 test = Set }
                map { name = '4-7 days'   value = 4 test = Set }
                map { name = '8-14 days'  value = 5 test = Set }
                map { name = '15-21 days' value = 6 test = Set }
                map { name = '22-31 days' value = 7 test = Set }
            }
        }
        template_field {
            name     = test_flags
            quantity = Text
            lookup {
                name      = FLAGS
                map_type  = BitToString
                mask_bits = 0x0007
                map { name = TEMPLATE_BIT0 bit = 0 test = Set }
                map { name = TEMPLATE_BIT1 bit = 1 test = Set }
            }
        }
    }
    fields {
        field {
            name     = time_dry
            quantity = Text
            template = duration_bucket
            info     = 'Amount of time the meter has been dry.'
            match { difvifkey = 02FF20 }
            lookup { pre_shift_right = 4 }
        }
        field {
            name     = time_reversed
            quantity = Text
            template = duration_bucket
            info     = 'Amount of time the meter has been reversed.'
            match { difvifkey = 02FF20 }
            lookup { pre_shift_right = 7 }
        }
        field {
            name     = time_leaking
            quantity = Text
            template = duration_bucket
            info     = 'Amount of time the meter has been leaking.'
            match { difvifkey = 02FF20 }
            lookup { pre_shift_right = 10 }
        }
        field {
            name     = time_bursting
            quantity = Text
            template = duration_bucket
            info     = 'Amount of time the meter has been bursting.'
            match { difvifkey = 02FF20 }
            lookup { pre_shift_right = 13 }
        }
        field {
            // Bit 0 of the raw status word is set (see the telegram below), bit 1 is not.
            // The field overrides bit 1 only, so bit 0's name must be inherited from the
            // template: proves map{} entries merge instead of being replaced wholesale.
            name     = flags_merged
            quantity = Text
            template = test_flags
            info     = 'Bit 0 inherited from the template, bit 1 overridden by the field.'
            match { difvifkey = 02FF20 }
            lookup { map { name = FIELD_BIT1 bit = 1 test = Set } }
        }
        field {
            // The field overrides bit 0, the one bit that is actually set. Only the
            // field's own name must appear, not the template's: proves the field's own
            // map{} entry wins instead of both being merged for the same bit.
            name     = flags_overridden
            quantity = Text
            template = test_flags
            info     = 'Bit 0 overridden by the field, must win over the template.'
            match { difvifkey = 02FF20 }
            lookup { map { name = FIELD_BIT0 bit = 0 test = Set } }
        }
    }
}
EOF

# Same multical21 telegram used by kamwater's own driver tests: raw status
# word 0x0071 decodes (with kamwater's real, non-templated mask_bits) to
# time_dry=22-31 days, time_reversed/time_leaking/time_bursting=none.
cat > $TEST/test_expected.txt <<EOF
{"_":"telegram","media":"cold water","driver":"templatetest","name":"MyTapWater","id":"76348799","flags_merged":"TEMPLATE_BIT0","flags_overridden":"FIELD_BIT0","time_bursting":"","time_dry":"22-31 days","time_leaking":"","time_reversed":"","timestamp":"1111-11-11T11:11:11Z"}
EOF

$PROG --format=json 2A442D2C998734761B168D2091D37CAC21576C78_02FF207100041308190000441308190000615B7F616713 \
      MyTapWater $TEST/driver.xmq 76348799 NOKEY > $TEST/test_output.txt 2>&1

if [ "$?" = "0" ]
then
    cat $TEST/test_output.txt | sed 's/"timestamp":"....-..-..T..:..:..Z"/"timestamp":"1111-11-11T11:11:11Z"/' > $TEST/test_response.txt
    diff $TEST/test_expected.txt $TEST/test_response.txt
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
    exit 1
fi

TESTNAME="Test field template map{} entries not overridden by the field are inherited (positive)"
grep -q '"flags_merged":"TEMPLATE_BIT0"' $TEST/test_response.txt
if [ "$?" = "0" ]
then
    printOK "$TESTNAME"
else
    printERROR "$TESTNAME"
    cat $TEST/test_response.txt
    exit 1
fi

TESTNAME="Test field's own map{} entry wins over the template's for the same bit (negative)"
grep -q '"flags_overridden":"FIELD_BIT0"' $TEST/test_response.txt
if [ "$?" = "0" ]
then
    printOK "$TESTNAME"
else
    printERROR "$TESTNAME"
    cat $TEST/test_response.txt
    exit 1
fi
