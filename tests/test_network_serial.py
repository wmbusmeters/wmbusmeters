#!/usr/bin/env python3
# Copyright (C) 2026 Felix Göhringer (gpl-3.0-or-later)
"""End-to-end network receiver tests; no physical receiver is required."""

import os
import selectors
import shutil
import socket
import subprocess
import sys
import threading
import tempfile
import time
import unittest

PROG = os.path.abspath(sys.argv.pop(1))
TELEGRAM = bytes.fromhex("1844AE4C4455223368077A55000000041389E20100023B0000")


def cul_telegram():
    def crc(block):
        value = 0
        for byte in block:
            value ^= byte << 8
            for _ in range(8):
                value = ((value << 1) ^ (0x3D65 if value & 0x8000 else 0)) & 0xFFFF
        return (value ^ 0xFFFF).to_bytes(2, "big")

    frame = TELEGRAM[:10] + crc(TELEGRAM[:10])
    for offset in range(10, len(TELEGRAM), 16):
        block = TELEGRAM[offset:offset + 16]
        frame += block + crc(block)
    return b"b" + frame.hex().encode("ascii") + b"0000\r\n"


class Receiver:
    """Small independent Telnet server and CUL emulator."""

    def __init__(self, rfc=False, cul=False, reject=False, ipv6=False):
        self.rfc, self.cul, self.reject = rfc, cul, reject
        self.listener = socket.socket(socket.AF_INET6 if ipv6 else socket.AF_INET)
        self.listener.bind(("::1" if ipv6 else "127.0.0.1", 0))
        self.listener.listen()
        self.listener.settimeout(0.2)
        self.port = self.listener.getsockname()[1]
        self.ready = threading.Event()
        self.stop = threading.Event()
        self.connections = 0
        self.commands = []
        self.settings = []
        self.error = None
        self.client = None
        self.lock = threading.Lock()
        self.thread = threading.Thread(target=self.run, daemon=True)
        self.thread.start()

    def write(self, payload):
        if self.rfc:
            payload = payload.replace(b"\xff", b"\xff\xff")
        with self.lock:
            self.client.sendall(payload)

    def disconnect(self):
        self.ready.clear()
        with self.lock:
            self.client.shutdown(socket.SHUT_RDWR)
            self.client.close()

    def run(self):
        try:
            while not self.stop.is_set():
                try:
                    connection, _ = self.listener.accept()
                except socket.timeout:
                    continue
                self.client = connection
                self.connections += 1
                connection.settimeout(0.2)
                state, verb, sub, payload = "data", 0, bytearray(), bytearray()
                if not self.rfc and not self.cul:
                    self.ready.set()
                try:
                    while not self.stop.is_set():
                        try:
                            chunk = connection.recv(4096)
                        except socket.timeout:
                            continue
                        if not chunk:
                            break
                        for value in chunk:
                            if not self.rfc:
                                payload.append(value)
                            elif state == "data":
                                if value == 255:
                                    state = "iac"
                                else:
                                    payload.append(value)
                            elif state == "iac":
                                if value == 255:
                                    payload.append(value)
                                    state = "data"
                                elif value in (251, 252, 253, 254):
                                    verb, state = value, "option"
                                elif value == 250:
                                    sub.clear()
                                    state = "sub"
                                else:
                                    state = "data"
                            elif state == "option":
                                if verb in (251, 253):
                                    answer = 253 if verb == 251 else 251
                                    connection.sendall(bytes((255, answer, value)))
                                state = "data"
                            elif state == "sub":
                                if value == 255:
                                    state = "sub_iac"
                                else:
                                    sub.append(value)
                            elif state == "sub_iac":
                                if value == 255:
                                    sub.append(value)
                                    state = "sub"
                                elif value == 240:
                                    if sub[0] == 44:
                                        self.settings.append(bytes(sub[1:]))
                                        reply = bytes((44, sub[1] + 100)) + bytes(sub[2:])
                                        if self.reject and sub[1] == 3:
                                            reply = bytes((44, 103, 3))
                                        # Deliberately split every control sequence into single bytes.
                                        wire = b"\xff\xfa" + reply.replace(b"\xff", b"\xff\xff") + b"\xff\xf0"
                                        for b in wire:
                                            connection.sendall(bytes((b,)))
                                        if bytes(sub[1:]) == b"\x05\x0e" and not self.cul:
                                            self.ready.set()
                                    state = "data"
                            if self.cul:
                                while b"\n" in payload:
                                    command, _, remaining = payload.partition(b"\n")
                                    payload = bytearray(remaining)
                                    command = command.strip()
                                    self.commands.append(bytes(command))
                                    if command == b"brt":
                                        connection.sendall(b"TMODE\r\n")
                                    elif command == b"X21":
                                        self.ready.set()
                except (ConnectionError, OSError):
                    pass
                finally:
                    connection.close()
        except BaseException as error:
            if not self.stop.is_set():
                self.error = error

    def close(self):
        self.stop.set()
        self.listener.close()
        if self.client:
            self.client.close()
        self.thread.join(3)
        if self.thread.is_alive():
            raise AssertionError("receiver thread did not stop")
        if self.error:
            raise self.error


class NetworkTests(unittest.TestCase):
    def start(self, receiver, kind="rawtty", ipv6=False):
        scheme = "rfc2217" if receiver.rfc else "tcp"
        host = "[::1]" if ipv6 else "127.0.0.1"
        device = f"{scheme}://{host}:{receiver.port}:{kind}:38400:t1"
        process = subprocess.Popen([PROG, "--debug", "--format=json", device],
                                   stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        self.addCleanup(receiver.close)

        def cleanup():
            if process.poll() is None:
                process.terminate()
            try:
                process.wait(10)
            except subprocess.TimeoutExpired:
                process.kill()
                process.wait()
            process.stdout.close()

        self.addCleanup(cleanup)
        return process

    def await_output(self, process, text, timeout=15):
        output = bytearray()
        deadline = time.monotonic() + timeout
        with selectors.DefaultSelector() as selector:
            selector.register(process.stdout, selectors.EVENT_READ)
            while time.monotonic() < deadline:
                if selector.select(0.2):
                    chunk = os.read(process.stdout.fileno(), 65536)
                    if not chunk:
                        break
                    output.extend(chunk)
                    if text in output:
                        return bytes(output)
        self.fail(f"Missing {text!r}, process={process.poll()}, output={output.decode(errors='replace')}")

    def test_raw_telegram(self):
        receiver = Receiver()
        process = self.start(receiver)
        self.assertTrue(receiver.ready.wait(10))
        self.await_output(process, b"regular reset of rawtty")
        # TCP delivers a byte stream; telegrams may span arbitrary packet boundaries.
        for b in TELEGRAM:
            receiver.write(bytes((b,)))
        self.await_output(process, b"33225544")

    def test_rfc_telegram_and_configuration(self):
        receiver = Receiver(rfc=True)
        process = self.start(receiver)
        self.assertTrue(receiver.ready.wait(10))
        self.await_output(process, b"regular reset of rawtty")
        receiver.write(TELEGRAM)
        self.await_output(process, b"33225544")
        self.assertEqual(receiver.settings[:6], [b"\x01\x00\x00\x96\x00", b"\x02\x08",
                                                b"\x03\x01", b"\x04\x01", b"\x05\x01", b"\x05\x0e"])

    def test_cul_reinitialized_after_disconnect(self):
        for rfc in (False, True):
            with self.subTest(rfc=rfc):
                receiver = Receiver(rfc=rfc, cul=True)
                process = self.start(receiver, "cul")
                self.assertTrue(receiver.ready.wait(10))
                receiver.write(b"b1844")
                receiver.disconnect()
                self.assertTrue(receiver.ready.wait(15))
                self.assertIsNone(process.poll())
                self.assertGreaterEqual(receiver.connections, 2)
                self.assertEqual(receiver.commands[:4], [b"brt", b"X21", b"brt", b"X21"])
                receiver.write(cul_telegram())
                self.await_output(process, b"Received telegram from: 33225544")

    def test_rejected_settings_retry_without_exit(self):
        receiver = Receiver(rfc=True, reject=True)
        process = self.start(receiver)
        deadline = time.monotonic() + 15
        while receiver.connections < 2 and time.monotonic() < deadline:
            time.sleep(0.05)
        self.assertGreaterEqual(receiver.connections, 2)
        self.assertIsNone(process.poll())
        self.assertFalse(receiver.ready.is_set())
        # The same configured endpoint should recover when the server is fixed.
        receiver.reject = False
        self.assertTrue(receiver.ready.wait(15))
        self.await_output(process, b"regular reset of rawtty")
        receiver.write(TELEGRAM)
        self.await_output(process, b"33225544")

    def test_server_available_after_startup(self):
        receiver = Receiver()
        # Reserve the address, but do not accept connections until after startup.
        receiver.stop.set()
        receiver.thread.join(3)
        receiver.listener.close()
        port = receiver.port
        process = self.start(receiver)
        self.await_output(process, b"could not connect/configure")
        receiver.listener = socket.socket()
        receiver.listener.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        receiver.listener.bind(("127.0.0.1", port))
        receiver.listener.listen()
        receiver.listener.settimeout(0.2)
        receiver.stop.clear()
        receiver.thread = threading.Thread(target=receiver.run, daemon=True)
        receiver.thread.start()
        self.assertTrue(receiver.ready.wait(15))
        self.await_output(process, b"regular reset of rawtty")
        receiver.write(TELEGRAM)
        self.await_output(process, b"33225544")

    def test_ipv6(self):
        try:
            receiver = Receiver(ipv6=True)
        except OSError as error:
            self.skipTest(str(error))
        process = self.start(receiver, ipv6=True)
        self.assertTrue(receiver.ready.wait(10))
        self.await_output(process, b"regular reset of rawtty")
        receiver.write(TELEGRAM)
        self.await_output(process, b"33225544")

    @unittest.skipUnless(shutil.which("ser2net"), "ser2net is not installed")
    def test_real_ser2net(self):
        import pty
        import termios

        for rfc in (False, True):
            with self.subTest(rfc=rfc), tempfile.TemporaryDirectory() as directory:
                master, slave = pty.openpty()
                server = process = None
                try:
                    with socket.socket() as reservation:
                        reservation.bind(("127.0.0.1", 0))
                        port = reservation.getsockname()[1]
                    accepter = "telnet(rfc2217),tcp" if rfc else "tcp"
                    config = os.path.join(directory, "ser2net.yaml")
                    with open(config, "w", encoding="utf-8") as stream:
                        stream.write(f"connection: &test\n  accepter: {accepter},127.0.0.1,{port}\n"
                                     f"  connector: serialdev,{os.ttyname(slave)},38400n81,local,xonxoff=false,rtscts=false\n")
                    server = subprocess.Popen(["ser2net", "-n", "-c", config],
                                              stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
                    scheme = "rfc2217" if rfc else "tcp"
                    process = subprocess.Popen([PROG, "--debug", f"{scheme}://127.0.0.1:{port}:rawtty:38400:t1"],
                                               stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
                    self.await_output(process, b"regular reset of rawtty", timeout=20)
                    # A raw TCP handshake can finish before ser2net configures
                    # the PTY. Do not inject serial bytes into its initial cooked mode.
                    deadline = time.monotonic() + 5
                    while termios.tcgetattr(slave)[3] & (termios.ICANON | termios.ECHO):
                        self.assertLess(time.monotonic(), deadline, "ser2net did not configure raw serial mode")
                        time.sleep(0.01)
                    settings = termios.tcgetattr(slave)
                    if settings[4] == 0 or settings[5] == 0:
                        # Some gensio/libc combinations use Linux's extended baud
                        # interface, which tcgetattr cannot represent correctly.
                        # TCGETS2 reads the actual kernel speeds on x86/ARM Linux.
                        import fcntl
                        import struct
                        self.assertTrue(sys.platform.startswith("linux"))
                        self.assertIn(os.uname().machine, ("x86_64", "aarch64", "armv7l", "i686"))
                        extended = fcntl.ioctl(slave, 0x802C542A, bytes(44))
                        self.assertEqual(struct.unpack_from("=II", extended, 36), (38400, 38400))
                    else:
                        self.assertEqual(settings[4], termios.B38400)
                        self.assertEqual(settings[5], termios.B38400)
                    self.assertFalse(settings[0] & (termios.IXON | termios.IXOFF))
                    os.write(master, TELEGRAM)
                    self.await_output(process, b"33225544")
                finally:
                    for child in (process, server):
                        if child:
                            child.terminate()
                            try:
                                child.wait(10)
                            except subprocess.TimeoutExpired:
                                child.kill()
                                child.wait()
                            child.stdout.close()
                    os.close(master)
                    os.close(slave)


if __name__ == "__main__":
    unittest.main()
