#!/bin/bash
# Copyright (C) 2026 Fredrik Öhrström (gpl-3.0-or-later)

TMP=$(mktemp)

# Use stdout if no argument is given, otherwise write to the specified file
if [ -z "$1" ]; then
    OUT="/dev/stdout"
else
    OUT="$1"
fi

# Retrieve the current year to update the copyright notice
CURRENT_YEAR=$(date +%Y)

(grep -rEc "Copyright \(C\) (....-)?.... [^\(]+ \(.+\)" src drivers/src | grep :0 > "$TMP")

if [ -s "$TMP" ]
then
    echo "These files do not have a proper copyright notice:"
    sed 's/:0//' "$TMP"
    exit 1
fi

(grep -rEo "Copyright \(C\) (....-)?.... [^\(]+ \(.+\)" src drivers/src \
    | cut -f 2 -d ':' \
    | tr -s ' ' \
    | sed 's/(C) \([0-9][0-9][0-9][0-9]\) /(C) \1-\1 /' > "$TMP")

{
    echo 'R"AUTHORS('
    echo "Copyright (C) 2017-${CURRENT_YEAR} Fredrik Öhrström"

    sed 's/ <.*>//' "$TMP" | \
        sed 's/ [0-9][0-9][0-9][0-9]-/ /' | \
        sed 's/ (.[^)].*)//g' | \
        grep -v Öhrström | \
        sort -rn | \
        sed 's/Copyright (C) /                   /' | \
        uniq

    echo ')AUTHORS";'
} > "$OUT"

# Cleanup the temporary file
rm -f "$TMP"