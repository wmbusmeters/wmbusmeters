#!/bin/sh

PROG="$1"

rm -rf testoutput
mkdir -p testoutput
TEST=testoutput

TESTNAME="Test cmdline formulas with errors --calculate_... "
TESTRESULT="ERROR"

$PROG --format=json \
      --calculate_sumtemp_c='external_temperature_c*198' \
      --calculate_addtemp_c='32+external_temperature_c' \
      --calculate_tjo_m3='99m3+12' \
      --calculate_bar_m3='99m3+12m' \
      --calculate_barr_m3='(123kw' \
      --format=fields --selectfields=name \
      23442D2C998734761B168D2087D19EAD217F1779EDA86AB6_710008190000081900007F13 \
      MyTapWater multical21 76348799 ""  > $TEST/test_output.txt 2>&1

cat <<EOF  > $TEST/test_expected.txt
Warning! Ignoring calculated field sumtemp because parse failed:
Constant number 198 lacks a valid unit!
external_temperature_c*198
                       ^~~~~
Warning! Ignoring calculated field addtemp because parse failed:
Constant number 32 lacks a valid unit!
32+external_temperature_c
^~~~~
Warning! Ignoring calculated field tjo because parse failed:
Constant number 12 lacks a valid unit!
99m3+12
     ^~~~~
Warning! Ignoring calculated field bar because parse failed:
Cannot add [m3|Volume|1m³] to [m|Length|1m]!
99m3+12m
    ^~~~~
Warning! Ignoring calculated field barr because parse failed:
Missing closing parenthesis at end of formula!
MyTapWater
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
