#!/bin/bash

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
Received telegram from: 20202020
          manufacturer: (APA) Apator, Poland
           device type: Water meter
Received telegram from: 21202020
          manufacturer: (APA) Apator, Poland
           device type: Water meter
Received telegram from: 22202020
          manufacturer: (APA) Apator, Poland
           device type: Water meter
Received telegram from: 23202020
          manufacturer: (APA) Apator, Poland
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
EOF

EXPECTED=$(cat $LOGFILE_EXPECTED)

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

GOT=$(cat $LOGFILE)


if [ ! "$GOT" = "$EXPECTED" ]
then
    echo ERROR: $TESTNAME
    echo GOT--------------
    echo $GOT
    echo EXPECTED---------
    echo $EXPECTED
    echo -----------------
    exit 1
else
    echo OK: $TESTNAME
fi


TESTNAME="Test listen and print any meter heard on stdout"
TESTRESULT="ERROR"

GOT=$($PROG --t1 simulations/simulation_t1.txt 2>&1)

if [ ! "$GOT" = "$EXPECTED" ]
then
    echo ERROR: $TESTNAME
    echo GOT--------------
    echo $GOT
    echo EXPECTED---------
    echo $EXPECTED
    echo -----------------
    exit 1
else
    echo OK: $TESTNAME
fi
