#!/bin/bash
# Copyright (C) 2026 Fredrik Öhrström (gpl-3.0-or-later)

export PROG="$1"
export OUT="build/generated_database.h"

TMP=$(mktemp)

(grep -rEc "Copyright \(C\) (....-)?.... [^\(]+ \(.+\)" src ./src | grep :0 > "$TMP")

if [ -s "$TMP" ]
then
    echo "These files do not have a proper copyright notice:"
    sed 's/:0//' "$TMP"
    exit 1
fi

cat > $OUT <<EOF
/*
 Copyright (C) 2024-$(date +%Y) Fredrik Öhrström (gpl-3.0-or-later)
EOF

(grep -rEo "Copyright \(C\) (....-)?.... [^\(]+ \(.+\)" src ./src \
    | cut -f 2 -d ':' \
    | tr -s ' ' \
    | sed 's/ <.*>//' \
    | sed -E '
        s/(C) ([0-9]{4}) /(C) \2-\2 /
        s/\(([0-9]{4})-\1\)/(\1)/
    ' \
    | grep -v Öhrström \
    | sort -ru \
    | sed 's/^/ /' \
| sed -E '
    s/^( *Copyright \(C\)) ([0-9]{4}-[0-9]{4}) /\1 \2 /
    t
    s/^( *Copyright \(C\)) ([0-9]{4}) /\1      \2 /
'
) > "$TMP"

cat "$TMP" >> "$OUT"

cat >> $OUT <<EOF

 This program is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

// This source is generated from drivers/src/*.xmq
// Run "cd drivers; make install" to regenerate this file.

#include"drivers.h"

BuiltinDriver builtins_[] =
{
EOF

#    { "elster", "melster,kelster", "driver{name=elster meter_type=GasMeter default_fields=name,id,total_m3,timestamp detect{mvt=ELS,81,03}field{name=total quantity=Volume match{measurement_type=Instantaneous vif_range=Volume}}}", false },

for i in src/*.xmq
do
    NAME=$(basename $i)
    NAME="${NAME%.*}"
    ALIASES="$(xmq $i select /driver/aliases to-text)"
    CONTENT="$(xmq $i delete /driver/tests/test delete /driver/tests delete "//comment()" to-xmq --compact | sed 's/"/\\"/g')"
    cat >>$OUT <<EOF
    { "$NAME", "$ALIASES", "$CONTENT", false },
EOF
done

cat >> $OUT <<EOF
};

MapToDriver builtins_mvts_[] =
{
EOF

for i in src/*.xmq
do
    NAME=$(basename $i)
    export NAME="${NAME%.*}"
    (xmq $i for-each /driver/detect/mvt --shell='./print_mvt.sh "${.}" "$NAME"') >> $OUT
done

cat >> $OUT <<EOF
};
EOF
