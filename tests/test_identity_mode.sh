#!/bin/sh

PROG="$1"

mkdir -p testoutput
TEST=testoutput

TESTNAME="Test identity mode"
TESTRESULT="OK"

# dll-mfct (ESY) dll-id (77887788) dll-version (30) dll-type (37 Radio converter (meter side))
# tpl-id (77997799) tpl-mfct (ESY) tpl-version (11) tpl-type (02 Electricity meter)

$PROG --identitymode=id --format=fields simulations/simulation_identity_mode.txt EL esyswm 77997799 NOKEY > $TEST/test_output.txt 2>&1
$PROG --identitymode=id-mfct --format=fields simulations/simulation_identity_mode.txt EL esyswm 77997799 NOKEY >> $TEST/test_output.txt 2>&1
$PROG --identitymode=full --format=fields simulations/simulation_identity_mode.txt EL esyswm 77997799 NOKEY >> $TEST/test_output.txt 2>&1

cat $TEST/test_output.txt | sed 's/[0-9][0-9][0-9][0-9]-[0-9][0-9]-[0-9][0-9] [0-9][0-9]:[0-9][0-9].[0-9][0-9]$/1111-11-11 11:11.11/' > $TEST/test_responses.txt

cat > $TEST/test_expected.txt <<EOF
EL;77997799;null;null;null;null;null;null;null;null;1ESY9887654321;1111-11-11 11:11.11
EL;77997799;1643.4165;0.43832;0.1876;1643.2;0.21;0.0281;0.02565;0.38456;1ESY9887654321;1111-11-11 11:11.11
EL;77997799.M=ETY;null;null;null;null;null;null;null;null;1ESY9887654321;1111-11-11 11:11.11
EL;77997799.M=ESY;1643.4165;0.43832;0.1876;1643.2;0.21;0.0281;0.02565;0.38456;null;1111-11-11 11:11.11
EL;77997799.M=ETY.V=11.T=02;null;null;null;null;null;null;null;null;1ESY9887654321;1111-11-11 11:11.11
EL;77997799.M=ESY.V=11.T=02;1643.4165;0.43832;0.1876;1643.2;0.21;0.0281;0.02565;0.38456;null;1111-11-11 11:11.11
EOF

if ! diff $TEST/test_expected.txt $TEST/test_responses.txt
then
    if [ "$USE_MELD" = "true" ]
    then
        meld $TEST/test_expected.txt $TEST/test_responses.txt
    fi
    echo "ERROR: $TESTNAME"
else
    echo OK: $TESTNAME
fi
