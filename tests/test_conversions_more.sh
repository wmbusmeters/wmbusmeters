#!/bin/sh

PROG="$1"

rm -rf testoutput
mkdir -p testoutput
TEST=testoutput

TESTNAME="Test more unit conversions"
TESTRESULT="ERROR"

VALUE=$($PROG --format=json \
      --calculate_total_wh=total_kwh \
      --calculate_active_power_overall_w=active_power_overall_kw \
      689292680800723612452394150102cd0000008c1004688902008c1104688902008c2004000000008c21040000000002fdc9ff01ee0002fddBff01100002acff0120008240acff010a0002fdc9ff02ef0002fddBff02110002acff0224008240acff02070002fdc9ff03ee0002fddBff030e0002acff031c008240acff03060002ff68010002acff0062008240acff00190001ff1300f416 \
      MyMeter eltako 23451236 NOKEY | jq -r '.total_kwh, .total_wh, .active_power_overall_kw, .active_power_overall_w')

VALUE=$(echo "$VALUE" | tr '\n' ' ')

if [ "$VALUE" = "289.68 289680 0.98 980 " ]
then
    echo "OK: $TESTNAME"
    exit 0
else
    echo ">${VALUE}<"
    echo ERROR: $TESTNAME
    exit 1
fi
