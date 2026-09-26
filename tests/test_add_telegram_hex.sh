#!/bin/sh

. tests/include.sh

PROG="$1"

rm -rf testoutput
mkdir -p testoutput
TEST=testoutput

TESTNAME="Test add telegram hex"
TESTRESULT="ERROR"

HEX="5e442d2c0105798240047a7d0050252f2f0406c50e000004147B86000004ff074254000004ff086047000002594117025d9a14023Bed0302ff220000026cca2c4406750B00004414ad680000426cc12c2f2f2f2f2f2f2f2f2f2f2f2f2f2f2f"

cat <<EOF | jq --sort-keys . > $TEST/test_expected.txt
{
  "_": "telegram",
  "media": "heat",
  "driver": "kamheat",
  "name": "Foo",
  "id": "82790501",
  "hex": "5E442D2C0105798240047A7D0050252F2F0406C50E000004147B86000004FF074254000004FF086047000002594117025D9A14023BED0302FF220000026CCA2C4406750B00004414AD680000426CC12C2F2F2F2F2F2F2F2F2F2F2F2F2F2F2F",
  "approx_power_m3ch": 6.82395,
  "forward_energy_m3c": 21570,
  "return_energy_m3c": 18272,
  "t1_temperature_c": 59.53,
  "t2_temperature_c": 52.74,
  "target_date": "2022-12-01",
  "target_energy_kwh": 2933,
  "target_volume_m3": 267.97,
  "total_energy_consumption_kwh": 3781,
  "total_volume_m3": 344.27,
  "volume_flow_m3h": 1.005,
  "meter_date": "2022-12-10",
  "status": "OK",
  "timestamp": "1111-11-11T11:11:11Z"
}
EOF

$PROG --format=json --addtelegramhex --calculate_approx_power_m3ch='(t1_temperature_c-t2_temperature_c)*volume_flow_m3h' $HEX Foo kamheat 82790501 NOKEY | jq --sort-keys . > $TEST/test_output.txt 2>&1

if [ "$?" = "0" ]
then
    cat $TEST/test_output.txt | sed 's/"timestamp": "....-..-..T..:..:..Z"/"timestamp": "1111-11-11T11:11:11Z"/' > $TEST/test_responses.txt
    diff $TEST/test_expected.txt $TEST/test_responses.txt
    if [ "$?" = "0" ]
    then
        printOK "$TESTNAME"
        TESTRESULT="OK"
    fi
fi

if [ "$TESTRESULT" = "ERROR" ]; then printERROR "$TESTNAME";  exit 1; fi
