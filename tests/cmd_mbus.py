#!/usr/bin/env python3
# Copyright (C) 2026 Felix Göhringer (gpl-3.0-or-later)
"""Respond to primary M-Bus polls and verify the effective serial rate."""
import os
import sys

with open(sys.argv[1], "a") as log:
    log.write(os.environ["SERIAL_BPS"] + "\n")
    log.flush()
with open("simulations/simulation_mbus.txt") as fixture:
    line = next(line for line in fixture if line.startswith("telegram="))
frame = bytearray.fromhex(line.split("|")[1].replace("#", ""))
while True:
    request = sys.stdin.buffer.read(5)
    if not request:
        break
    assert len(request) == 5 and request[0] == 0x10 and request[4] == 0x16
    assert request[3] == (request[1] + request[2]) & 255
    if request[1] == 0x40:  # Broadcast initialization, no reply needed.
        continue
    assert request[1] in (0x5B, 0x7B)
    expected = "9600" if request[2] == 0 else "2400"
    assert os.environ["SERIAL_BPS"] == expected
    assert os.environ["SERIAL_PARITY"] == "e"
    response = frame.copy()
    response[5] = request[2]
    response[-2] = sum(response[4:-2]) & 255
    sys.stdout.buffer.write(response)
    sys.stdout.buffer.flush()
