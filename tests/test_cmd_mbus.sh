#!/bin/sh
# Copyright (C) 2026 Felix Göhringer (gpl-3.0-or-later)
. tests/include.sh
PROG="$1"
if ! command -v python3 >/dev/null 2>&1; then
    echo "Skipping CMD M-Bus test, python3 not installed."
    exit 0
fi
mkdir -p testoutput
rm -f testoutput/cmd_mbus_rates
TESTNAME="Test per-meter CMD baud rate and bus-default restoration"
if ! "$PROG" --debug --oneshot --exitafter=20s --pollinterval=1s --format=fields --selectfields=name \
    'wire=mbus:2400:CMD(python3 tests/cmd_mbus.py testoutput/cmd_mbus_rates)' \
    Fast sensostar:wire:9600:mbus p0 NOKEY Default sensostar:wire:mbus p1 NOKEY \
    >testoutput/cmd_mbus.out 2>testoutput/cmd_mbus.err
then
    cat testoutput/cmd_mbus.err
    printERROR "$TESTNAME"
    exit 1
fi
printf '2400\n9600\n2400\n' >testoutput/cmd_mbus_expected
head -n 3 testoutput/cmd_mbus_rates >testoutput/cmd_mbus_actual
if ! diff testoutput/cmd_mbus_expected testoutput/cmd_mbus_actual || \
   ! grep -qx Fast testoutput/cmd_mbus.out || ! grep -qx Default testoutput/cmd_mbus.out
then
    cat testoutput/cmd_mbus.out testoutput/cmd_mbus.err
    printERROR "$TESTNAME"
    exit 1
fi
printOK "$TESTNAME"
