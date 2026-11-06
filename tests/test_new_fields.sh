#!/bin/sh

. tests/include.sh

PROG="$1"

mkdir -p testoutput
TEST=testoutput

TESTNAME="Test new fields trigger new discovery"
TESTRESULT="ERROR"

rm -f $TEST/thelogg

cat > $TEST/simulation_input.txt <<EOF

# The meter values every minute or so.
telegram=264465320100009035087A53000000_0B6E7801004B6E610200426C3F3C326CFFFF046D18135B3A

# The daily model/version + storage nr 8 telegram, sent once per day.
telegram=234465320100009035087A58000000_8B046E58010082046C5E3901FD0C10046D220F5C3A

# This one should not trigger metershell
telegram=264465320100009035087A53000000_0B6E7802004B6E610200426C3F3C326CFFFF046D18135B3A

# Nor this one.
telegram=234465320100009035087A58000000_8B046E58010082046C5E3901FD0C11046D220F5C3A

EOF

$PROG --format=fields --selectfields=current_consumption_hca,device_datetime,model_version --metershell='echo "$METER_JSON" >> '"$TEST/thelogg" $TEST/simulation_input.txt Q qcaloric 90000001 NOKEY > $TEST/test_output.txt 2>&1

jq 'with_entries(select(.key == "model_version" or .key == "current_consumption_hca" or .key == "device_datetime"))' $TEST/thelogg > $TEST/test_metershell_output.txt

# Here we see only two telegrams that add new fields.
cat > $TEST/test_expected_metershell.txt <<EOF
{
  "current_consumption_hca": 178,
  "device_datetime": "2026-10-27 19:24"
}
{
  "current_consumption_hca": 178,
  "device_datetime": "2026-10-28 15:34",
  "model_version": "10"
}
EOF

# Here we see output from 4 telegrams.
cat > $TEST/test_expected_stdout.txt <<EOF
178;2026-10-27 19:24;null
178;2026-10-28 15:34;10
278;2026-10-27 19:24;10
278;2026-10-28 15:34;11
EOF

if [ "$?" = "0" ]
then
    diff $TEST/test_expected_stdout.txt $TEST/test_output.txt
    if [ "$?" != "0" ]
    then
        if [ "$USE_MELD" = "true" ]
        then
            meld $TEST/test_expected_stdout.txt $TEST/test_output.txt
        fi
        printERROR "$TESTNAME"
        exit 1
    fi

    diff $TEST/test_expected_metershell.txt $TEST/test_metershell_output.txt
    if [ "$?" != "0" ]
    then
        if [ "$USE_MELD" = "true" ]
        then
            meld $TEST/test_expected_metershell.txt $TEST/test_metershell_output.txt
        fi
        printERROR "$TESTNAME"
        exit 1
    fi

    printOK "$TESTNAME"
    TESTRESULT="OK"
fi

if [ "$TESTRESULT" = "ERROR" ]
then
    printERROR "$TESTNAME"
    exit 1
fi
