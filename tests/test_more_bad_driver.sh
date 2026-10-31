#!/bin/sh

. tests/include.sh

PROG="$1"
TEST=testoutput
mkdir -p $TEST

TESTNAME="Test driver error detect more than one STATUS"
TESTRESULT="ERROR"

cat > $TEST/test_expected.txt <<EOF
...
EOF

cat > $TEST/driver.xmq <<EOF
// Copyright (C) 2017-2026 Fredrik Öhrström (gpl-3.0-or-later)
driver {
    name           = kamwater
    aliases        = multical21,flowiq2200
    meter_type     = WaterMeter
    default_fields = name,id,status,total_m3,target_m3,timestamp
    manufacturer   = Kamstrup
    detect {
        // multical21
        mvt = KAM,1b,06
    }
    flags {
        flag {
            name = BURST
            info = 'The flow of water has been consistently high for half an hour, which indicates a pipe burst.'
        }
        flag {
            name = DRY
            info = 'The is no water in the meter. Nothing will be measured.'
        }
        flag {
            name = LEAK
            info = 'The flow of water has not stopped for at least one continuous hour during the last 24 hours.'
        }
        flag {
            name = REVERSE
            info = 'The water flows through the meter in the wrong direction.'
        }
        flag {
            name = TAMPER
            info = 'Attempt of fraud. The meter is no longer valid for billing.'
        }
        flag {
            name = LOW_BATTERY
            info = 'The battery is running low. Less than 6 months remaining.'
        }
        flag {
            name = FLOW_ABOVE_Q4
            info = 'The water flow has exceeded the upper flow limit Q4 which can cause too high error rate.'
        }
        flag {
            name = NO_CONSUMPTION
            info = 'No flow of water has occurred for the last 15 days.'
        }
        flag {
            name = LOW_AMBIENT_TEMPERATURE
            info = 'The ambient temperature is below the configured lower temperature limit.'
        }
        flag {
            name = HIGH_AMBIENT_TEMPERATURE
            info = 'The ambient temperature is above the configured upper temperature limit.'
        }
    }
    templates {
        template_field {
            name = duration_bucket
            lookup {
                name      = DURATION
                map_type  = IndexToString
                mask_bits = 0x7
                map {
                    name  = ''
                    value = 0x0
                    test  = Set
                }
                map {
                    name  = '1-8 hours'
                    value = 0x1
                    test  = Set
                }
                map {
                    name  = '9-24 hours'
                    value = 0x2
                    test  = Set
                }
                map {
                    name  = '2-3 days'
                    value = 0x3
                    test  = Set
                }
                map {
                    name  = '4-7 days'
                    value = 0x4
                    test  = Set
                }
                map {
                    name  = '8-14 days'
                    value = 0x5
                    test  = Set
                }
                map {
                    name  = '15-21 days'
                    value = 0x6
                    test  = Set
                }
                map {
                    name  = '22-31 days'
                    value = 0x7
                    test  = Set
                }
            }
        }
        template_field {
            // Common status fields for modern status fields:
            // - 04FF23 Info Cold environment
            // - 04FF24 Info Warm environment
            // The datasheet also mentions 16bit fields (02FF25, 12FF25), but they are
            // only partially compatible and we have no recursive inheritance.
            name = x2xx_status
            lookup {
                name     = ERROR_FLAGS
                map_type = BitToString
                // In these status fields, buts 0-10 are the instant info (with
                // bits 8 and reserved, and bit 6's different meaning between
                // meters configured for hot/cold *environment*). Bits 11-31
                // are for time indication. But since bits 29-31 are reserved,
                // let's roll them back into the "status" part and not "time
                // indication" part. That way, all parts of the 32bit field are
                // now covered.
                mask_bits       = 0xe00003ff
                default_message = OK
                map {
                    name = DRY
                    bit  = 0
                    test = Set
                }
                map {
                    name = REVERSE
                    bit  = 1
                    test = Set
                }
                map {
                    name = LEAK
                    bit  = 2
                    test = Set
                }
                map {
                    name = BURST
                    bit  = 3
                    test = Set
                }
                map {
                    name = TAMPER
                    bit  = 4
                    test = Set
                }
                map {
                    name = LOW_BATTERY
                    bit  = 5
                    test = Set
                }
                // bit 6 is different and will be filed in later
                map {
                    name = FLOW_ABOVE_Q4
                    bit  = 7
                    test = Set
                }
                // bit 8 is unused
                map {
                    name = NO_CONSUMPTION
                    bit  = 9
                    test = Set
                }
                // bit 10 is unused
            }
        }
    }
    fields {
        field {
            // A fake field which never matches, but propagates the TPL error bits.
            // This is required, because the *driver* cannot really define multiple
            // fields with INCLUDE_TPL_STATUS. If there was more than one such field,
            // the matchers would accidentally overwrite their own values. So, create
            // this virtual one, and let the "real" fields (of which there are multiple
            // alternatives in the driver, and only one of them will match in a real
            // packet) propagate thir stuff.
            name       = status
            quantity   = Flags
            attributes = STATUS,INCLUDE_TPL_STATUS
        }
        field {
            // A fake field which never matches, but propagates the TPL error bits.
            // This is required, because the *driver* cannot really define multiple
            // fields with INCLUDE_TPL_STATUS. If there was more than one such field,
            // the matchers would accidentally overwrite their own values. So, create
            // this virtual one, and let the "real" fields (of which there are multiple
            // alternatives in the driver, and only one of them will match in a real
            // packet) propagate thir stuff.
            name       = statuss
            quantity   = Flags
            attributes = STATUS,INCLUDE_TPL_STATUS
        }
        field {
            // A fake field which never matches, but propagates the TPL error bits.
            // This is required, because the *driver* cannot really define multiple
            // fields with INCLUDE_TPL_STATUS. If there was more than one such field,
            // the matchers would accidentally overwrite their own values. So, create
            // this virtual one, and let the "real" fields (of which there are multiple
            // alternatives in the driver, and only one of them will match in a real
            // packet) propagate thir stuff.
            name       = statuss
            quantity   = Flags
            attributes = STATUS,INCLUDE_TPL_STATUS
        }
        field {
            name       = status16
            quantity   = Flags
            info       = 'Status and error flags.'
            attributes = INJECT_INTO_STATUS
            match {
                difvifkey = 02FF20
            }
            lookup {
                name            = ERROR_FLAGS
                map_type        = BitToString
                mask_bits       = 0x000f
                default_message = OK
                map {
                    name = DRY
                    bit  = 0
                    test = Set
                }
                map {
                    name = REVERSE
                    bit  = 1
                    test = Set
                }
                map {
                    name = LEAK
                    bit  = 2
                    test = Set
                }
                map {
                    name = BURST
                    bit  = 3
                    test = Set
                }
            }
        }
        field {
            name          = current_status
            quantity      = Text
            info          = 'Status of meter. This field will go away use status instead.'
            match {
                difvifkey = 02FF20
            }
            lookup {
                name      = ERROR_FLAGS
                map_type  = BitToString
                mask_bits = 0x000f
                map {
                    name = DRY
                    bit  = 0
                    test = Set
                }
                map {
                    name = REVERSE
                    bit  = 1
                    test = Set
                }
                map {
                    name = LEAK
                    bit  = 2
                    test = Set
                }
                map {
                    name = BURST
                    bit  = 3
                    test = Set
                }
            }
        }
        field {
            name     = time_dry
            quantity = Text
            template = duration_bucket
            info     = 'Amount of time the meter has been dry.'
            match {
                difvifkey = 02FF20
            }
            lookup {
                pre_shift_right = 4
            }
        }
    }
}
EOF

$PROG --format=json 2A442D2C998734761B168D2091D37CAC21576C78_02FF207100041308190000441308190000615B7F616713 Vatten $TEST/driver.xmq 76348799 NOKEY | jq .

exit 0

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
else
    printERROR "$TESTNAME"
    echo "wmbusmeters returned error code: $?"
    cat $TEST/test_output.txt
    exit 1
fi
