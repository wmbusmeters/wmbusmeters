#!/bin/sh

PROG="$1"

mkdir -p testoutput

TEST=testoutput

TESTNAME="Test selected fields"
TESTRESULT="ERROR"

cat <<EOF > $TEST/test_expected.txt
76348799;Vatten;6408;6.408;0;127;260.6
76348799;Vatten;6408;6.408;0;127;260.6
EOF

$PROG --format=fields --separator=';' \
      --selectfields=id,name,total_l,total_m3,max_flow_m3h,flow_temperature_c,flow_temperature_f \
      --addconversions=L,F \
      simulations/simulation_c1.txt Vatten multical21 76348799 "" \
      > $TEST/test_output.txt

if [ "$?" = "0" ]
then
    diff $TEST/test_expected.txt $TEST/test_output.txt
    if [ "$?" = "0" ]
    then
        echo OK: $TESTNAME
        TESTRESULT="OK"
    fi
fi

if [ "$TESTRESULT" = "ERROR" ]
then
    echo ERROR: $TESTNAME
    exit 1
fi
