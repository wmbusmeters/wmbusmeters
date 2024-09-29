#!/bin/sh

PROG="$1"

rm -rf testoutput
mkdir -p testoutput
TEST=testoutput

TESTNAME="Test force_scale."
TESTRESULT="ERROR"

cat <<EOF > $TEST/test.xmq
driver {
    name           = ime
    default_fields = alfa_kwh,beta_kwh,gamma_kwh,delta_kwh
    meter_type     = ElectricityMeter
    detect {
        mvt = IME,55,08
    }
    fields {
        field {
            name           = alfa
            quantity       = Energy
            vif_scaling    = None
            dif_signedness = Unsigned
            display_unit   = kwh
            force_scale    = 1
            match {
                difvifkey = 849010FF80843B
            }
        }
        field {
            name           = beta
            quantity       = Energy
            vif_scaling    = None
            dif_signedness = Unsigned
            display_unit   = kwh
            force_scale    = 1/3
            match {
                difvifkey = 849010FF80843B
            }
        }
        field {
            name           = gamma
            quantity       = Energy
            vif_scaling    = None
            dif_signedness = Unsigned
            display_unit   = kwh
            force_scale    = 1000.0
            match {
                difvifkey = 849010FF80843B
            }
        }
        field {
            name           = delta
            quantity       = Energy
            vif_scaling    = None
            dif_signedness = Unsigned
            display_unit   = kwh
            force_scale    = 3.3/3.3
            match {
                difvifkey = 849010FF80843B
            }
        }
    }
}

EOF

$PROG --format=fields \
      68CBCB6808017278563412A525660267000000849010FF80843B7A820700849010FF80843C00000000849010FF81843BF35B0400849010FF81843C010000008410FF80843B427107008420FF80843B381100008410FF80843C000000008420FF80843C000000008410FF81843B5A4F04008420FF81843B980C00008410FF81843C010000008420FF81843C0000000084A010FF80843B7A82070084A010FF80843C0000000084A010FF81843BF35B040084A010FF81843C0100000004FF90290000000002FF912B00001F0000000000f11623442D2C998734761B168D2087D19EAD217F1779EDA86AB6_710008190000081900007F13 \
      METERU $TEST/test.xmq 12345678 NOKEY \
      > $TEST/test_output.txt

cat <<EOF > $TEST/test_expected.txt
492154;164051.333333;492154000;492154
EOF

if [ "$?" = "0" ]
then
    cat $TEST/test_output.txt | sed 's/"timestamp": "....-..-..T..:..:..Z"/"timestamp": "1111-11-11T11:11:11Z"/' > $TEST/test_responses.txt
    diff $TEST/test_expected.txt $TEST/test_responses.txt
    if [ "$?" = "0" ]
    then
        echo "OK: $TESTNAME"
        TESTRESULT="OK"
    else
        if [ "$USE_MELD" = "true" ]
        then
            meld $TEST/test_expected.txt $TEST/test_responses.txt
        fi
    fi
fi

if [ "$TESTRESULT" = "ERROR" ]; then echo ERROR: $TESTNAME;  exit 1; fi
