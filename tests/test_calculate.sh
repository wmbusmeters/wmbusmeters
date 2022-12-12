#!/bin/sh

PROG="$1"

rm -rf testoutput
mkdir -p testoutput
TEST=testoutput

TESTNAME="Test calculations"
TESTRESULT="ERROR"

HEX="5e442d2c0105798240047a7d0050252f2f0406c50e000004147B86000004ff074254000004ff086047000002594117025d9a14023Bed0302ff220000026cca2c4406750B00004414ad680000426cc12c2f2f2f2f2f2f2f2f2f2f2f2f2f2f2f"

cat > $TEST/test_expected.txt <<EOF
21570;6.82395
EOF

$PROG --format=fields --selectfields=forward_energy_m3c,approx_power_m3ch --calculate_approx_power_m3ch='(t1_temperature_c-t2_temperature_c)*volume_flow_m3h' $HEX Foo kamheat 82790501 NOKEY > $TEST/test_output.txt 2>&1

if [ "$?" = "0" ]
then
    cat $TEST/test_output.txt | sed 's/"timestamp":"....-..-..T..:..:..Z"/"timestamp":"1111-11-11T11:11:11Z"/' > $TEST/test_responses.txt
    diff $TEST/test_expected.txt $TEST/test_responses.txt
    if [ "$?" = "0" ]
    then
        echo "OK: $TESTNAME"
        TESTRESULT="OK"
    fi
fi

if [ "$TESTRESULT" = "ERROR" ]; then echo ERROR: $TESTNAME;  exit 1; fi
