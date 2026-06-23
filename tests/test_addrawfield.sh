#!/bin/sh

PROG="$1"
TEST=testoutput
mkdir -p $TEST

TESTNAME="Test addrawfield feature"
TESTRESULT="ERROR"

# Use a known unencrypted telegram (lansenpu pulse counter)
HEX="234433300602010014007a8e0400002f2f0efd3a1147000000008e40fd3a341200000000"

# Test 1: Without --addrawfield, _raw should NOT be present
$PROG --silent --format=json $HEX MyCounter auto 00010206 NOKEY 2> $TEST/test_stderr.txt | jq --sort-keys . > $TEST/test_no_raw.txt
HAS_RAW=$(jq 'has("_raw")' $TEST/test_no_raw.txt)
if [ "$HAS_RAW" != "false" ]; then
    echo "ERROR: _raw present without --addrawfield"
    exit 1
fi
echo "OK: Without --addrawfield, _raw is absent"

# Test 2: With --addrawfield, _raw should be present and non-empty
$PROG --silent --format=json --addrawfield $HEX MyCounter auto 00010206 NOKEY 2> $TEST/test_stderr.txt | jq --sort-keys . > $TEST/test_with_raw.txt
HAS_RAW=$(jq 'has("_raw")' $TEST/test_with_raw.txt)
RAW_VALUE=$(jq -r '._raw' $TEST/test_with_raw.txt)
if [ "$HAS_RAW" != "true" ]; then
    echo "ERROR: _raw absent with --addrawfield"
    exit 1
fi
if [ -z "$RAW_VALUE" ] || [ "$RAW_VALUE" = "null" ]; then
    echo "ERROR: _raw is empty"
    exit 1
fi
echo "OK: With --addrawfield, _raw is present"

# Test 3: Verify _raw value matches the input hex (bin2hex outputs uppercase)
EXPECTED=$(echo "$HEX" | tr '[:lower:]' '[:upper:]')
if [ "$RAW_VALUE" != "$EXPECTED" ]; then
    echo "ERROR: _raw value mismatch. Expected $EXPECTED, got $RAW_VALUE"
    exit 1
fi
echo "OK: _raw value matches input hex"

# Test 4: With --ppjson --addrawfield, _raw should be properly formatted
$PROG --silent --format=json --ppjson --addrawfield $HEX MyCounter auto 00010206 NOKEY 2> $TEST/test_stderr.txt | jq --sort-keys . > $TEST/test_pp_raw.txt
HAS_RAW=$(jq 'has("_raw")' $TEST/test_pp_raw.txt)
if [ "$HAS_RAW" != "true" ]; then
    echo "ERROR: --ppjson --addrawfield missing _raw"
    exit 1
fi
echo "OK: --ppjson --addrawfield produces valid output"

echo "OK: $TESTNAME"
exit 0
