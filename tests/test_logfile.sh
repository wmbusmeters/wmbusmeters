#!/bin/bash

PROG="$1"
TEST=testoutput
mkdir -p $TEST/meter_readings2

cat simulations/simulation_t1.txt | grep '^{' > $TEST/test_expected.txt

WMBUSMETERS_CONFIG_ROOT=tests/config2 $PROG --useconfig
