#!/usr/bin/env python3
"""
Example client for the wmbusmeters decoding server.

Start the server:
    wmbusmeters --decodingserver=12345

Then run this script:
    python3 decoding_client.py

Protocol: JSON Lines over TCP — one JSON request per line, one JSON response per line.
"""

import json
import socket


def decode_telegram(host, port, telegram_hex, key="NOKEY", driver="auto", fmt=None):
    """Send a single decode request and return the parsed JSON response."""
    request = {
        "_": "decode",
        "telegram": telegram_hex,
        "key": key,
        "driver": driver,
    }
    if fmt:
        request["format"] = fmt

    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.settimeout(5.0)
    sock.connect((host, port))

    # One JSON object per line in, one JSON object per line out.
    sock.sendall((json.dumps(request) + "\n").encode())

    buf = b""
    while b"\n" not in buf:
        chunk = sock.recv(4096)
        if not chunk:
            break
        buf += chunk
    sock.close()

    line = buf.split(b"\n", 1)[0]
    return json.loads(line.decode())


class DecodingSession:
    """Persistent connection for sending multiple decode requests.

    The server uses per-client meter caching, so reusing a connection
    is more efficient when decoding multiple telegrams from the same meters.
    """

    def __init__(self, host="localhost", port=12345):
        self.host = host
        self.port = port
        self.sock = None
        self._buf = b""

    def connect(self):
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.sock.settimeout(5.0)
        self.sock.connect((self.host, self.port))
        self._buf = b""

    def close(self):
        if self.sock:
            self.sock.close()
            self.sock = None

    def decode(self, telegram_hex, key="NOKEY", driver="auto", fmt=None):
        """Send a decode request over the persistent connection."""
        if not self.sock:
            self.connect()

        request = {
            "_": "decode",
            "telegram": telegram_hex,
            "key": key,
            "driver": driver,
        }
        if fmt:
            request["format"] = fmt

        self.sock.sendall((json.dumps(request) + "\n").encode())

        # Read until we get a complete response line.
        while b"\n" not in self._buf:
            chunk = self.sock.recv(4096)
            if not chunk:
                raise ConnectionError("Server closed connection")
            self._buf += chunk

        line, self._buf = self._buf.split(b"\n", 1)
        return json.loads(line.decode())

    def __enter__(self):
        self.connect()
        return self

    def __exit__(self, *args):
        self.close()


if __name__ == "__main__":
    HOST = "localhost"
    PORT = 12345

    # --- Example 1: Single request using the helper function ---
    print("=== Single request ===")
    result = decode_telegram(
        HOST, PORT,
        telegram_hex="1844AE4C4455223368077A55000000041389E20100023B0000",
        driver="iperl",
    )
    print(json.dumps(result, indent=2))
    print(f"Meter: {result.get('meter')}, Total: {result.get('total_m3')} m3")

    # --- Example 2: Multiple requests over a persistent connection ---
    print("\n=== Persistent session with multiple requests ===")
    telegrams = [
        # iperl water meter (no encryption)
        ("1844AE4C4455223368077A55000000041389E20100023B0000", "NOKEY", "auto"),
        # multical21 water meter (encrypted)
        ("2A442D2C998734761B168D2091D37CAC21E1D68CDAFFCD3DC452BD802913FF7B1706CA9E355D6C2701CC24",
         "28F64A24988064A079AA2C807D6102AE", "auto"),
    ]

    with DecodingSession(HOST, PORT) as session:
        for hex_data, key, drv in telegrams:
            result = session.decode(hex_data, key=key, driver=drv)
            meter = result.get("meter", "?")
            meter_id = result.get("id", "?")
            fields = {k: v for k, v in result.items() if k not in ("_", "timestamp")}
            print(f"  {meter} (id={meter_id}): {json.dumps(fields)}")
