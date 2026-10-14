#!/bin/sh

. tests/include.sh

PROG="$1"

mkdir -p testoutput
TEST=testoutput

TESTNAME="Test that a used flag must be declared"
TESTRESULT="ERROR"

cat > $TEST/driver.xmq <<EOF
driver {
    name           = testur
    meter_type     = WaterMeter
    default_fields = name,id,status,total_m3,timestamp
    detect {
        mvt = DWZ,00,06
    }
    library {
        use = total_m3
    }
    flags {
        flag {
            name = LEAKGE_OR_NO_USAGE
            infi = 'Info'
        }
    }
    fields {
        field {
            name       = status
            quantity   = Text
            info       = 'Status and error flags.'
            attributes = STATUS,INCLUDE_TPL_STATUS
            match {
                measurement_type = Instantaneous
                vif_range        = ErrorFlags
            }
            lookup {
                name            = ERROR_FLAGS
                map_type        = BitToString
                mask_bits       = 0xffff
                default_message = OK
                map {
                    name  = LEAKAGE_OR_NO_USAGEE
                    value = 0x40
                    test  = Set
                }
            }
        }
    }
}
EOF


OUTPUT=$($PROG --analyze=$TEST/driver.xmq 21446A127777777702067A00040000_04136A00000002FD174000 2>&1 | grep trying | head -n 1 | grep -o LEAKAGE_OR_NO_USAGEE)

if [ "$?" = "0" ]
then
    if [ "$OUTPUT" = "LEAKAGE_OR_NO_USAGEE" ]
    then
        printOK "$TESTNAME"
        TESTRESULT="OK"
    else
        if [ "$USE_MELD" = "true" ]
        then
            echo "Expected LEAGE_OR_NO_USAGEE to generate a warning or error. But got nothing."
        fi
    fi
fi

if [ "$TESTRESULT" = "ERROR" ]
then
    printERROR "$TESTNAME"
    exit 1
fi
