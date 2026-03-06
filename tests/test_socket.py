#!/usr/bin/env python3
"""
Test script for the wmbusmeters SOCKET device.

Starts wmbusmeters with a SOCKET device, connects via Unix domain socket,
sends JSON Lines decode requests, and verifies responses.

Usage:
    python3 tests/test_socket.py [path/to/wmbusmeters]

If no path is given, defaults to build/wmbusmeters.g
"""

import json
import os
import signal
import socket
import subprocess
import sys
import time

SOCKET_PATH = "/tmp/test_wmbusmeters_py.sock"

# --- Test cases: (description, request, expected_checks) ---
# expected_checks is a dict of JSON field -> expected value (or a callable for custom checks)

TEST_CASES = [
    # 1. Encrypted multical21 with correct key
    (
        "encrypted multical21 with correct key",
        {
            "_": "decode",
            "telegram": "2A442D2C998734761B168D2091D37CAC21E1D68CDAFFCD3DC452BD802913FF7B1706CA9E355D6C2701CC24",
            "key": "28F64A24988064A079AA2C807D6102AE",
        },
        {
            "meter": "multical21",
            "id": "76348799",
            "media": "cold water",
            "total_m3": 6.408,
            "target_m3": 6.408,
            "status": "DRY",
        },
    ),
    # 2. Encrypted multical21 with wrong key
    (
        "encrypted multical21 with wrong key",
        {
            "_": "decode",
            "telegram": "2A442D2C998734761B168D2091D37CAC21E1D68CDAFFCD3DC452BD802913FF7B1706CA9E355D6C2701CC24",
            "key": "00000000000000000000000000000000",
        },
        {
            "id": "76348799",
            "error": lambda v: "decryption failed" in v,
        },
    ),
    # 3. Unencrypted lansenpu (no key needed)
    (
        "unencrypted lansenpu",
        {
            "_": "decode",
            "telegram": "234433300602010014007a8e0000002f2f0efd3a1147000000008e40fd3a341200000000",
        },
        {
            "meter": "lansenpu",
            "id": "00010206",
            "media": "other",
            "a_counter": 4711,
            "b_counter": 1234,
        },
    ),
    # 4. Unencrypted supercom587 (warm water)
    (
        "unencrypted supercom587",
        {
            "_": "decode",
            "telegram": "A244EE4D785634123C067A8F0000000C1348550000426CE1F14C130000000082046C21298C0413330000008D04931E3A3CFE3300000033000000330000003300000033000000330000003300000033000000330000003300000033000000330000004300000034180000046D0D0B5C2B03FD6C5E150082206C5C290BFD0F0200018C4079678885238310FD3100000082106C01018110FD610002FD66020002FD170000",
        },
        {
            "meter": "supercom587",
            "id": "12345678",
            "media": "warm water",
            "total_m3": 5.548,
        },
    ),
    # 5. MBUS telegram (piigth room sensor)
    (
        "MBUS piigth room sensor",
        {
            "_": "decode",
            "telegram": "68383868080072840200102941011B0D0000000265FE0842653009820165E70802FB1A480142FB1A45018201FB1A4E010C788402001002FD0F21000F0316",
        },
        {
            "meter": "piigth",
            "id": "10000284",
            "media": "room sensor",
            "temperature_c": 23.02,
            "relative_humidity_rh": 32.8,
        },
    ),
    # 6. Explicit driver override
    (
        "explicit driver multical21",
        {
            "_": "decode",
            "telegram": "2A442D2C998734761B168D2091D37CAC21E1D68CDAFFCD3DC452BD802913FF7B1706CA9E355D6C2701CC24",
            "key": "28F64A24988064A079AA2C807D6102AE",
            "driver": "multical21",
        },
        {
            "meter": "multical21",
            "id": "76348799",
            "total_m3": 6.408,
        },
    ),
    # 7. Invalid hex telegram
    (
        "invalid hex telegram",
        {
            "_": "decode",
            "telegram": "ZZZZ_NOT_HEX",
        },
        {
            "error": lambda v: "invalid hex" in v.lower(),
        },
    ),
    # 8. Missing telegram field
    (
        "missing telegram field",
        {
            "_": "decode",
            "key": "0000",
        },
        {
            "error": lambda v: "missing" in v.lower(),
        },
    ),
    # 9. Explicit MBUS format
    (
        "explicit MBUS format piigth",
        {
            "_": "decode",
            "telegram": "68383868080072840200102941011B0D0000000265FE0842653009820165E70802FB1A480142FB1A45018201FB1A4E010C788402001002FD0F21000F0316",
            "format": "mbus",
        },
        {
            "meter": "piigth",
            "id": "10000284",
            "temperature_c": 23.02,
        },
    ),
    # 10. Explicit WMBUS format
    (
        "explicit WMBUS format multical21",
        {
            "_": "decode",
            "telegram": "2A442D2C998734761B168D2091D37CAC21E1D68CDAFFCD3DC452BD802913FF7B1706CA9E355D6C2701CC24",
            "key": "28F64A24988064A079AA2C807D6102AE",
            "format": "wmbus",
        },
        {
            "meter": "multical21",
            "id": "76348799",
            "total_m3": 6.408,
        },
    ),
]


def start_wmbusmeters(binary):
    """Start wmbusmeters with SOCKET device, return subprocess."""
    # Clean up any leftover socket
    if os.path.exists(SOCKET_PATH):
        os.unlink(SOCKET_PATH)

    proc = subprocess.Popen(
        [binary, "socket(%s):c1" % SOCKET_PATH],
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
    )

    # Wait for socket file to appear
    for _ in range(50):
        if os.path.exists(SOCKET_PATH):
            return proc
        time.sleep(0.1)

    # If we get here, something went wrong
    proc.kill()
    stdout, _ = proc.communicate(timeout=5)
    print("FAIL: wmbusmeters did not create socket within 5 seconds")
    print("Output:", stdout.decode(errors="replace"))
    sys.exit(1)


def send_request(request_dict):
    """Connect to socket, send one JSON line, read one JSON line response."""
    sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
    sock.settimeout(5)
    sock.connect(SOCKET_PATH)

    line = json.dumps(request_dict) + "\n"
    sock.sendall(line.encode())

    # Server sends back a single-line JSON response terminated by newline.
    buf = b""
    while b"\n" not in buf:
        chunk = sock.recv(4096)
        if not chunk:
            break
        buf += chunk

    sock.close()

    text = buf.decode().strip()
    if not text:
        return None
    return json.loads(text)


def send_multiple_requests(requests):
    """Connect once, send multiple JSON lines, read multiple JSON line responses."""
    sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
    sock.settimeout(5)
    sock.connect(SOCKET_PATH)

    # Send all requests
    for req in requests:
        line = json.dumps(req) + "\n"
        sock.sendall(line.encode())

    # Read all responses — one JSON object per line.
    buf = b""
    while True:
        try:
            chunk = sock.recv(4096)
            if not chunk:
                break
            buf += chunk
        except socket.timeout:
            break

    sock.close()

    lines = buf.decode().strip().split("\n")
    return [json.loads(line) for line in lines if line.strip()]


def check_response(description, response, expected_checks):
    """Verify response matches expected checks. Returns (pass, message)."""
    if response is None:
        return False, "no response received"

    errors = []
    for field, expected in expected_checks.items():
        if field not in response:
            errors.append("missing field '%s'" % field)
            continue

        actual = response[field]
        if callable(expected):
            if not expected(actual):
                errors.append(
                    "field '%s': check failed (value=%r)" % (field, actual)
                )
        elif actual != expected:
            errors.append(
                "field '%s': expected %r, got %r" % (field, expected, actual)
            )

    if errors:
        return False, "; ".join(errors)
    return True, "ok"


def main():
    binary = sys.argv[1] if len(sys.argv) > 1 else "build/wmbusmeters.g"

    if not os.path.isfile(binary):
        print("ERROR: binary not found: %s" % binary)
        sys.exit(1)

    print("Using binary: %s" % binary)
    print("Socket path:  %s" % SOCKET_PATH)
    print()

    proc = start_wmbusmeters(binary)
    print("wmbusmeters started (pid %d)" % proc.pid)
    print()

    passed = 0
    failed = 0

    # --- Individual request tests ---
    for description, request, expected_checks in TEST_CASES:
        try:
            response = send_request(request)
            ok, msg = check_response(description, response, expected_checks)
            if ok:
                print("  OK: %s" % description)
                passed += 1
            else:
                print("FAIL: %s" % description)
                print("      %s" % msg)
                print("      response: %s" % json.dumps(response, indent=None))
                failed += 1
        except Exception as e:
            print("FAIL: %s" % description)
            print("      exception: %s" % e)
            failed += 1

    # --- Multi-request on single connection test ---
    print()
    print("Multi-request test (3 telegrams on one connection):")
    try:
        multi_requests = [
            TEST_CASES[0][1],  # multical21 encrypted
            TEST_CASES[2][1],  # lansenpu unencrypted
            TEST_CASES[3][1],  # supercom587 unencrypted
        ]
        multi_expected = [
            TEST_CASES[0][2],
            TEST_CASES[2][2],
            TEST_CASES[3][2],
        ]

        responses = send_multiple_requests(multi_requests)
        if len(responses) != 3:
            print("FAIL: expected 3 responses, got %d" % len(responses))
            failed += 1
        else:
            all_ok = True
            for i, (resp, exp) in enumerate(zip(responses, multi_expected)):
                desc = "multi[%d]" % i
                ok, msg = check_response(desc, resp, exp)
                if not ok:
                    print("FAIL: %s - %s" % (desc, msg))
                    all_ok = False
            if all_ok:
                print("  OK: 3 telegrams decoded on single connection")
                passed += 1
            else:
                failed += 1
    except Exception as e:
        print("FAIL: multi-request test: %s" % e)
        failed += 1

    # --- Reconnection test ---
    print()
    print("Reconnection test (connect, disconnect, reconnect):")
    try:
        # First connection
        r1 = send_request(TEST_CASES[0][1])
        ok1, _ = check_response("reconnect-1", r1, TEST_CASES[0][2])

        # Brief pause for server to return to listening
        time.sleep(0.5)

        # Second connection
        r2 = send_request(TEST_CASES[2][1])
        ok2, _ = check_response("reconnect-2", r2, TEST_CASES[2][2])

        if ok1 and ok2:
            print("  OK: reconnection works")
            passed += 1
        else:
            print("FAIL: reconnection failed")
            failed += 1
    except Exception as e:
        print("FAIL: reconnection test: %s" % e)
        failed += 1

    # --- Summary ---
    print()
    total = passed + failed
    print("%d/%d tests passed" % (passed, total))

    # Cleanup
    os.kill(proc.pid, signal.SIGTERM)
    try:
        proc.wait(timeout=5)
    except subprocess.TimeoutExpired:
        proc.kill()
        proc.wait()
    if os.path.exists(SOCKET_PATH):
        os.unlink(SOCKET_PATH)

    sys.exit(0 if failed == 0 else 1)


if __name__ == "__main__":
    main()
