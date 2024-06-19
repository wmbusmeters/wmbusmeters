#!/bin/sh

PROG="$1"

rm -rf testoutput
mkdir -p testoutput
TEST=testoutput

TESTNAME="Test date calculations"
TESTRESULT="ERROR"

HEX="5E44A51139259471410D7A720050052F2F0C06742400008C1006000000000C13823522008C2013494400000B3B0000000C2B000000000A5A22030A5E91020AA61800004C0619130000CC100600000000426CDF252F2F2F2F2F2F2F2F2F2F2F"

cat > $TEST/test_expected_unsorted.txt <<EOF
    "set_date":"2022-05-31",
    "set_1_fromd_utc":"2022-12-22T01:00:00Z",
    "set_2_fromdhm_utc":"2022-12-22T13:12:00Z",
    "set_3_fromdhms_utc":"2022-12-22T13:12:12Z",
    "set_4_fromdhmsz_utc":"2022-12-22T13:12:12Z",
    "set_5_fromdut_utc":"2022-12-22T12:12:12Z",
    "set_6_from_setdate_utc":"2022-05-31T01:00:00Z",
    "set_7_fromd_datetime":"2022-12-22 00:00",
    "set_8_fromdhm_datetime":"2022-12-22 12:12",
    "set_9_fromdhms_datetime":"2022-12-22 12:12",
    "set_10_fromdhmsz_datetime":"2022-12-22 12:12",
    "set_11_fromdhmsz_datetime":"2022-12-22 11:12",
    "set_12_from_setdate_datetime":"2022-05-31 00:00",
    "set_13_fromd_date":"2022-12-22",
    "set_14_fromdhm_date":"2022-12-22",
    "set_15_fromdhms_date":"2022-12-22",
    "set_16_fromdhmsz_date":"2022-12-22",
    "set_17_fromdhmsz_date":"2022-12-22",
    "set_18_from_setdate_date":"2022-05-31",
    "set_19_fromd_ut":1671670800,
    "set_20_fromdhm_ut":1671714720,
    "set_21_fromdhms_ut":1671714732,
    "set_22_fromdhmsz_ut":1671714732,
    "set_23_fromdhmsz_ut":1671711132,
    "set_24_from_setdate_ut":1653958800,
    "set_25_fromd_datetime":"2023-01-22 00:00",
    "set_26_fromdhm_datetime":"2023-01-22 12:12",
    "set_27_fromdhms_datetime":"2023-01-22 12:12",
    "set_28_fromdhmsz_datetime":"2023-01-22 12:12",
    "set_29_fromdhmsz_datetime":"2023-01-22 11:12",
    "set_30_from_setdate_datetime":"2022-06-30 00:00",
    "set_31_from_setdate2_datetime":"2022-07-31 07:33",
EOF

sort $TEST/test_expected_unsorted.txt > $TEST/test_expected.txt

# TZ=UTC+1 date --date 2022-12-22T12:12:12Z +%s  --> 1671711132 # UTC time
# TZ=UTC+1 date --date '2022-12-22 12:12:12' +%s --> 1671714732 # Local time in UTC+1 ie Stockholm no daylight savings.
# TZ=UTC+1 date --date '2022-12-22 12:12' +%s    --> 1671714720
# TZ=UTC+1 date --date '2022-12-22' +%s          --> 1671670800

# Test using a fixed timezone without daylight savings
# This is necessary to have the utc conversion be stable for testing.
TZ=UTC+1 $PROG --format=json --ppjson \
  --calculate_set_1_fromd_utc="'2022-12-22'" \
  --calculate_set_2_fromdhm_utc="'2022-12-22 12:12'" \
  --calculate_set_3_fromdhms_utc="'2022-12-22 12:12:12'" \
  --calculate_set_4_fromdhmsz_utc="'2022-12-22T12:12:12Z'" \
  --calculate_set_5_fromdut_utc='1671711132 ut' \
  --calculate_set_6_from_setdate_utc=set_date \
  \
  --calculate_set_7_fromd_datetime="'2022-12-22'" \
  --calculate_set_8_fromdhm_datetime="'2022-12-22 12:12'" \
  --calculate_set_9_fromdhms_datetime="'2022-12-22 12:12:12'" \
  --calculate_set_10_fromdhmsz_datetime="'2022-12-22T12:12:12Z'" \
  --calculate_set_11_fromdhmsz_datetime='1671711132 ut'\
  --calculate_set_12_from_setdate_datetime=set_date \
  \
  --calculate_set_13_fromd_date="'2022-12-22'" \
  --calculate_set_14_fromdhm_date="'2022-12-22 12:12'" \
  --calculate_set_15_fromdhms_date="'2022-12-22 12:12:12'" \
  --calculate_set_16_fromdhmsz_date="'2022-12-22T12:12:12Z'" \
  --calculate_set_17_fromdhmsz_date='1671711132 ut'\
  --calculate_set_18_from_setdate_date=set_date \
  \
  --calculate_set_19_fromd_ut="'2022-12-22'" \
  --calculate_set_20_fromdhm_ut="'2022-12-22 12:12'" \
  --calculate_set_21_fromdhms_ut="'2022-12-22 12:12:12'" \
  --calculate_set_22_fromdhmsz_ut="'2022-12-22T12:12:12Z'" \
  --calculate_set_23_fromdhmsz_ut='1671711132 ut'\
  --calculate_set_24_from_setdate_ut=set_date \
  \
  --calculate_set_25_fromd_datetime="'2022-12-22' + 1 month" \
  --calculate_set_26_fromdhm_datetime="'2022-12-22 12:12' + 1 month" \
  --calculate_set_27_fromdhms_datetime="'2022-12-22 12:12:12' + 1 month" \
  --calculate_set_28_fromdhmsz_datetime="'2022-12-22T12:12:12Z' + 1 month" \
  --calculate_set_29_fromdhmsz_datetime='1671711132 ut + 1 month'\
  --calculate_set_30_from_setdate_datetime='set_date + 1 month'\
\
  --calculate_set_31_from_setdate2_datetime="set_30_from_setdate_datetime + 1 month + 7 h + '00:33'"\
      "$HEX" \
      Moo sharky774 71942539 NOKEY | grep \"set_ | sort > $TEST/test_output.txt

if [ "$?" = "0" ]
then
    diff $TEST/test_expected.txt $TEST/test_output.txt
    if [ "$?" = "0" ]
    then
        echo "OK: $TESTNAME"
        TESTRESULT="OK"
    else
        if [ "$USE_MELD" = "true" ]
        then
            meld $TEST/test_expected.txt $TEST/test_output.txt
        fi
    fi
fi

if [ "$TESTRESULT" = "ERROR" ]; then echo ERROR: $TESTNAME;  exit 1; fi
