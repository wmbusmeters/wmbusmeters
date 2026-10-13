# CMD serial receivers

A serial dongle can use a command in place of its local TTY. For example,
with a gensiot build supporting RFC2217 startup settings:

```sh
wmbusmeters 'cul:t1:CMD(gensiot -e -1 "telnet(rfc2217,${SERIAL_BPS}${SERIAL_PARITY}${SERIAL_BITS}${SERIAL_STOPBITS},noflow),tcp,bridge,2000")'
```

The command receives initialization and outgoing bytes on stdin and supplies
receiver bytes on stdout. Keep diagnostics on stderr. Normal shell expansion
applies inside CMD, so quote the device argument as above.

Before starting the command, the serial driver supplies its actual settings:

| Variable | Value |
| --- | --- |
| `SERIAL_BPS` | Driver baud rate, such as 38400 for CUL |
| `SERIAL_BITS` | 8 |
| `SERIAL_PARITY` | `n`, `e` or `o` |
| `SERIAL_STOPBITS` | 1 |
| `SERIAL_FLOW` | `none` |

M-Bus uses even parity and defaults to 2400 baud; rawtty/hextty commands default
to 9600 baud. For these devices, and RC1180, the device's baud argument selects
the speed. Other dongles use the same fixed baud rate as their local TTY path.
Commands such as rtlwmbus remain read-only producers and do not get serial
settings. XMQTTY does not define physical serial settings.

When a polled M-Bus meter specifies its own baud rate, wmbusmeters applies it
before sending the poll. Meters without a baud override use the original bus
baud rate, even after another meter changed it. A change restarts CMD with the new environment and
runs the normal bus initialization again. Repeating the same settings does
not restart the command. Scheduled resets and recovery also start a fresh
command with the current settings. Payload sends are blocked while a settings
change still needs a reopen.

The example uses plain TCP. For a protected connection, configure gensio's TLS
layer and certificate verification to match the bridge. TLS and RFC2217 are
handled by gensiot. Startup settings require the gensio changes integrated in
October 2026; gensio 3.0.4 does not include them.
