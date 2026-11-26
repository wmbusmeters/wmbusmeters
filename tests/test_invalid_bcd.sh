#!/bin/sh

. tests/include.sh
PROG="$1"
mkdir -p testoutput
OUTPUT=testoutput/invalid_bcd.json

# Decrypted and anonymized Hydrus telegram supplied in issue #2034.
TELEGRAM=5344A5117856341263078C0085900F002C2575300B00A3E59CAF0AB9B6007AD0E03107102F2F_0C1392450400426CFFFF4C13AAAAAAAA84046D3B375E368C04136164030004FD170000000D0A5A05020B3B000000
"$PROG" --format=json "$TELEGRAM" test dme173 12345678 NOKEY > "$OUTPUT"
if jq -e '.total_at_set_date_m3 == null and .set_date == null and .total_m3 == 44.592 and .target_m3 == 36.461 and .target_datetime == "2026-06-30 23:59"' "$OUTPUT" >/dev/null
then
    printOK "invalid BCD measurement is null while valid readings are retained"
else
    cat "$OUTPUT"
    printERROR "invalid BCD measurement became a numeric reading"
    exit 1
fi
