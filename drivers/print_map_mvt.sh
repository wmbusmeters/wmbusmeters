#!/bin/bash

MVT="$1"
DRIVER="$2"

M=$(echo "$1" | cut -f 1 -d ',')
V=$(echo "$1" | cut -f 2 -d ',')
T=$(echo "$1" | cut -f 3 -d ',')

echo "    ${M}-${V}-${T}=$DRIVER"
