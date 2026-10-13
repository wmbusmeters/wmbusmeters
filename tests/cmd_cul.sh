#!/bin/sh
# Copyright (C) 2026 Felix Göhringer (gpl-3.0-or-later)
# Respond only after receiving the CUL initialization commands on stdin.
if [ -n "$CMD_TEST_LOG" ]; then
    printf '%s/%s/%s\n' "$SERIAL_BPS" "$SERIAL_PARITY" "$$" >> "$CMD_TEST_LOG"
elif [ -n "$SERIAL_BPS" ]; then
    [ "$SERIAL_BPS/$SERIAL_BITS/$SERIAL_PARITY/$SERIAL_STOPBITS/$SERIAL_FLOW" = "38400/8/n/1/none" ] || exit 5
fi
IFS= read -r mode || exit 1
[ "$mode" = "$(printf 'brt\r')" ] || exit 2
printf 'CMD diagnostic\n' >&2
printf 'TMODE\r\n'
IFS= read -r start || exit 3
[ "$start" = "$(printf 'X21\r')" ] || exit 4
printf 'CUL command exchange complete\n' >&2
# Keep the receiver alive until wmbusmeters exits.
exec cat >/dev/null
