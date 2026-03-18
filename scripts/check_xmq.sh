#!/bin/sh

XMQ=$1

if [ -z "$XMQ" ]
then
    XMQ=xmq
fi

min="4.1.0"
ver="$($XMQ --version | cut -f 2 -d ' ')"

if printf "%s\n%s\n" "$min" "$ver" | sort -V | head -n1 | grep -qx "$min"; then
    true
else
    echo "Please upgrade xmq to a least version $min"
    exit 1
fi
