#!/bin/bash

PROG="$1"

mkdir -p testoutput

TEST=testoutput

TESTNAME="Test incorrect decryption keys"
TESTRESULT="ERROR"

cat simulations/simulation_t1.txt | grep '^{' > $TEST/test_expected.txt
$PROG --format=json simulations/simulation_t1.txt \
      MyWarmWater supercom587 12345678 11111111111111111111111111111111 \
      MyColdWater supercom587 11111111 11111111111111111111111111111111 \
      MoreWater   iperl       12345699 11111111111111111111111111111111 \
      WaterWater  iperl       33225544 11111111111111111111111111111111 \
      | grep warning > $TEST/test_output.txt

cat <<EOF > $TEST/test_expected.txt
(Mode5) warning: decryption received non-multiple of 16 bytes! Got 148 bytes shrinking message to 144 bytes.
(Mode5) warning: telegram payload does not start with 2F2F (did you use the correct encryption key?)
(Mode5) warning: decryption received non-multiple of 16 bytes! Got 148 bytes shrinking message to 144 bytes.
(Mode5) warning: telegram payload does not start with 2F2F (did you use the correct encryption key?)
(Mode5) warning: telegram payload does not start with 2F2F (did you use the correct encryption key?)
(Mode5) warning: decryption received non-multiple of 16 bytes! Got 10 bytes shrinking message to 0 bytes.
(Mode5) warning: telegram payload does not start with 2F2F (did you use the correct encryption key?)
EOF

# Check that the program does not crash!
if [ "$?" == "0" ]
then
    diff $TEST/test_expected.txt $TEST/test_output.txt
    if [ "$?" == "0" ]
    then
        echo "OK: $TESTNAME"
        TESTRESULT="OK"
    fi
fi

if [ "$TESTRESULT" = "ERROR" ]; then echo ERROR: $TESTNAME;  exit 1; fi
