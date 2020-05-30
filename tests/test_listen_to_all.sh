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
          manufacturer: (SON) Sontex, Switzerland
           device type: Warm Water (30°C-90°C) meter
Received telegram from: 11111111
          manufacturer: (SON) Sontex, Switzerland
           device type: Water meter
Received telegram from: 12345699
          manufacturer: (SEN) Sensus Metering Systems, Germany
           device type: Water meter
Received telegram from: 33225544
          manufacturer: (SEN) Sensus Metering Systems, Germany
           device type: Water meter
Received telegram from: 10101010
          manufacturer: (APA) Apator, Poland
           device type: Electricity meter
Received telegram from: 34333231
          manufacturer: (TCH) Techem Service
           device type: Warm water
Received telegram from: 58234965
          manufacturer: (TCH) Techem Service
           device type: Heat meter
Received telegram from: 11776622
          manufacturer: (TCH) Techem Service
           device type: Heat Cost Allocator
Received telegram from: 88018801
          manufacturer: (INE) INNOTAS Elektronik, Germany
           device type: Heat Cost Allocator
Received telegram from: 00010203
          manufacturer: (LAS) Lansen Systems, Sweden
           device type: Room sensor (eg temperature or humidity)
Received telegram from: 11772288
          manufacturer: (BMT) BMETERS, Italy
           device type: Room sensor (eg temperature or humidity)
Received telegram from: 21242472
          manufacturer: (SAP) Sappel
           device type: Oil meter
Received telegram from: 66290778
          manufacturer: (DME) DIEHL Metering, Germany
           device type: Unknown
Received telegram from: 64646464
          manufacturer: (DME) DIEHL Metering, Germany
           device type: Water meter
Received telegram from: 86868686
          manufacturer: (BMT) BMETERS, Italy
           device type: Water meter
Received telegram from: 72727272
          manufacturer: (AXI) UAB Axis Industries, Lithuania
           device type: Water meter
Received telegram from: 22992299
          manufacturer: (EBZ) eBZ, Germany
           device type: Radio converter (meter side)
Received telegram from: 77997799
          manufacturer: (ESY) EasyMeter
           device type: Radio converter (meter side)
Received telegram from: 77997799
          manufacturer: (ESY) EasyMeter
           device type: Radio converter (meter side)
Received telegram from: 55995599
          manufacturer: (EMH) EMH metering formerly EMH Elektrizitatszahler
           device type: Electricity meter
Received telegram from: 004444dd
          manufacturer: (APT) Unknown
           device type: Gas meter
Received telegram from: 74737271
          manufacturer: (BMT) BMETERS, Italy
           device type: Water meter
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

$PROG --t1 simulations/simulation_t1.txt 2>&1 > $LOGFILE

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
