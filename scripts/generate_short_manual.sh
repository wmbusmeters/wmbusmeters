#!/bin/bash
# Copyright (C) 2026 Fredrik Öhrström (gpl-3.0-or-later)

TMP=$(mktemp)

# Use stdout if no argument is given, otherwise write to the specified file
if [ -z "$1" ]; then
    OUT="/dev/stdout"
else
    OUT="$1"
fi

{
    echo "/*"
    echo " Copyright (C) 2017-2026 Fredrik Öhrström (gpl-3.0-or-later)"
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
    echo "#ifndef SHORT_MANUAL_H"
    echo "#define SHORT_MANUAL_H"
    echo ""
    echo 'inline constexpr const char* SHORT_MANUAL = R"MANUAL('
    sed -n '/wmbusmeters version/,/```/p' README.md \
        | grep -v 'wmbusmeters version' \
        | grep -v '```'

    echo ')MANUAL";'
    echo "#endif"
} > "$OUT"

# Cleanup the temporary file
rm -f "$TMP"
