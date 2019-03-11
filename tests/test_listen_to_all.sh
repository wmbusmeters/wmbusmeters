#!/bin/bash

PROG="$1"

if [ "$PROG" = "" ]
then
    echo Please supply the binary to be tested as the first argument.
    exit 1
fi

TEST=testoutput
LOGFILE=$TEST/logfile

mkdir -p $TEST
rm -f $LOGFILE

EXPECTED=$(cat <<EOF
No meters configured. Printing id:s of all telegrams heard!

Received telegram from: 12345678
          manufacturer: (SON) Sontex SA
           device type: Warm Water (30°C-90°C) meter
Received telegram from: 11111111
          manufacturer: (SON) Sontex SA
           device type: Water meter
Received telegram from: 12345699
          manufacturer: (SEN) Sensus GmbH
           device type: Water meter
Received telegram from: 33225544
          manufacturer: (SEN) Sensus GmbH
           device type: Water meter
EOF
           )

RES=$($PROG --logfile=$LOGFILE --t1 simulations/simulation_t1.txt 2>&1)

if [ ! "$RES" = "" ]
then
    ERRORS=true
    echo Expected no output on stdout and stderr
    echo but got------------------
    echo $RES
    echo ---------------------
fi

GOT=$(cat $LOGFILE)

if [ ! "$GOT" = "$EXPECTED" ]
then
    echo GOT--------------
    echo $GOT
    echo EXPECTED---------
    echo $EXPECTED
    echo -----------------
    exit 1
else
    echo OK: listen to all with logfile
fi


GOT=$($PROG --t1 simulations/simulation_t1.txt 2>&1)

if [ ! "$GOT" = "$EXPECTED" ]
then
    echo GOT--------------
    echo $GOT
    echo EXPECTED---------
    echo $EXPECTED
    echo -----------------
    exit 1
else
    echo OK: listen to all with stdout
fi
