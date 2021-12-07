#!/bin/sh

PROG="$1"
TEST=testoutput
mkdir -p $TEST

TESTNAME="Test analyze compact telegram"
TESTRESULT="ERROR"

cat > $TEST/test_expected.txt <<EOF
000   : 23 length (35 bytes)
001   : 44 dll-c (from meter SND_NR)
002   : 2d2c dll-mfct (KAM)
004   : 99873476 dll-id (76348799)
008   : 1b dll-version
009   : 16 dll-type (Cold water meter)
010   : 8d ell-ci-field (ELL: Extended Link Layer II (8 Byte))
011   : 20 ell-cc (slow_resp sync)
012   : 87 ell-acc
013   : d19ead21 sn (AES_CTR)
017   : 7f17 payload crc (calculated 7f17 OK)
019   : 79 tpl-ci-field (EN 13757-3 Application Layer with Compact frame (no tplh))
020   : eda8 format signature
022   : 6ab6 data crc
024 C!: 7100 info codes (DRY(dry 22-31 days))
026 C!: 08190000 total consumption (6.408000 m3)
030 C!: 08190000 target consumption (6.408000 m3)
034 C!: 7F flow temperature (127.000000 °C)
035 C!: 13 external temperature (19.000000 °C)
Best driver flowiq3100 12/12
EOF

$PROG --analyze=plain 23442D2C998734761B168D2087D19EAD217F1779EDA86AB6710008190000081900007F13 Foof flowiq3100 11111111 NOKEY 2> $TEST/test_output.txt 1>&2

if [ "$?" = "0" ]
then
    diff $TEST/test_expected.txt $TEST/test_output.txt
    if [ "$?" = "0" ]
    then
        echo OK: $TESTNAME
        TESTRESULT="OK"
    fi
else
    echo ERROR: $TESTNAME
    echo "wmbusmeters returned error code: $?"
    cat $TEST/test_output.txt
fi

TESTNAME="Test analyze find best driver"
TESTRESULT="ERROR"

cat > $TEST/test_expected.txt <<EOF
Best driver flowiq3100 12/12
EOF

$PROG --analyze=plain 23442D2C998734761B168D2087D19EAD217F1779EDA86AB6710008190000081900007F13 2> $TEST/test_output.txt 1>&2

if [ "$?" = "0" ]
then
    diff $TEST/test_expected.txt $TEST/test_output.txt
    if [ "$?" = "0" ]
    then
        echo OK: $TESTNAME
        TESTRESULT="OK"
    fi
else
    echo ERROR: $TESTNAME
    echo "wmbusmeters returned error code: $?"
    cat $TEST/test_output.txt
fi

TESTNAME="Test analyze normal telegram"
TESTRESULT="ERROR"

cat > $TEST/test_expected.txt <<EOF
000   : a2 length (162 bytes)
001   : 44 dll-c (from meter SND_NR)
002   : ee4d dll-mfct (SON)
004   : 78563412 dll-id (12345678)
008   : 3c dll-version
009   : 06 dll-type (Warm Water (30°C-90°C) meter)
010   : 7a tpl-ci-field (EN 13757-3 Application Layer (short tplh))
011   : 8f tpl-acc-field
012   : 00 tpl-sts-field (OK)
013   : 0000 tpl-cfg 0000 ( )
015   : 0C dif (8 digit BCD Instantaneous value)
016   : 13 vif (Volume l)
017 C!: 48550000 total consumption (5.548000 m3)
021   : 42 dif (16 Bit Integer/Binary Instantaneous value storagenr=1)
022   : 6C vif (Date type G)
023 C!: E1F1 set date (2127-01-01)
025   : 4C dif (8 digit BCD Instantaneous value storagenr=1)
026   : 13 vif (Volume l)
027 C!: 00000000 consumption at set date (0.000000 m3)
031   : 82 dif (16 Bit Integer/Binary Instantaneous value)
032   : 04 dife (subunit=0 tariff=0 storagenr=8)
033   : 6C vif (Date type G)
034 C!: 2129 history starts with date (2017-09-01)
036   : 8C dif (8 digit BCD Instantaneous value)
037   : 04 dife (subunit=0 tariff=0 storagenr=8)
038   : 13 vif (Volume l)
039 C!: 33000000 consumption at history 1 (0.033000 m3)
043   : 8D dif (variable length Instantaneous value)
044   : 04 dife (subunit=0 tariff=0 storagenr=8)
045   : 93 vif (Volume l)
046   : 1E vife (Compact profile with register)
047   : 3A varlen=58
048 C?: 3CFE3300000033000000330000003300000033000000330000003300000033000000330000003300000033000000330000004300000034180000
106   : 04 dif (32 Bit Integer/Binary Instantaneous value)
107   : 6D vif (Date and time type)
108 C!: 0D0B5C2B device datetime (2018-11-28 11:13)
112   : 03 dif (24 Bit Integer/Binary Instantaneous value)
113   : FD vif (Second extension FD of VIF-codes)
114   : 6C vife (Operating time battery [hour(s)])
115 C?: 5E1500
118   : 82 dif (16 Bit Integer/Binary Instantaneous value)
119   : 20 dife (subunit=0 tariff=2 storagenr=0)
120   : 6C vif (Date type G)
121 C?: 5C29
123   : 0B dif (6 digit BCD Instantaneous value)
124   : FD vif (Second extension FD of VIF-codes)
125   : 0F vife (Software version #)
126 C?: 020001
129   : 8C dif (8 digit BCD Instantaneous value)
130   : 40 dife (subunit=1 tariff=0 storagenr=0)
131   : 79 vif (Enhanced identification)
132 C?: 67888523
136   : 83 dif (24 Bit Integer/Binary Instantaneous value)
137   : 10 dife (subunit=0 tariff=1 storagenr=0)
138   : FD vif (Second extension FD of VIF-codes)
139   : 31 vife (Duration of tariff [minute(s)])
140 C?: 000000
143   : 82 dif (16 Bit Integer/Binary Instantaneous value)
144   : 10 dife (subunit=0 tariff=1 storagenr=0)
145   : 6C vif (Date type G)
146 C?: 0101
148   : 81 dif (8 Bit Integer/Binary Instantaneous value)
149   : 10 dife (subunit=0 tariff=1 storagenr=0)
150   : FD vif (Second extension FD of VIF-codes)
151   : 61 vife (Cumulation counter)
152 C?: 00
153   : 02 dif (16 Bit Integer/Binary Instantaneous value)
154   : FD vif (Second extension FD of VIF-codes)
155   : 66 vife (State of parameter activation)
156 C?: 0200
158   : 02 dif (16 Bit Integer/Binary Instantaneous value)
159   : FD vif (Second extension FD of VIF-codes)
160   : 17 vife (Error flags (binary))
161 C?: 0000
Best driver evo868 20/100
EOF

$PROG --analyze=plain A244EE4D785634123C067A8F0000000C1348550000426CE1F14C130000000082046C21298C0413330000008D04931E3A3CFE3300000033000000330000003300000033000000330000003300000033000000330000003300000033000000330000004300000034180000046D0D0B5C2B03FD6C5E150082206C5C290BFD0F0200018C4079678885238310FD3100000082106C01018110FD610002FD66020002FD170000 Water evo868 11111111 NOKEY 2> $TEST/test_output.txt 1>&2

if [ "$?" = "0" ]
then
    diff $TEST/test_expected.txt $TEST/test_output.txt
    if [ "$?" = "0" ]
    then
        echo OK: $TESTNAME
        TESTRESULT="OK"
    fi
else
    echo "wmbusmeters returned error code: $?"
    cat $TEST/test_output.txt
fi
