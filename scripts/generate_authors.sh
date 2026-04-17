#!/bin/bash
# Copyright (C) 2026 Fredrik Öhrström (gpl-3.0-or-later)

TMP=$(mktemp)
NEWOUT=$(mktemp)

if [ -z "$1" ]; then
    echo "Usage: generate_authors.sh [outfile]"
    exit 0
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
    echo "/*"
    echo " Copyright (C) 2017-$CURRENT_YEAR Fredrik Öhrström (gpl-3.0-or-later)"
    echo ""
    echo " This program is free software: you can redistribute it and/or modify"
    echo " it under the terms of the GNU General Public License as published by"
    echo " the Free Software Foundation, either version 3 of the License, or"
    echo " (at your option) any later version."
    echo ""
    echo " This program is distributed in the hope that it will be useful,"
    echo " but WITHOUT ANY WARRANTY; without even the implied warranty of"
    echo " MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the"
    echo " GNU General Public License for more details."
    echo ""
    echo " You should have received a copy of the GNU General Public License"
    echo " along with this program.  If not, see <http://www.gnu.org/licenses/>."
    echo "*/"
    echo ""
    echo "#ifndef AUTHORS_H"
    echo "#define AUTHORS_H"
    echo ""
    echo 'inline constexpr const char* AUTHORS = R"AUTHORS('
    echo "Copyright (C) 2017-${CURRENT_YEAR} Fredrik Öhrström"

    sed 's/ <.*>//' "$TMP" | \
        sed 's/ [0-9][0-9][0-9][0-9]-/ /' | \
        sed 's/ (.[^)].*)//g' | \
        grep -v Öhrström | \
        sort -rn | \
        sed 's/Copyright (C) /                   /' | \
        uniq

    echo ')AUTHORS";'
    echo "#endif"
} > "$NEWOUT"

if [ ! -f $OUT ]
then
    # The new author files does not exist, write it.
    cp $NEWOUT $OUT
    echo "Wrote $OUT"
else
    if ! diff $OUT $NEWOUT > /dev/null 2>&1
    then
        # The author file exists and the new is different, write it.
        cp $NEWOUT $OUT
        echo "Wrote $OUT"
    fi
fi

# Cleanup the temporary files.
rm -f "$NEWOUT"
rm -f "$TMP"
