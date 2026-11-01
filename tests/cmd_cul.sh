#!/bin/sh
# Copyright (C) 2026 Felix Göhringer (gpl-3.0-or-later)
#
# Respond only after receiving the CUL initialization commands on stdin.
#
# This script simulates a wM-Bus CUL so that the CUL command
# exchange (set link mode) can be tested without real hardware.
#
# When run by the testinternals test_serial_settings_restart test the
# SERIAL_BPS/SERIAL_PARITY (and $$ as a session id) are logged to
# $CMD_TEST_LOG so restarts with new serial settings can be checked.
if [ -n "$CMD_TEST_LOG" ]; then
    printf '%s/%s/%s\n' "$SERIAL_BPS" "$SERIAL_PARITY" "$$" >>"$CMD_TEST_LOG"
fi
# Signal that the command is up and running. The parent (wmbusmeters)
# may consume this line when waiting for a restarted command to start.
printf 'CUL ready\r\n'
if [ -n "$SERIAL_BPS" ]; then
    [ -n "$SERIAL_BITS" ] && [ -n "$SERIAL_PARITY" ] && [ -n "$SERIAL_STOPBITS" ] && [ -n "$SERIAL_FLOW" ] || exit 5
fi
IFS= read -r mode || exit 1
[ "$mode" = "$(printf 'brt\r')" ] || exit 2
printf 'cmd_cul.sh working\n' >&2
printf 'TMODE\r\n'
IFS= read -r start || exit 3
[ "$start" = "$(printf 'X21\r')" ] || exit 4
printf 'cmd_cul.sh command exchange complete\n' >&2
exec cat >/dev/null
