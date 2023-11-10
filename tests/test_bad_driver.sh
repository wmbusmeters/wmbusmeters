#!/bin/sh

PROG="$1"
TEST=testoutput
mkdir -p $TEST

performCheck() {
if [ "$?" = "0" ]
then
    cat $TEST/test_output.txt | sed 's/"timestamp":"....-..-..T..:..:..Z"/"timestamp":"1111-11-11T11:11:11Z"/' > $TEST/test_response.txt
    diff $TEST/test_expected.txt $TEST/test_response.txt
    if [ "$?" = "0" ]
    then
        echo "OK: $TESTNAME"
        TESTRESULT="OK"
    else
        if [ "$USE_MELD" = "true" ]
        then
            meld $TEST/test_expected.txt $TEST/test_response.txt
        fi
    fi
else
    echo "ERROR: $TESTNAME $0"
    echo "wmbusmeters returned error code: $?"
    cat $TEST/test_output.txt
fi
}

########################################################################################################################
########################################################################################################################
########################################################################################################################

TESTNAME="Test no driver root"
TESTRESULT="ERROR"
cat > $TEST/driver.xmq <<EOF
ddriver { name = Iffo }
EOF

cat > $TEST/test_expected.txt <<EOF
(dynamic) Error in testoutput/driver.xmq cannot find driver/name
          A driver file looks like: driver { name = abc123 ... }
EOF

$PROG 1844AE4C4455223399077A55000000_041389E20100023B0000 Hej $TEST/driver.xmq 33225544 NO_KEY > $TEST/test_output.txt 2>&1 || true

performCheck

TESTNAME="Test bad driver name"
TESTRESULT="ERROR"
cat > $TEST/driver.xmq <<EOF
driver { name = Iffo }
EOF

cat > $TEST/test_expected.txt <<EOF
(dynamic) Error in testoutput/driver.xmq invalid driver name "Iffo"
          The driver name must consist of lower case ascii a-z and digits 0-9.
EOF

$PROG 1844AE4C4455223399077A55000000_041389E20100023B0000 Hej $TEST/driver.xmq 33225544 NO_KEY > $TEST/test_output.txt 2>&1 || true

performCheck

TESTNAME="Test no meter_type"
TESTRESULT="ERROR"
cat > $TEST/driver.xmq <<EOF
driver { name = iffo }
EOF

cat > $TEST/test_expected.txt <<EOF
(dynamic) Error in testoutput/driver.xmq cannot find driver/meter_type
          Remember to add: meter_type = ...
EOF

$PROG 1844AE4C4455223399077A55000000_041389E20100023B0000 Hej $TEST/driver.xmq 33225544 NO_KEY > $TEST/test_output.txt 2>&1 || true

performCheck

TESTNAME="Test bad meter_type"
TESTRESULT="ERROR"
cat > $TEST/driver.xmq <<EOF
driver { name = iffo meter_type = gurka }
EOF

cat > $TEST/test_expected.txt <<EOF
(dynamic) Error in testoutput/driver.xmq unknown meter type gurka
Available meter types are:
DoorWindowDetector
ElectricityMeter
GasMeter
HeatCostAllocationMeter
HeatMeter
HeatCoolingMeter
PulseCounter
SmokeDetector
TempHygroMeter
WaterMeter
PressureSensor
EOF

$PROG 1844AE4C4455223399077A55000000_041389E20100023B0000 Hej $TEST/driver.xmq 33225544 NO_KEY > $TEST/test_output.txt 2>&1 || true

performCheck

TESTNAME="Test no default_fields"
TESTRESULT="ERROR"
cat > $TEST/driver.xmq <<EOF
driver { name = iffo meter_type = WaterMeter }
EOF

cat > $TEST/test_expected.txt <<EOF
(dynamic) Error in testoutput/driver.xmq cannot find driver/default_fields
          Remember to add: default_fields = name,id,total_m3,timestamp
          Where you change total_m3 to your meters most important field.
EOF

$PROG 1844AE4C4455223399077A55000000_041389E20100023B0000 Hej $TEST/driver.xmq 33225544 NO_KEY > $TEST/test_output.txt 2>&1 || true

performCheck

TESTNAME="Test no detect mvt:s"
TESTRESULT="ERROR"
cat > $TEST/driver.xmq <<EOF
driver { name = iffo meter_type = WaterMeter default_fields = name,id,total_m3,timestamp }
EOF

cat > $TEST/test_expected.txt <<EOF
(dynamic) Error in testoutput/driver.xmq cannot find any driver/detect/mvt triplets
          Remember to add: detect { mvt = AAA,05,07 mvt = AAA,06,07 ... }
          The triplets contain MANUFACTURER,VERSION,TYPE
          and you can see these values when listening to all meters.
EOF

$PROG 1844AE4C4455223399077A55000000_041389E20100023B0000 Hej $TEST/driver.xmq 33225544 NO_KEY > $TEST/test_output.txt 2>&1 || true

performCheck

TESTNAME="Test bad mvt"
TESTRESULT="ERROR"
cat > $TEST/driver.xmq <<EOF
driver { name = iffo meter_type = WaterMeter default_fields = name,id,total_m3,timestamp
    detect { mvt = alfa }
}
EOF

cat > $TEST/test_expected.txt <<EOF
(dynamic) Error in testoutput/driver.xmq, wrong number of fields in mvt triple: mvt = alfa
          There should be three fields, for example: mvt = AAA,07,05
EOF

$PROG 1844AE4C4455223399077A55000000_041389E20100023B0000 Hej $TEST/driver.xmq 33225544 NO_KEY > $TEST/test_output.txt 2>&1 || true

performCheck
