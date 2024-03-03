#!/bin/sh

PROG="$1"

mkdir -p testoutput
TEST=testoutput

TESTNAME="Test addresses"
TESTRESULT="OK"

# dll-mfct (ESY) dll-id (77887788) dll-version (30) dll-type (37 Radio converter (meter side))
# tpl-id (77997799) tpl-mfct (ESY) tpl-version (11) tpl-type (02 Electricity meter)
TELEGRAM=7B4479168877887730378C20F0900F002C2549EE0A0077C19D3D1A08ABCD729977997779161102F0005007102F2F_0702F5C3FA000000000007823C5407000000000000841004E081020084200415000000042938AB000004A9FF01FA0A000004A9FF02050A000004A9FF03389600002F2F2F2F2F2F2F2F2F2F2F2F2F
ARGS="--format=fields --selectfields=total_energy_consumption_kwh $TELEGRAM EL esyswm"

checkResult() {
    F=$($PROG $ARGS "$E" NOKEY)
    if [ "$F" != "1643.4165" ]
    then
        echo "EXPECTED 1643.4165 *********************************************"
        echo "E=$E"
        echo $PROG $ARGS "$E" NOKEY
        $PROG $ARGS "$E" NOKEY
        echo "*********************************************"
        TESTRESULT=ERROR
    fi
}

expectEmpty() {
    F=$($PROG $ARGS "$E" NOKEY)
    if [ "$F" != "" ]
    then
        echo "EXPECTED EMPTY OUTPUT *********************************************"
        echo "E=$E"
        echo $PROG $ARGS "$E" NOKEY
        $PROG $ARGS "$E" NOKEY
        echo "*********************************************"
        TESTRESULT=ERROR
    fi
}

E=77997799
checkResult

E=77997799.M=ESY
checkResult

E=77997799.M=PII
expectEmpty

E=77*
checkResult

E=*
checkResult

E=ANYID
checkResult

E=77997799.T=02
checkResult

E=77887788.T=02
expectEmpty

E=7788*.T=37.V=30.M=ESY
checkResult

E=77997799,!*.V=88
checkResult

E=*.T=02
checkResult

E=*.T=02,!77*
expectEmpty

E=*.T=02,!77*.V=11
expectEmpty

E=7788*.T=37,!7799*.T=02
expectEmpty

echo "$TESTRESULT: $TESTNAME"
