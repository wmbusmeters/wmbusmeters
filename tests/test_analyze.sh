#!/bin/sh

PROG="$1"
TEST=testoutput
mkdir -p $TEST

performCheck() {
if [ "$?" = "0" ]
then
    cat $TEST/test_output.txt | sed 's/"timestamp":"....-..-..T..:..:..Z"/"timestamp":"1111-11-11T11:11:11Z"/' > $TEST/test_response.txt
    diff $TEST/test_expected.txt $TEST/test_response.txt
    if [ "$?" = "0" ]
    then
        echo "OK: $TESTNAME"
        TESTRESULT="OK"
    else
        if [ "$USE_MELD" = "true" ]
        then
            meld $TEST/test_expected.txt $TEST/test_response.txt
        fi
    fi
else
    echo "ERROR: $TESTNAME $0"
    echo "wmbusmeters returned error code: $?"
    cat $TEST/test_output.txt
fi
}

########################################################################################################################
########################################################################################################################
########################################################################################################################

TESTNAME="Test analyze encrypted (no-key) ctr full telegram"
TESTRESULT="ERROR"

cat > $TEST/test_expected.txt <<EOF
Auto driver    : multical21
Similar driver : unknown 00/00
Using driver   : multical21 00/00
000   : 2a length (42 bytes)
001   : 44 dll-c (from meter SND_NR)
002   : 2d2c dll-mfct (KAM)
004   : 99873476 dll-id (76348799)
008   : 1b dll-version
009   : 16 dll-type (Cold water meter)
010   : 8d ell-ci-field (ELL: Extended Link Layer II (8 Byte))
011   : 20 ell-cc (slow_resp sync)
012   : 91 ell-acc
013   : d37cac21 sn (AES_CTR)
017   : e1d6 payload crc (calculated a528 ERROR)
019 CE: 8CDAFFCD3DC452BD802913FF7B1706CA9E355D6C2701CC24 failed decryption. Wrong key?

{
    "media":"cold water",
    "meter":"multical21",
    "name":"",
    "id":"76348799",
    "timestamp":"1111-11-11T11:11:11Z"
}
EOF

$PROG --analyze 2A442D2C998734761B168D2091D37CAC21E1D68CDAFFCD3DC452BD802913FF7B1706CA9E355D6C2701CC24 > $TEST/test_output.txt 2>&1

performCheck

########################################################################################################################
########################################################################################################################
########################################################################################################################

TESTNAME="Test analyze encrypted (no-key) ctr compact telegram"
TESTRESULT="ERROR"

cat > $TEST/test_expected.txt <<EOF
Auto driver    : multical21
Similar driver : unknown 00/00
Using driver   : multical21 00/00
000   : 23 length (35 bytes)
001   : 44 dll-c (from meter SND_NR)
002   : 2d2c dll-mfct (KAM)
004   : 99873476 dll-id (76348799)
008   : 1b dll-version
009   : 16 dll-type (Cold water meter)
010   : 8d ell-ci-field (ELL: Extended Link Layer II (8 Byte))
011   : 20 ell-cc (slow_resp sync)
012   : 98 ell-acc
013   : 3081b222 sn (AES_CTR)
017   : 7a6f payload crc (calculated 54d6 ERROR)
019 CE: A1F10E1B79B5EB4B17E81F930E937EE06C failed decryption. Wrong key?

{
    "media":"cold water",
    "meter":"multical21",
    "name":"",
    "id":"76348799",
    "timestamp":"1111-11-11T11:11:11Z"
}
EOF

$PROG --analyze 23442D2C998734761B168D20983081B2227A6FA1F10E1B79B5EB4B17E81F930E937EE06C > $TEST/test_output.txt 2>&1

performCheck

########################################################################################################################
########################################################################################################################
########################################################################################################################

TESTNAME="Test analyze encrypted (with-key) ctr full telegram"
TESTRESULT="ERROR"

cat > $TEST/test_expected.txt <<EOF
Auto driver    : multical21
Similar driver : multical21 12/12
Using driver   : multical21 00/00
000   : 2a length (42 bytes)
001   : 44 dll-c (from meter SND_NR)
002   : 2d2c dll-mfct (KAM)
004   : 99873476 dll-id (76348799)
008   : 1b dll-version
009   : 16 dll-type (Cold water meter)
010   : 8d ell-ci-field (ELL: Extended Link Layer II (8 Byte))
011   : 20 ell-cc (slow_resp sync)
012   : 91 ell-acc
013   : d37cac21 sn (AES_CTR)
017   : 576c payload crc (calculated 576c OK)
019   : 78 tpl-ci-field (EN 13757-3 Application Layer (no tplh))
020   : 02 dif (16 Bit Integer/Binary Instantaneous value)
021   : FF vif (Manufacturer specific)
022   : 20 vife (?)
023 C!: 7100 ("status":"DRY") ("current_status":"DRY") ("time_dry":"22-31 days") ("time_reversed":"") ("time_leaking":"") ("time_bursting":"")
025   : 04 dif (32 Bit Integer/Binary Instantaneous value)
026   : 13 vif (Volume l)
027 C!: 08190000 ("total_m3":6.408)
031   : 44 dif (32 Bit Integer/Binary Instantaneous value storagenr=1)
032   : 13 vif (Volume l)
033 C!: 08190000 ("target_m3":6.408)
037   : 61 dif (8 Bit Integer/Binary Minimum value storagenr=1)
038   : 5B vif (Flow temperature °C)
039 C!: 7F ("flow_temperature_c":127)
040   : 61 dif (8 Bit Integer/Binary Minimum value storagenr=1)
041   : 67 vif (External temperature °C)
042 C!: 13 ("external_temperature_c":19)

{
    "media":"cold water",
    "meter":"multical21",
    "name":"",
    "id":"76348799",
    "external_temperature_c":19,
    "flow_temperature_c":127,
    "target_m3":6.408,
    "total_m3":6.408,
    "current_status":"DRY",
    "status":"DRY",
    "time_bursting":"",
    "time_dry":"22-31 days",
    "time_leaking":"",
    "time_reversed":"",
    "timestamp":"1111-11-11T11:11:11Z"
}
EOF

$PROG --analyze=28F64A24988064A079AA2C807D6102AE 2A442D2C998734761B168D2091D37CAC21E1D68CDAFFCD3DC452BD802913FF7B1706CA9E355D6C2701CC24 > $TEST/test_output.txt 2>&1

performCheck

########################################################################################################################
########################################################################################################################
########################################################################################################################

TESTNAME="Test analyze encrypted (no-key) ctr compact telegram"
TESTRESULT="ERROR"

cat > $TEST/test_expected.txt <<EOF
Auto driver    : multical21
Similar driver : multical21 12/12
Using driver   : multical21 00/00
000   : 23 length (35 bytes)
001   : 44 dll-c (from meter SND_NR)
002   : 2d2c dll-mfct (KAM)
004   : 99873476 dll-id (76348799)
008   : 1b dll-version
009   : 16 dll-type (Cold water meter)
010   : 8d ell-ci-field (ELL: Extended Link Layer II (8 Byte))
011   : 20 ell-cc (slow_resp sync)
012   : 98 ell-acc
013   : 3081b222 sn (AES_CTR)
017   : c33d payload crc (calculated c33d OK)
019   : 79 tpl-ci-field (EN 13757-3 Application Layer with Compact frame (no tplh))
020   : eda8 format signature
022   : e475 data crc
024 C!: 7100 ("status":"DRY") ("current_status":"DRY") ("time_dry":"22-31 days") ("time_reversed":"") ("time_leaking":"") ("time_bursting":"")
026 C!: 09190000 ("total_m3":6.409)
030 C!: 09190000 ("target_m3":6.409)
034 C!: 7F ("flow_temperature_c":127)
035 C!: 16 ("external_temperature_c":22)

{
    "media":"cold water",
    "meter":"multical21",
    "name":"",
    "id":"76348799",
    "external_temperature_c":22,
    "flow_temperature_c":127,
    "target_m3":6.409,
    "total_m3":6.409,
    "current_status":"DRY",
    "status":"DRY",
    "time_bursting":"",
    "time_dry":"22-31 days",
    "time_leaking":"",
    "time_reversed":"",
    "timestamp":"1111-11-11T11:11:11Z"
}
EOF

$PROG --analyze=28F64A24988064A079AA2C807D6102AE 23442D2C998734761B168D20983081B2227A6FA1F10E1B79B5EB4B17E81F930E937EE06C > $TEST/test_output.txt 2>&1

performCheck
