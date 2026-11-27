#!/bin/bash

MVT="$1"
DRIVER="$2"

M=$(echo "$1" | cut -f 1 -d ',')
V=$(echo "$1" | cut -f 2 -d ',')
T=$(echo "$1" | cut -f 3 -d ',')

if [ "$V" = "*" ]; then V=ff; fi
if [ "$T" = "*" ]; then T=ff; fi

# An optional third argument "dll" marks a dll_mvt, the driver is then detected
# on the dll mvt even when a long tpl carries another mvt.
if [ "$3" = "dll" ]
then
    echo "    { { MANUFACTURER_${M},0x${V},0x${T} }, \"$DRIVER\", true },"
else
    echo "    { { MANUFACTURER_${M},0x${V},0x${T} }, \"$DRIVER\" },"
fi
