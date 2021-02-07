#!/bin/sh

PROG="$1"

mkdir -p testoutput
TEST=testoutput

TESTNAME="Test Izas meters"
TESTRESULT="ERROR"
LOGFILE=$TEST/logfile
LOGFILE_EXPECTED=$TEST/logfile_expected

mkdir -p $TEST
rm -f $LOGFILE

METERS="IzarWater   izar        21242472 NOKEY
        IzarWater2  izar        66290778 NOKEY
        IzarWater3  izar        19790778 NOKEY
        IzarWater4  izar        2124589c NOKEY"

cat simulations/simulation_izars.txt | grep '^{' > $TEST/test_expected.txt
$PROG --format=json simulations/simulation_izars.txt $METERS > $TEST/test_output.txt 2> $TEST/test_stderr.txt
if [ "$?" = "0" ]
then
    cat $TEST/test_output.txt | sed 's/"timestamp":"....-..-..T..:..:..Z"/"timestamp":"1111-11-11T11:11:11Z"/' > $TEST/test_responses.txt
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

TESTNAME="Test listen to all izas"
TESTRESULT="ERROR"

cat > $LOGFILE_EXPECTED <<EOF
No meters configured. Printing id:s of all telegrams heard!
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
Received telegram from: 19790778
          manufacturer: (DME) DIEHL Metering, Germany (0x11a5)
                  type: Breaker (electricity) (0x20)
                   ver: 0x48
                driver: izar
Received telegram from: 2124589c
          manufacturer: (SAP) Sappel (0x4c30)
                  type: Heat meter (0x04)
                   ver: 0x0c
                driver: izar
EOF

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
