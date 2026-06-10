#!/bin/sh
# Tests for DES-CBC decryption (EN 13757-7:2018 security modes 2 and 3).
# Uses --analyze so no driver is required.

PROG="$1"
# Mode 2 telegram: IV=0, key=0102030405060708
#   Plaintext: 2F 2F | 04 13 E8 03 00 00 (volume=1000 L) | 2F... (filler)
TELEGRAM=1E468B050100000004077A01001002BBD1EAFD0222F871CB70DA16C78AE098
KEY=0102030405060708

# ---------------------------------------------------------------------------
TESTNAME="DES mode 2: correct key — 2F2F check bytes OK"
$PROG --analyze=$KEY $TELEGRAM 2>&1 | grep -q "2f2f decrypt check bytes (OK)"
if [ "$?" = "0" ]; then echo "OK: $TESTNAME"; else echo "ERROR: $TESTNAME"; exit 1; fi

# ---------------------------------------------------------------------------
TESTNAME="DES mode 2: correct key — volume decoded"
$PROG --analyze=$KEY $TELEGRAM 2>&1 | grep -q "E8030000"
if [ "$?" = "0" ]; then echo "OK: $TESTNAME"; else echo "ERROR: $TESTNAME"; exit 1; fi

# ---------------------------------------------------------------------------
TESTNAME="DES mode 2: no key — CE: encrypted marker present"
$PROG --analyze $TELEGRAM 2>&1 | grep -qE "CE:"
if [ "$?" = "0" ]; then echo "OK: $TESTNAME"; else echo "ERROR: $TESTNAME"; exit 1; fi

# ---------------------------------------------------------------------------
TESTNAME="DES mode 2: wrong key — check bytes fail"
$PROG --analyze=FFFFFFFFFFFFFFFF $TELEGRAM 2>&1 | grep -q "ERROR should be 2f2f"
if [ "$?" = "0" ]; then echo "OK: $TESTNAME"; else echo "ERROR: $TESTNAME"; exit 1; fi

# ---------------------------------------------------------------------------
# Mode 3: IV = ID + mfct + date-type-G (today's date)
# Generate a fresh telegram each time using today's UTC date.
# ---------------------------------------------------------------------------
TESTNAME="DES mode 3: correct key + today's date — 2F2F check bytes OK"

MODE3_TELEGRAM=$(python3 - <<'PYEOF'
from Crypto.Cipher import DES
import datetime, sys

key   = bytes.fromhex('0102030405060708')
devid = bytes.fromhex('01000000')   # device id LE (00000001)
mfct  = bytes.fromhex('8B05')       # ALK

d = datetime.datetime.now(datetime.timezone.utc).date()
year2 = d.year - 2000
month = d.month
day   = d.day
b0 = ((month & 0x07) << 5) | (day & 0x1F)
b1 = ((year2 & 0x7F) << 1) | ((month >> 3) & 0x01)
date_g = bytes([b0, b1])

iv = devid + mfct + date_g   # 8 bytes

# Plaintext: 2F 2F + volume 1000 L + 2F filler to 16 bytes
plaintext = bytes.fromhex('2F2F0413E80300002F2F2F2F2F2F2F2F')
ct = DES.new(key, DES.MODE_CBC, iv=iv).encrypt(plaintext)

# Telegram: CI=7A, acc=01, sts=00, cfg=1003 (N=16 bytes, mode 3)
header = bytes.fromhex('468B050100000004077A01001003')
body   = header + ct
print(bytes([len(body)]).hex() + body.hex().upper(), end='')
PYEOF
)

$PROG --analyze=$KEY $MODE3_TELEGRAM 2>&1 | grep -q "2f2f decrypt check bytes (OK)"
if [ "$?" = "0" ]; then echo "OK: $TESTNAME"; else echo "ERROR: $TESTNAME"; exit 1; fi

# ---------------------------------------------------------------------------
TESTNAME="DES mode 3: no key — CE: encrypted marker present"
$PROG --analyze $MODE3_TELEGRAM 2>&1 | grep -qE "CE:"
if [ "$?" = "0" ]; then echo "OK: $TESTNAME"; else echo "ERROR: $TESTNAME"; exit 1; fi

# ---------------------------------------------------------------------------
TESTNAME="DES mode 3: wrong key — check bytes fail"
$PROG --analyze=FFFFFFFFFFFFFFFF $MODE3_TELEGRAM 2>&1 | grep -q "ERROR should be 2f2f\|failed"
if [ "$?" = "0" ]; then echo "OK: $TESTNAME"; else echo "ERROR: $TESTNAME"; exit 1; fi
