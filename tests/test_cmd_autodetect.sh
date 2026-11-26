#!/bin/sh
# Copyright (C) 2026 Solarboaters (gpl-3.0-or-later)
. tests/include.sh
# The simulated USB enumeration uses the Linux dynamic loader.
if [ "$(uname -s)" != Linux ]; then
    echo 'Skipping CMD autodetection test (requires Linux LD_PRELOAD)'
    exit 0
fi
python3 tests/cmd_autodetect.py "$1"
