#!/bin/sh

PROG="$1"

rm -rf testoutput
mkdir -p testoutput
TEST=testoutput

TESTNAME="Test dbm unit"
TESTRESULT="ERROR"

OUTPUT=$($PROG --format=fields --selectfields=alfa_dbm,beta_kw,gamma_dbm,delta_dbm,omega_jh,rho_w \
               --calculate_alfa_dbm='1w' \
               --calculate_beta_kw='0dbm' \
               --calculate_gamma_dbm='0.00000001w' \
               --calculate_delta_dbm='0.001w' \
               --calculate_omega_jh='50dbm' \
               --calculate_rho_w='1dbm-2dbm' \
      23442D2C998734761B168D2087D19EAD217F1779EDA86AB6_710008190000081900007F13 \
      MyTapWater multical21 76348799 NOKEY)

EXPECTED="30;0.000001;-50;0;360000;0.000794"
if [ "$?" = "0" ]
then
    if [ "$EXPECTED" = "$OUTPUT" ]
    then
        echo "OK: $TESTNAME"
        exit 0
    else
        echo "Expected $EXPECTED"
        echo "Got      $OUTPUT"
    fi
fi

if [ "$TESTRESULT" = "ERROR" ]; then echo ERROR: $TESTNAME;  exit 1; fi
