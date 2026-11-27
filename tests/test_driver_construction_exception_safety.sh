#!/bin/sh

. tests/include.sh

PROG="$1"
TEST=testoutput
mkdir -p $TEST

TESTNAME="Test a failed field construction does not corrupt other meters of the same driver"
TESTRESULT="ERROR"

cat > $TEST/driver.xmq <<EOF
driver {
    name           = brokenfield
    meter_type     = WaterMeter
    default_fields = name,id,timestamp
    detect {
        mvt = KAM,1b,16
    }
    fields {
        field {
            name = broken
            info = 'deliberately missing quantity, throws during construction'
            match { difvifkey = 02FF20 }
        }
    }
}
EOF

# we need two meters to reproduce
cat > $TEST/test_expected.txt <<EOF
{"_":"telegram","media":"cold water","driver":"brokenfield","name":"Meter1","id":"76348799","timestamp":"1111-11-11T11:11:11Z"}
{"_":"telegram","media":"cold water","driver":"brokenfield","name":"Meter2","id":"76348799","timestamp":"1111-11-11T11:11:11Z"}
EOF

$PROG --format=json 2A442D2C998734761B168D2091D37CAC21576C78_02FF207100041308190000441308190000615B7F616713 \
      Meter1 $TEST/driver.xmq 76348799 NOKEY \
      Meter2 $TEST/driver.xmq "*" NOKEY > $TEST/test_output.txt 2>&1

cat $TEST/test_output.txt | grep -a '^{' | sed 's/"timestamp":"....-..-..T..:..:..Z"/"timestamp":"1111-11-11T11:11:11Z"/' > $TEST/test_response.txt

if grep -qa "use-after-free\|double-free\|heap-buffer-overflow\|SEGV\|LeakSanitizer" $TEST/test_output.txt
then
    printERROR "$TESTNAME"
    echo "wmbusmeters hit a memory-safety error or leak:"
    cat $TEST/test_output.txt
    exit 1
fi

diff $TEST/test_expected.txt $TEST/test_response.txt
if [ "$?" = "0" ]
then
    printOK "$TESTNAME"
    TESTRESULT="OK"
else
    printERROR "$TESTNAME"
    echo "Expected both meters to still produce output (even if the broken field is just missing):"
    cat $TEST/test_output.txt
    exit 1
fi
