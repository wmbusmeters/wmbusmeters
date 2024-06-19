#!/bin/bash

export PROG="$1"
export OUT="build/generated_database.cc"

cat > $OUT <<EOF
/*
 Copyright (C) 2024 Fredrik Öhrström (gpl-3.0-or-later)

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

// Generated $(date +%Y-%m-%d_%H:%M)

BuiltinDriver builtins_[] =
{
EOF

#    { "elster", "driver{name=elster meter_type=GasMeter default_fields=name,id,total_m3,timestamp detect{mvt=ELS,81,03}field{name=total quantity=Volume match{measurement_type=Instantaneous vif_range=Volume}}}", false },

for i in src/*.xmq
do
    NAME=$(basename $i)
    NAME="${NAME%.*}"
    CONTENT="$(xmq $i delete /driver/test delete "//comment()" to-xmq --compact | sed 's/"/\\"/g')"
    cat >>$OUT <<EOF
    { "$NAME", "$CONTENT", false },
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
