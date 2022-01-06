#!/bin/sh

PROG="$1"

mkdir -p testoutput
TEST=testoutput

TESTNAME="Test meter with unknown driver"
TESTRESULT="ERROR"

METERS="Dorren lansendw 00010205 NOKEY Forren lansensm 00010206 NOKEY"

cat > $TEST/test_expected.txt <<EOF
(meter) Dorren: meter detection did not match the selected driver lansendw! correct driver is: unknown!
(meter) Not printing this warning again for id: 00010205 mfct: (LAS) Lansen Systems, Sweden (0x3033) type: Unknown (0xff) ver: 0x07
(meter) please consider opening an issue at https://github.com/weetmuts/wmbusmeters/
(meter) to add support for this unknown mfct,media,version combination
{"media":"Unknown","meter":"lansendw","name":"Dorren","id":"00010205","status":"OPEN","counter_a_int":23,"counter_b_int":0,"timestamp":"1111-11-11T11:11:11Z"}
(meter) Forren: meter detection did not match the selected driver lansensm! correct driver is: lansendw
(meter) Not printing this warning again for id: 00010206 mfct: (LAS) Lansen Systems, Sweden (0x3033) type: Reserved for sensors (0x1d) ver: 0x07
{"media":"reserved","meter":"lansensm","name":"Forren","id":"00010206","status":"OK","timestamp":"1111-11-11T11:11:11Z"}
{"media":"Unknown","meter":"lansendw","name":"Dorren","id":"00010205","status":"OPEN","counter_a_int":23,"counter_b_int":0,"timestamp":"1111-11-11T11:11:11Z"}
{"media":"reserved","meter":"lansensm","name":"Forren","id":"00010206","status":"OK","timestamp":"1111-11-11T11:11:11Z"}
EOF

$PROG --format=json --ignoreduplicates=false --usestdoutforlogging simulations/simulation_unknown.txt $METERS  > $TEST/test_output.txt
if [ "$?" = "0" ]
then
    cat $TEST/test_output.txt | sed 's/"timestamp":"....-..-..T..:..:..Z"/"timestamp":"1111-11-11T11:11:11Z"/' > $TEST/test_responses.txt
    diff $TEST/test_expected.txt $TEST/test_responses.txt
    if [ "$?" = "0" ]
    then
        echo OK: $TESTNAME
        TESTRESULT="OK"
    else
        TESTRESULT="ERROR"
    fi
fi


if [ "$TESTRESULT" = "ERROR" ]
then
    echo ERROR: $TESTNAME
    exit 1
fi
