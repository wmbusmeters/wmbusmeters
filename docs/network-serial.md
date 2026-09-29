# Serial receivers over a network

Connect a serial receiver on another computer directly to wmbusmeters using
ser2net or another compatible serial server. No local pseudo-terminal or socat
process is needed.

```
device=rfc2217://192.168.1.20:2000:cul:t1
device=tcp://192.168.1.20:2000:cul:t1
```

Use one of these lines, matching the server's mode. `tcp://` means an unmodified,
bidirectional byte stream. `rfc2217://` means Telnet with the RFC2217 Com Port
Control option; plain Telnet without that option is insufficient.

Hostnames, IPv4 addresses, and bracketed IPv6 addresses are supported:

```
device=rfc2217://bridge.local:2000:im871a:c1,t1
device=tcp://[fd00::20]:2000:cul:t1
device=MAIN=rfc2217://192.168.1.20:2001:mbus:2400
device=tcp://192.168.1.20:2002:rawtty:38400:t1
```

The receiver type is mandatory. Network endpoints are not automatically scanned
or probed as local character devices. Drivers retain their serial parameters:
for example CUL uses 38400 baud, 8 data bits, no parity and one stop bit; M-Bus
uses even parity. For rawtty and hextty, specify a baud rate.

RFC2217 negotiates the Telnet options, escapes binary data, and verifies server
acknowledgements for baud rate, data bits, parity, stop bits and disabled serial
flow control. Flow-control suspend/resume messages are handled. DTR, RTS and
BREAK are left at the server's settings, consistent with the local TTY drivers,
which do not currently request these operations. Unsupported settings or a
missing acknowledgement cause the connection to be closed and retried.

## ser2net 4 configuration for nanoCUL

On the computer with the USB receiver, use a stable `/dev/serial/by-id/` path
instead of `/dev/ttyUSB0` when possible. RFC2217:

```yaml
connection: &nanocul
  accepter: telnet(rfc2217),tcp,2000
  connector: serialdev,/dev/serial/by-id/REPLACE_WITH_YOUR_RECEIVER,38400n81,local,xonxoff=false,rtscts=false
  options:
    kickolduser: false
```

Alternatively, raw TCP:

```yaml
connection: &nanocul
  accepter: tcp,2000
  connector: serialdev,/dev/serial/by-id/REPLACE_WITH_YOUR_RECEIVER,38400n81,local,xonxoff=false,rtscts=false
  options:
    kickolduser: false
```

In raw mode, set the correct baud rate, parity and flow control in ser2net.
Keep server banners disabled. Only one application should control the receiver.
Both examples are unencrypted: expose the port only on a trusted network.

## Home Assistant

In an add-on containing a wmbusmeters build with this feature, replace the local
TTY `device=` line with the corresponding network line. Meter and MQTT settings
are unchanged. Existing released add-ons will need an updated wmbusmeters binary
before they recognize these addresses.

## Connection recovery and tests

EOF, reset connections and failed writes close the socket and notify the existing
device lifecycle. The hot-plug check retries the configured address and recreates
the receiver driver, discarding partial telegrams and applying its configuration
again. A server that is unavailable during startup can become available later.
Explicit `--nodeviceexit` retains its existing meaning.

Connection and protocol negotiation use five-second monotonic deadlines each;
system hostname resolution follows the operating system's resolver settings.
TCP keepalive detects silent connection loss; on Linux the probes start after
30 idle seconds, with three probes ten seconds apart. Normal meter silence alone
does not trigger reconnects.

Protocol and address tests run in `build/testinternals`. End-to-end tests run with:

```sh
python3 tests/test_network_serial.py build/wmbusmeters
```

They use independent mock servers, test receiver commands as well as meter
reception, fragment protocol sequences, reject serial settings, and verify that
both transports reinitialize CUL after reconnecting. A physical nanoCUL and a
real ser2net instance should also be tested before claiming hardware validation.
When ser2net is installed, the suite additionally checks both server modes with
a real ser2net process and a pseudo-terminal. Python 3 is needed for these tests,
but neither Python nor ser2net is a dependency of the wmbusmeters binary.
