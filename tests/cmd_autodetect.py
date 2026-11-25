# Copyright (C) 2026 Solarboaters (gpl-3.0-or-later)
"""Run the real device scan with a simulated dongle, never accessing USB."""
import os
from pathlib import Path
import shlex
import subprocess
import sys
import tempfile

with tempfile.TemporaryDirectory(prefix="wmbus-autodetect-") as directory:
    root = Path(directory)
    source = root / "dongle.c"
    source.write_text('''
#include <stdint.h>
#include <string.h>
uint32_t rtlsdr_get_device_count(void) { return 1; }
int rtlsdr_get_device_usb_strings(uint32_t index, char *manufacturer,
                                  char *product, char *serial) {
    strcpy(manufacturer, "Test");
    strcpy(product, "Simulated");
    strcpy(serial, "00000001");
    return 0;
}
''')
    library = root / "dongle.so"
    subprocess.run(shlex.split(os.environ.get("CC", "cc")) +
                   ["-shared", "-fPIC", str(source), "-o", str(library)], check=True)
    scripts = {
        "rtl_sdr": '''#!/bin/sh
if [ "$#" = 0 ]; then echo RTL2832; exit 0; fi
echo dongle >> "$AUTODETECT_LOG"
exec sleep 20
''',
        "rtl_wmbus": '''#!/bin/sh
if [ "$1" = -h ]; then echo '-f exit'; exit 0; fi
exec cat
''',
        "receiver": '''#!/bin/sh
echo command >> "$AUTODETECT_LOG"
exec sleep 20
''',
    }
    for name, script in scripts.items():
        path = root / name
        path.write_text(script)
        path.chmod(0o755)
    env = dict(os.environ, PATH=str(root) + os.pathsep + os.environ["PATH"],
               LD_PRELOAD=str(library), AUTODETECT_LOG=str(root / "started"))
    # A wildcard command, and an explicitly identical id, must both remain
    # independent of the dongle. The last case is the reporter's workaround.
    for device in ("rtlwmbus", "rtlwmbus[00000001]", "rtlwmbus[gateway]"):
        (root / "started").write_text("")
        result = subprocess.run(
            [sys.argv[1], "--debug", "--exitafter=3s", "auto:t1",
             device + ":CMD(" + str(root / "receiver") + ")"],
            env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
            text=True, timeout=30)
        started = (root / "started").read_text().splitlines()
        assert result.returncode == 0, result.stdout
        assert started.count("command") == 1, (device, started, result.stdout)
        assert started.count("dongle") == 1, (device, started, result.stdout)
        print("OK independent command and auto-detected dongle:", device)
