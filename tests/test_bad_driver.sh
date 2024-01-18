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
(driver) error in testoutput/driver.xmq, cannot find: driver/name
-------------------------------------------------------------------------------
A driver file looks like this: driver { name = abc123 ... }
-------------------------------------------------------------------------------
Failed to load driver from file: testoutput/driver.xmq
EOF

$PROG 1844AE4C4455223399077A55000000_041389E20100023B0000 Hej $TEST/driver.xmq 33225544 NO_KEY > $TEST/test_output.txt 2>&1 || true

performCheck

TESTNAME="Test bad driver name"
TESTRESULT="ERROR"
cat > $TEST/driver.xmq <<EOF
driver { name = 'a b' }
EOF

cat > $TEST/test_expected.txt <<EOF
(driver) error in testoutput/driver.xmq, bad driver name: a b
-------------------------------------------------------------------------------
The driver name must consist of lower case ascii a-z and digits 0-9.
-------------------------------------------------------------------------------
Failed to load driver from file: testoutput/driver.xmq
EOF

$PROG 1844AE4C4455223399077A55000000_041389E20100023B0000 Hej $TEST/driver.xmq 33225544 NO_KEY > $TEST/test_output.txt 2>&1 || true

performCheck

TESTNAME="Test no meter_type"
TESTRESULT="ERROR"
cat > $TEST/driver.xmq <<EOF
driver { name = iffo }
EOF

cat > $TEST/test_expected.txt <<EOF
(driver) error in testoutput/driver.xmq, cannot find: driver/meter_type
-------------------------------------------------------------------------------
Remember to add: meter_type = ...
Available meter types are:
DoorWindowDetector
ElectricityMeter
GasMeter
HeatCoolingMeter
HeatCostAllocationMeter
HeatMeter
PressureSensor
PulseCounter
Repeater
SmokeDetector
TempHygroMeter
WaterMeter
-------------------------------------------------------------------------------
Failed to load driver from file: testoutput/driver.xmq
EOF

$PROG 1844AE4C4455223399077A55000000_041389E20100023B0000 Hej $TEST/driver.xmq 33225544 NO_KEY > $TEST/test_output.txt 2>&1 || true

performCheck

TESTNAME="Test bad meter_type"
TESTRESULT="ERROR"
cat > $TEST/driver.xmq <<EOF
driver { name = iffo meter_type = gurka }
EOF

cat > $TEST/test_expected.txt <<EOF
(driver) error in testoutput/driver.xmq, unknown meter type: gurka
-------------------------------------------------------------------------------
Available meter types are:
DoorWindowDetector
ElectricityMeter
GasMeter
HeatCoolingMeter
HeatCostAllocationMeter
HeatMeter
PressureSensor
PulseCounter
Repeater
SmokeDetector
TempHygroMeter
WaterMeter
-------------------------------------------------------------------------------
Failed to load driver from file: testoutput/driver.xmq
EOF

$PROG 1844AE4C4455223399077A55000000_041389E20100023B0000 Hej $TEST/driver.xmq 33225544 NO_KEY > $TEST/test_output.txt 2>&1 || true

performCheck

TESTNAME="Test no default_fields"
TESTRESULT="ERROR"
cat > $TEST/driver.xmq <<EOF
driver { name = iffo meter_type = WaterMeter }
EOF

cat > $TEST/test_expected.txt <<EOF
(driver) error in testoutput/driver.xmq, cannot find: driver/default_fields
-------------------------------------------------------------------------------
Remember to add for example: default_fields = name,id,total_m3,timestamp
Where you change total_m3 to your meters most important field.
-------------------------------------------------------------------------------
Failed to load driver from file: testoutput/driver.xmq
EOF

$PROG 1844AE4C4455223399077A55000000_041389E20100023B0000 Hej $TEST/driver.xmq 33225544 NO_KEY > $TEST/test_output.txt 2>&1 || true

performCheck

TESTNAME="Test no detect mvt:s"
TESTRESULT="ERROR"
cat > $TEST/driver.xmq <<EOF
driver { name = iffo meter_type = WaterMeter default_fields = name,id,total_m3,timestamp }
EOF

cat > $TEST/test_expected.txt <<EOF
(driver) error in testoutput/driver.xmq, cannot find any detection triplets: driver/detect/mvt
-------------------------------------------------------------------------------
Remember to add: detect { mvt = AAA,05,07 mvt = AAA,06,07 ... }
The triplets consists of MANUFACTURER,VERSION,TYPE
You can see these values when listening to all meters.
The manufacturer can be given as three uppercase characters A-Z
or as 4 lower case hex digits.
-------------------------------------------------------------------------------
Failed to load driver from file: testoutput/driver.xmq
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
(driver) error in testoutput/driver.xmq, wrong number of fields in mvt triple: mvt = alfa
-------------------------------------------------------------------------------
There should be three fields, for example: mvt = AAA,07,05
-------------------------------------------------------------------------------
Failed to load driver from file: testoutput/driver.xmq
EOF

$PROG 1844AE4C4455223399077A55000000_041389E20100023B0000 Hej $TEST/driver.xmq 33225544 NO_KEY > $TEST/test_output.txt 2>&1 || true

performCheck

TESTNAME="Test no field name"
TESTRESULT="ERROR"
cat > $TEST/driver.xmq <<EOF
driver { name = iffo meter_type = WaterMeter default_fields = name,id,total_m3,timestamp
    detect { mvt = SEN,99,07 }
    field { namee = total }
}
EOF

cat > $TEST/test_expected.txt <<EOF
(driver) error in testoutput/driver.xmq, cannot find: driver/field/name
-------------------------------------------------------------------------------
Remember to add for example: field { name = total ... }
-------------------------------------------------------------------------------
Hej;?total_m3?
EOF

$PROG --format=fields --selectfields=name,total_m3 1844AE4C4455223399077A55000000_041389E20100023B0000 Hej $TEST/driver.xmq 33225544 NO_KEY > $TEST/test_output.txt 2>&1 || true

performCheck

TESTNAME="Test bad field name with unit"
TESTRESULT="ERROR"
cat > $TEST/driver.xmq <<EOF
driver { name = iffo meter_type = WaterMeter default_fields = name,id,total_m3,timestamp
    detect { mvt = SEN,99,07 }
    field { name = total_m3 }
}
EOF

cat > $TEST/test_expected.txt <<EOF
(driver) error in testoutput/driver.xmq, bad field name total_m3 (field names should not have units)
-------------------------------------------------------------------------------
The field name should not have a unit since units are added automatically.
Either indirectly based on the quantity or directly based on the display_unit.
-------------------------------------------------------------------------------
Hej;?total_m3?
EOF

$PROG --format=fields --selectfields=name,total_m3 1844AE4C4455223399077A55000000_041389E20100023B0000 Hej $TEST/driver.xmq 33225544 NO_KEY > $TEST/test_output.txt 2>&1 || true

performCheck

TESTNAME="Test field missing quantity"
TESTRESULT="ERROR"
cat > $TEST/driver.xmq <<EOF
driver { name = iffo meter_type = WaterMeter default_fields = name,id,total_m3,timestamp
    detect { mvt = SEN,99,07 }
    field { name = total }
}
EOF

cat > $TEST/test_expected.txt <<EOF
(driver) error in testoutput/driver.xmq, cannot find: driver/field/quantity
-------------------------------------------------------------------------------
Remember to add for example: field { quantity = Volume ... }
Available quantities:
Time
Length
Mass
Amperage
Temperature
AmountOfSubstance
LuminousIntensity
Energy
Reactive_Energy
Apparent_Energy
Power
Volume
Flow
Voltage
Frequency
Pressure
PointInTime
RelativeHumidity
HCA
Text
Angle
Dimensionless
-------------------------------------------------------------------------------
Hej;?total_m3?
EOF

$PROG --format=fields --selectfields=name,total_m3 1844AE4C4455223399077A55000000_041389E20100023B0000 Hej $TEST/driver.xmq 33225544 NO_KEY > $TEST/test_output.txt 2>&1 || true

performCheck

TESTNAME="Test field bad quantity"
TESTRESULT="ERROR"
cat > $TEST/driver.xmq <<EOF
driver { name = iffo meter_type = WaterMeter default_fields = name,id,total_m3,timestamp
    detect { mvt = SEN,99,07 }
    field { name = total quantity = gurka }
}
EOF

cat > $TEST/test_expected.txt <<EOF
(driver) error in testoutput/driver.xmq, bad quantity: gurka
-------------------------------------------------------------------------------
Available quantities:
Time
Length
Mass
Amperage
Temperature
AmountOfSubstance
LuminousIntensity
Energy
Reactive_Energy
Apparent_Energy
Power
Volume
Flow
Voltage
Frequency
Pressure
PointInTime
RelativeHumidity
HCA
Text
Angle
Dimensionless
-------------------------------------------------------------------------------
Hej;?total_m3?
EOF

$PROG --format=fields --selectfields=name,total_m3 1844AE4C4455223399077A55000000_041389E20100023B0000 Hej $TEST/driver.xmq 33225544 NO_KEY > $TEST/test_output.txt 2>&1 || true

performCheck

TESTNAME="Test field with no matcher and null output"
TESTRESULT="ERROR"
cat > $TEST/driver.xmq <<EOF
driver { name = iffo meter_type = WaterMeter default_fields = name,id,total_m3,timestamp
    detect { mvt = SEN,99,07 }
    field { name = total quantity = Volume }
}
EOF

# There is no calculate nor match in the driver. Thus no value is inserted to it remains null.
cat > $TEST/test_expected.txt <<EOF
Hej;null
EOF

$PROG --format=fields --selectfields=name,total_m3 1844AE4C4455223399077A55000000_041389E20100023B0000 Hej $TEST/driver.xmq 33225544 NO_KEY > $TEST/test_output.txt 2>&1 || true

performCheck

TESTNAME="Test field with calculate"
TESTRESULT="ERROR"
cat > $TEST/driver.xmq <<EOF
driver { name = iffo meter_type = WaterMeter default_fields = name,id,total_m3,timestamp
    detect { mvt = SEN,99,07 }
    field { name = total quantity = Volume calculate = '4711.23m3 + 0.9m3' }
}
EOF

cat > $TEST/test_expected.txt <<EOF
Hej	4712.13 m³
EOF

$PROG --format=hr --selectfields=name,total_m3 1844AE4C4455223399077A55000000_041389E20100023B0000 Hej $TEST/driver.xmq 33225544 NO_KEY > $TEST/test_output.txt 2>&1 || true

performCheck

TESTNAME="Test field matcher without measurement_type"
TESTRESULT="ERROR"
cat > $TEST/driver.xmq <<EOF
driver { name = iffo meter_type = WaterMeter default_fields = name,id,total_m3,timestamp
    detect { mvt = SEN,99,07 }
    field { name = total quantity = Volume match { } }
}
EOF

cat > $TEST/test_expected.txt <<EOF
(driver) error in testoutput/driver.xmq, cannot find: driver/field/match/measurement_type
-------------------------------------------------------------------------------
Remember to add for example: match { measurement_type = Instantaneous ... }
Available measurement types:
Instantaneous
Minimum
Maximum
AtError
Any
-------------------------------------------------------------------------------
Hej	?total_m3?
EOF

$PROG --format=hr --selectfields=name,total_m3 1844AE4C4455223399077A55000000_041389E20100023B0000 Hej $TEST/driver.xmq 33225544 NO_KEY > $TEST/test_output.txt 2>&1 || true

performCheck

TESTNAME="Test field matcher with bad measurement_type "
TESTRESULT="ERROR"
cat > $TEST/driver.xmq <<EOF
driver { name = iffo meter_type = WaterMeter default_fields = name,id,total_m3,timestamp
    detect { mvt = SEN,99,07 }
    field { name = total quantity = Volume match { measurement_type = sdfInstantaneous } }
}
EOF

cat > $TEST/test_expected.txt <<EOF
(driver) error in testoutput/driver.xmq, bad measurement_type: sdfInstantaneous
-------------------------------------------------------------------------------
Available measurement types:
Instantaneous
Minimum
Maximum
AtError
Any
-------------------------------------------------------------------------------
Hej	?total_m3?
EOF

$PROG --format=hr --selectfields=name,total_m3 1844AE4C4455223399077A55000000_041389E20100023B0000 Hej $TEST/driver.xmq 33225544 NO_KEY > $TEST/test_output.txt 2>&1 || true

performCheck

TESTNAME="Test field matcher without vif_range"
TESTRESULT="ERROR"
cat > $TEST/driver.xmq <<EOF
driver { name = iffo meter_type = WaterMeter default_fields = name,id,total_m3,timestamp
    detect { mvt = SEN,99,07 }
    field { name = total quantity = Volume match { measurement_type = Instantaneous } }
}
EOF

cat > $TEST/test_expected.txt <<EOF
(driver) error in testoutput/driver.xmq, cannot find: driver/field/match/vif_range
-------------------------------------------------------------------------------
Remember to add for example: match { ... vif_range = ReturnTemperature ... }
Available vif ranges:
Volume
OnTime
OperatingTime
VolumeFlow
FlowTemperature
ReturnTemperature
TemperatureDifference
ExternalTemperature
Pressure
HeatCostAllocation
Date
DateTime
EnergyMJ
EnergyWh
PowerW
ActualityDuration
FabricationNo
EnhancedIdentification
EnergyMWh
RelativeHumidity
AccessNumber
Medium
Manufacturer
ParameterSet
ModelVersion
HardwareVersion
FirmwareVersion
SoftwareVersion
Location
Customer
ErrorFlags
DigitalOutput
DigitalInput
DurationSinceReadout
DurationOfTariff
Dimensionless
Voltage
Amperage
ResetCounter
CumulationCounter
SpecialSupplierInformation
RemainingBattery
AnyVolumeVIF
AnyEnergyVIF
AnyPowerVIF
-------------------------------------------------------------------------------
Hej	?total_m3?
EOF

$PROG --format=hr --selectfields=name,total_m3 1844AE4C4455223399077A55000000_041389E20100023B0000 Hej $TEST/driver.xmq 33225544 NO_KEY > $TEST/test_output.txt 2>&1 || true

performCheck

TESTNAME="Test proper field matcher"
TESTRESULT="ERROR"
cat > $TEST/driver.xmq <<EOF
driver { name = iffo meter_type = WaterMeter default_fields = name,id,total_m3,timestamp
    detect { mvt = SEN,99,07 }
    field { name = total quantity = Volume match { measurement_type = Instantaneous vif_range = Volume } }
}
EOF

cat > $TEST/test_expected.txt <<EOF
Hej	123.529 m³
EOF

$PROG --format=hr --selectfields=name,total_m3 1844AE4C4455223399077A55000000_041389E20100023B0000 Hej $TEST/driver.xmq 33225544 NO_KEY > $TEST/test_output.txt 2>&1 || true

performCheck
