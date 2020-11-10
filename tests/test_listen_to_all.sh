#!/bin/sh

PROG="$1"

if [ "$PROG" = "" ]
then
    echo Please supply the binary to be tested as the first argument.
    exit 1
fi

TEST=testoutput
LOGFILE=$TEST/logfile
LOGFILE_EXPECTED=$TEST/logfile.expected

TESTNAME="Test listen and print any meter heard in logfile"
TESTRESULT="ERROR"

mkdir -p $TEST
rm -f $LOGFILE

cat > $LOGFILE_EXPECTED <<EOF
No meters configured. Printing id:s of all telegrams heard!
Received telegram from: 12345678
          manufacturer: (SON) Sontex, Switzerland (0x4dee)
                  type: Warm Water (30°C-90°C) meter (0x06)
                   ver: 0x3c
                driver: supercom587
Received telegram from: 11111111
          manufacturer: (SON) Sontex, Switzerland (0x4dee)
                  type: Water meter (0x07)
                   ver: 0x3c
                driver: supercom587
Received telegram from: 27282728
          manufacturer: (SON) Sontex, Switzerland (0x4dee)
                  type: Heat Cost Allocator (0x08)
                   ver: 0x16
                driver: sontex868
Received telegram from: 12345699
          manufacturer: (SEN) Sensus Metering Systems, Germany (0x4cae)
                  type: Water meter (0x07)
                   ver: 0x68
                driver: iperl
Received telegram from: 33225544
          manufacturer: (SEN) Sensus Metering Systems, Germany (0x4cae)
                  type: Water meter (0x07)
                   ver: 0x68
                driver: iperl
Received telegram from: 10101010
          manufacturer: (APA) Apator, Poland (0x601)
                  type: Electricity meter (0x02)
                   ver: 0x02
                driver: amiplus
Received telegram from: 34333231
          manufacturer: (TCH) Techem Service (0x5068)
                  type: Warm water (0x62)
                   ver: 0x74
                driver: mkradio3
Received telegram from: 58234965
          manufacturer: (TCH) Techem Service (0x5068)
                  type: Heat meter (0xc3)
                   ver: 0x27
                driver: vario451
Received telegram from: 11776622
          manufacturer: (TCH) Techem Service (0x5068)
                  type: Heat Cost Allocator (0x80)
                   ver: 0x69
                driver: fhkvdataiii
Received telegram from: 88018801
          manufacturer: (INE) INNOTAS Elektronik, Germany (0x25c5)
                  type: Heat Cost Allocator (0x08)
                   ver: 0x55
                driver: eurisii
Received telegram from: 00010204
          manufacturer: (LAS) Lansen Systems, Sweden (0x3033)
                  type: Smoke detector (0x1a)
                   ver: 0x03
                driver: lansensm
Received telegram from: 00010204
          manufacturer: (LAS) Lansen Systems, Sweden (0x3033)
                  type: Smoke detector (0x1a)
                   ver: 0x03
                driver: lansensm
Received telegram from: 00010203
          manufacturer: (LAS) Lansen Systems, Sweden (0x3033)
                  type: Room sensor (eg temperature or humidity) (0x1b)
                   ver: 0x07
                driver: lansenth
Received telegram from: 00010205
          manufacturer: (LAS) Lansen Systems, Sweden (0x3033)
                  type: Reserved for sensors (0x1d)
                   ver: 0x07
                driver: lansendw
Received telegram from: 00010205
          manufacturer: (LAS) Lansen Systems, Sweden (0x3033)
                  type: Reserved for sensors (0x1d)
                   ver: 0x07
                driver: lansendw
Received telegram from: 00010206
          manufacturer: (LAS) Lansen Systems, Sweden (0x3033)
                  type: Other (0x00)
                   ver: 0x14
                driver: lansenpu
Received telegram from: 11772288
          manufacturer: (BMT) BMETERS, Italy (0x9b4)
                  type: Room sensor (eg temperature or humidity) (0x1b)
                   ver: 0x10
                driver: rfmamb
Received telegram from: 21242472
          manufacturer: (SAP) Sappel (0x4c30)
                  type: Oil meter (0x01)
                   ver: 0xd4
                driver: izar
Received telegram from: 66290778
          manufacturer: (DME) DIEHL Metering, Germany (0x11a5)
                  type: Unknown (0x66)
                   ver: 0x23
                driver: izar
Received telegram from: 64646464
          manufacturer: (DME) DIEHL Metering, Germany (0x11a5)
                  type: Water meter (0x07)
                   ver: 0x70
                driver: hydrus
Received telegram from: 65656565
          manufacturer: (DME) DIEHL Metering, Germany (0x11a5)
                  type: Warm Water (30°C-90°C) meter (0x06)
                   ver: 0x70
                driver: hydrus
Received telegram from: 86868686
          manufacturer: (BMT) BMETERS, Italy (0x9b4)
                  type: Water meter (0x07)
                   ver: 0x13
                driver: hydrodigit
Received telegram from: 72727272
          manufacturer: (AXI) UAB Axis Industries, Lithuania (0x709)
                  type: Water meter (0x07)
                   ver: 0x10
                driver: q400
Received telegram from: 22992299
          manufacturer: (EBZ) eBZ, Germany (0x145a)
                  type: Radio converter (meter side) (0x37)
                   ver: 0x02
                driver: ebzwmbe
Received telegram from: 77997799
          manufacturer: (ESY) EasyMeter (0x1679)
                  type: Radio converter (meter side) (0x37)
                   ver: 0x30
                driver: esyswm
Received telegram from: 77997799
          manufacturer: (ESY) EasyMeter (0x1679)
                  type: Radio converter (meter side) (0x37)
                   ver: 0x30
                driver: esyswm
Received telegram from: 55995599
          manufacturer: (EMH) EMH metering formerly EMH Elektrizitatszahler (0x15a8)
                  type: Electricity meter (0x02)
                   ver: 0x02
                driver: ehzp
Received telegram from: 004444dd
          manufacturer: (APT) Unknown (0x8614)
                  type: Gas meter (0x03)
                   ver: 0x03
                driver: apator08
Received telegram from: 74737271
          manufacturer: (BMT) BMETERS, Italy (0x9b4)
                  type: Water meter (0x07)
                   ver: 0x05
                driver: rfmtx1
Received telegram from: 20096221
          manufacturer: (DWZ) Lorenz, Germany (0x12fa)
                  type: Warm Water (30°C-90°C) meter (0x06)
                   ver: 0x02
                driver: waterstarm
Received telegram from: 78563412
          manufacturer: (AMT) INTEGRA METERING (0x5b4)
                  type: Water meter (0x07)
                   ver: 0xf1
                driver: topaseskr
Received telegram from: 95969798
          manufacturer: (APA) Apator, Poland (0x601)
                  type: Cold water meter (0x16)
                   ver: 0x01
                driver: ultrimis
Received telegram from: 12345679
          manufacturer: (EFE) Engelmann Sensor, Germany (0x14c5)
                  type: Heat meter (0x04)
                   ver: 0x00
                driver: sensostar
EOF

RES=$($PROG --logfile=$LOGFILE --t1 simulations/simulation_t1.txt 2>&1)

if [ ! "$RES" = "" ]
then
    echo ERROR: $TESTNAME
    echo Expected no output on stdout and stderr
    echo but got------------------
    echo $RES
    echo ---------------------
    exit 1
fi

RES=$(diff $LOGFILE $LOGFILE_EXPECTED)

if [ ! -z "$RES" ]
then
    echo ERROR: $TESTNAME
    echo -----------------
    diff $LOGFILE $LOGFILE_EXPECTED
    echo -----------------
    exit 1
else
    echo OK: $TESTNAME
fi

TESTNAME="Test listen and print any meter heard on stdout"
TESTRESULT="ERROR"

$PROG --t1 simulations/simulation_t1.txt 2> $LOGFILE

RES=$(diff $LOGFILE $LOGFILE_EXPECTED)

if [ ! -z "$RES"  ]
then
    echo ERROR: $TESTNAME
    echo -----------------
    diff $LOGFILE $LOGFILE_EXPECTED
    echo -----------------
    exit 1
else
    echo OK: $TESTNAME
fi
