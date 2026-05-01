#!/bin/bash
# Copyright (C) 2026 Fredrik Öhrström (gpl-3.0-or-later)

mkdir -p perftest
mkdir -p build

if ! test -f perftest/difvifexamples
then
    if ! test -f perftest/wmbusmeters_extract
    then
        rm -rf build
        ./configure
        make -j4 COLLECT=true
        cp build/wmbusmeters perftest/wmbusmeters_extract
    fi

    # Extract all difvif segments into /tmp/difvifexamples
    cp perftest/wmbusmeters_extract build
    rm /tmp/difvifexamples
    make test

    sort /tmp/difvifexamples | uniq > perftest/difvifexamples
fi

if ! test -f perftest/perf
then
    rm -rf build
    ./configure
    make build/perf -j4
    cp build/perf perftest/perf
fi

./perftest/perf perftest/difvifexamples 1000
