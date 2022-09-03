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
        echo "ERROR: $TESTNAME $0"
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
Auto driver  : multical21
Best driver  :  00/00
Using driver : multical21 00/00
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
    "status":null,
    "total_m3":null,
    "target_m3":null,
    "current_status":null,
    "time_dry":null,
    "time_reversed":null,
    "time_leaking":null,
    "time_bursting":null,
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
Auto driver  : multical21
Best driver  :  00/00
Using driver : multical21 00/00
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
    "status":null,
    "total_m3":null,
    "target_m3":null,
    "current_status":null,
    "time_dry":null,
    "time_reversed":null,
    "time_leaking":null,
    "time_bursting":null,
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
Auto driver  : multical21
Best driver  : multical21 12/12
Using driver : multical21 00/00
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
022   : 20 combinable vif (PerSecond)
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
    "status":"DRY",
    "total_m3":6.408,
    "target_m3":6.408,
    "flow_temperature_c":127,
    "external_temperature_c":19,
    "current_status":"DRY",
    "time_dry":"22-31 days",
    "time_reversed":"",
    "time_leaking":"",
    "time_bursting":"",
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
Auto driver  : multical21
Best driver  : multical21 12/12
Using driver : multical21 00/00
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
    "status":"DRY",
    "total_m3":6.409,
    "target_m3":6.409,
    "flow_temperature_c":127,
    "external_temperature_c":22,
    "current_status":"DRY",
    "time_dry":"22-31 days",
    "time_reversed":"",
    "time_leaking":"",
    "time_bursting":"",
    "timestamp":"1111-11-11T11:11:11Z"
}
EOF

$PROG --analyze=28F64A24988064A079AA2C807D6102AE 23442D2C998734761B168D20983081B2227A6FA1F10E1B79B5EB4B17E81F930E937EE06C > $TEST/test_output.txt 2>&1

performCheck

########################################################################################################################
########################################################################################################################
########################################################################################################################

TESTNAME="Test analyze CBC IV (no-key)"
TESTRESULT="ERROR"

cat > $TEST/test_expected.txt <<EOF
Auto driver  : supercom587
Best driver  :  00/00
Using driver : supercom587 00/00
000   : ae length (174 bytes)
001   : 44 dll-c (from meter SND_NR)
002   : ee4d dll-mfct (SON)
004   : 77777777 dll-id (77777777)
008   : 3c dll-version
009   : 07 dll-type (Water meter)
010   : 7a tpl-ci-field (EN 13757-3 Application Layer (short tplh))
011   : 44 tpl-acc-field
012   : 00 tpl-sts-field (OK)
013   : a025 tpl-cfg 25a0 (synchronous AES_CBC_IV nb=10 cntn=0 ra=0 hc=0 )
015 CE: E78F4A01F9DCA029EDA03BA452686E8FA917507B29E5358B52D77C111EA4C41140290523F3F6B9F9261705E041C0CA41305004605F42D6C9464E5A04EEE227510BD0DC0983C665C3A5E4739C2082975476AC637BCDD39766AEF030502B6A7697BE9E1C49AF535C15470FCF8ADA36CAB9D0B2A1A8690F8DDCF70859F18B3414D8315B311A0AFA57325531587CB7E9CC110E807F24C190D7E635BEDAF4CAE8A161 encrypted

{
    "media":"water",
    "meter":"supercom587",
    "name":"",
    "id":"77777777",
    "total_m3":0,
    "timestamp":"1111-11-11T11:11:11Z"
}
EOF

$PROG --analyze AE44EE4D777777773C077A4400A025E78F4A01F9DCA029EDA03BA452686E8FA917507B29E5358B52D77C111EA4C41140290523F3F6B9F9261705E041C0CA41305004605F42D6C9464E5A04EEE227510BD0DC0983C665C3A5E4739C2082975476AC637BCDD39766AEF030502B6A7697BE9E1C49AF535C15470FCF8ADA36CAB9D0B2A1A8690F8DDCF70859F18B3414D8315B311A0AFA57325531587CB7E9CC110E807F24C190D7E635BEDAF4CAE8A161  > $TEST/test_output.txt 2>&1

performCheck

########################################################################################################################
########################################################################################################################
########################################################################################################################

#TESTNAME="Test analyze CBC IV (with-key)"
#TESTRESULT="ERROR"

#cat > $TEST/test_expected.txt <<EOF
#EOF

#$PROG --analyze=5065747220486F6C79737A6577736B69 AE44EE4D777777773C077A4400A025E78F4A01F9DCA029EDA03BA452686E8FA917507B29E5358B52D77C111EA4C41140290523F3F6B9F9261705E041C0CA41305004605F42D6C9464E5A04EEE227510BD0DC0983C665C3A5E4739C2082975476AC637BCDD39766AEF030502B6A7697BE9E1C49AF535C15470FCF8ADA36CAB9D0B2A1A8690F8DDCF70859F18B3414D8315B311A0AFA57325531587CB7E9CC110E807F24C190D7E635BEDAF4CAE8A161  > $TEST/test_output.txt 2>&1

#performCheck
