#!/bin/sh

PROG="$1"

mkdir -p testoutput
TEST=testoutput

TESTNAME="Test Izars meters"
TESTRESULT="ERROR"
LOGFILE=$TEST/logfile
LOGFILE_EXPECTED=$TEST/logfile_expected

mkdir -p $TEST
rm -f $LOGFILE

METERS="IzarWater   izar        21242472 NOKEY
        IzarWater2  izar        66236629 NOKEY
        IzarWater3  izar        20481979 NOKEY
        IzarWater4  izar        2124589c NOKEY
        IzarWater5  izar        20e4ffde NOKEY
        IzarWater6  auto        48500375 NOKEY
        IzarWater7  izar        208962f3 NOKEY"

cat simulations/simulation_izars.txt | grep '^{' | jq --sort-keys . > $TEST/test_expected.txt
$PROG --format=json simulations/simulation_izars.txt $METERS  2> $TEST/test_stderr.txt | jq --sort-keys . > $TEST/test_output.txt
if [ "$?" = "0" ]
then
    cat $TEST/test_output.txt | sed 's/"timestamp": "....-..-..T..:..:..Z"/"timestamp": "1111-11-11T11:11:11Z"/' > $TEST/test_responses.txt
    diff $TEST/test_expected.txt $TEST/test_responses.txt
    if [ "$?" = "0" ]
    then
        echo OK json: $TESTNAME
        TESTRESULT="OK"
    else
        TESTRESULT="ERROR"
    fi
else
    echo "wmbusmeters returned error code: $?"
    cat $TEST/test_output.txt
    cat $TEST/test_stderr.txt
fi

if [ "$TESTRESULT" = "ERROR" ]
then
    echo ERROR: $TESTNAME
    exit 1
fi

TESTNAME="Test listen to all izars"
TESTRESULT="ERROR"

cat > $LOGFILE_EXPECTED <<EOF
No meters configured. Printing id:s of all telegrams heard!
Received telegram from: 21242472
          manufacturer: (SAP) Sappel (0x4c30)
                  type: Water meter (0x07)
                   ver: 0x00
                driver: izar
Received telegram from: 66236629
          manufacturer: (DME) DIEHL Metering, Germany (0x11a5)
                  type: Water meter (0x07)
                   ver: 0x78
                driver: izar
Received telegram from: 20481979
          manufacturer: (DME) DIEHL Metering, Germany (0x11a5)
                  type: Water meter (0x07)
                   ver: 0x78
                driver: izar
Received telegram from: 2124589c
          manufacturer: (SAP) Sappel (0x4c30)
                  type: Water meter (0x07)
                   ver: 0x00
                driver: izar
Received telegram from: 20e4ffde
          manufacturer: (SAP) Sappel (0x4c30)
                  type: Water meter (0x07)
                   ver: 0x00
                driver: izar
Received telegram from: 48500375
          manufacturer: (HYD) Hydrometer (0x2324)
                  type: Water meter (0x07)
                   ver: 0x86
                driver: izar
Received telegram from: 208962f3
          manufacturer: (SAP) Sappel (0x4c30)
                  type: Water meter (0x07)
                   ver: 0x00
                driver: izar
-----------------EOF

RES=$($PROG --logfile=$LOGFILE --t1 simulations/simulation_izars.txt 2>&1)

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
