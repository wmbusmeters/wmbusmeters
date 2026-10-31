#!/bin/sh
# Copyright (C) 2026 Felix Göhringer (gpl-3.0-or-later)
. tests/include.sh
PROG="$1"
mkdir -p testoutput
TESTNAME="Test CMD serial override initialization"
if ! "$PROG" --debug --exitafter=2s 'cul:t1:CMD(sh tests/cmd_cul.sh)' >testoutput/cmd.out 2>testoutput/cmd.err
then
    cat testoutput/cmd.err
    printERROR "$TESTNAME"
    exit 1
fi
if ! grep -q 'CUL command exchange complete' testoutput/cmd.err
then
    cat testoutput/cmd.err
    printERROR "$TESTNAME"
    exit 1
fi
# The helper's diagnostic must not appear as serial input.
if grep 'received binary' testoutput/cmd.err | grep -q '434D4420646961676E6F73746963'
then
    printERROR "$TESTNAME: stderr entered serial input"
    exit 1
fi
printOK "$TESTNAME"
