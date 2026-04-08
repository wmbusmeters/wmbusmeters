#!/bin/sh

# Temporarily disable this test
exit 0

XMQ=$1

if [ -z "$XMQ" ]
then
    XMQ=xmq
fi

if ! command -v $XMQ >/dev/null
then
    # If there is no xmq, that is fine.
    echo "No $XMQ found."
    exit 0
fi

min="4.1.0"
ver="$($XMQ --version | cut -f 2 -d ' ')"

if printf "%s\n%s\n" "$min" "$ver" | sort -V | head -n1 | grep -qx "$min"
then
    true
else
    echo "Please upgrade xmq to a least version $min"
    echo "cd 3rdparty/xmq; git pull; ./configure; make"
    exit 1
fi
