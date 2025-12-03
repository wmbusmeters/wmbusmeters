#!/bin/sh

PROG="$1"

rm -rf testoutput
mkdir -p testoutput
TEST=testoutput

TESTNAME="Test dbm unit"
TESTRESULT="ERROR"

OUTPUT=$($PROG --format=fields --selectfields=alfa_dbm,beta_kw,gamma_dbm,delta_dbm,omega_jh  \
               --calculate_alfa_dbm='1w' \
               --calculate_beta_kw='0dbm' \
               --calculate_gamma_dbm='0.00000001w' \
               --calculate_delta_dbm='0.001w' \
               --calculate_omega_jh='50dbm' \
      23442D2C998734761B168D2087D19EAD217F1779EDA86AB6_710008190000081900007F13 \
      MyTapWater multical21 76348799 NOKEY)

if [ "$?" = "0" ]
then
    if [ "30;0.000001;-50;0;360000" = "$OUTPUT" ]
    then
        echo "OK: $TESTNAME"
        exit 0
    fi
fi

if [ "$TESTRESULT" = "ERROR" ]; then echo ERROR: $TESTNAME;  exit 1; fi
