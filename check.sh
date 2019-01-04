#!/bin/bash
# Build with static analysis tool Coverity
make clean
rm -rf cov-int
cov-build --dir cov-int make
tar czvf wmbusmeters.tgz cov-int
