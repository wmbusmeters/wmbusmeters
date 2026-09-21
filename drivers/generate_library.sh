#!/bin/bash
# Copyright (C) 2026 Fredrik Öhrström (gpl-3.0-or-later)
#
# Generates src/generated_library.cc from library.xmq.
#
# The generated file contains the implementation of
# bool MeterCommonImplementation::addOptionalLibraryFields(string field_names)

export LIBRARY=${LIBRARY:-library.xmq}
export OUT=${OUT:-build/generated_library.cc}

die()
{
    echo "generate_library.sh: error: $*" >&2
    exit 1
}

[ -f "$LIBRARY" ] || die "cannot read $LIBRARY, run this script from the drivers directory"

mkdir -p build

# Print all template blocks separated by NUL so they can be read in bash.
template_blocks()
{
    xmq "$LIBRARY" select /library/template to-xmq \
    | LC_ALL=C awk '
        {
            buf = buf $0 "\n"
            if ($0 == "}")
            {
                # Only the unindented template closing brace ends a block,
                # inner closers are indented.
                printf "%s\0", buf
                buf = ""
            }
        }
        END { if (buf != "") printf "%s\0", buf }
    '
}

c_escape()
{
    local s=${1//\\/\\\\}
    s=${s//\"/\\\"}
    printf '%s' "$s"
}

# Extract a single "key = value" leaf from the current template block.
kv()
{
    printf '%s\n' "$BLOCK" | xmq select $1 to-text
}

quantity_c()
{
    case "$1" in
        Volume)       echo "Quantity::Volume" ;;
        Energy)       echo "Quantity::Energy" ;;
        Time)         echo "Quantity::Time" ;;
        Temperature)  echo "Quantity::Temperature" ;;
        Flow)         echo "Quantity::Flow" ;;
        PointInTime)  echo "Quantity::PointInTime" ;;
        Voltage)      echo "Quantity::Voltage" ;;
        Dimensionless) echo "Quantity::Dimensionless" ;;
        HCA)          echo "Quantity::HCA" ;;
        Text)         echo "Quantity::Text" ;;
        "")           echo "Quantity::Text" ;;
        *)            die "unknown quantity: $1" ;;
    esac
}

unit_c()
{
    case "$1" in
        s)        echo "Unit::Second" ;;
        h)        echo "Unit::Hour" ;;
        y)        echo "Unit::Year" ;;
        date)     echo "Unit::DateLT" ;;
        datetime) echo "Unit::DateTimeLT" ;;
        "")       echo "" ;;
        *)        die "unknown display_unit: $1" ;;
    esac
}

# Emit one C++ if-block for the current template (in $BLOCK).
emit_template()
{
    local id name quantity info change
    local vif_scaling dif_signedness attributes display_unit
    local measurement_type vif_range storage_nr add_combinable aliases
    local props condition unit

    id=$(printf '%s\n' "$BLOCK" | xmq select '/template/@id' to-text)
    [ -n "$id" ] || die "cannot parse template id from: $(printf '%s' "$BLOCK" | head -1)"

    name=$(kv /template/name)
    quantity=$(kv /template/quantity)
    info=$(kv /template/info)
    change=$(kv /template/change)
    vif_scaling=$(kv /template/vif_scaling)
    dif_signedness=$(kv /template/dif_signedness)
    attributes=$(kv /template/attributes)
    display_unit=$(kv /template/display_unit)
    measurement_type=$(kv /template/match/measurement_type)
    vif_range=$(kv /template/match/vif_range)
    storage_nr=$(kv /template/match/storage_nr)
    add_combinable=$(kv /template/match/add_combinable)
    aliases=$(kv /template/aliases)

    [ -n "$name" ] || die "template $id has no name"

    # Strip the surrounding single quotes from the info string.
    info=${info#\'}
    info=${info%\'}

    # condition: checkIf(fields,"id") plus any aliases.
    condition="checkIf(fields,\"$id\")"
    local IFS=','
    local alias
    for alias in $aliases; do
        alias=${alias//[[:space:]]/}
        [ -n "$alias" ] || continue
        condition="$condition || checkIf(fields,\"$alias\")"
    done

    # print properties.
    props="DEFAULT_PRINT_PROPERTIES"
    if [ -n "$attributes" ]
    then
        props=""
        local prop
        for prop in $attributes; do
            prop=${prop//[[:space:]]/}
            props+="${props:+ | }$prop"
        done
        [ -n "$props" ] || die "template $id has bad attributes: $attributes"
    fi

    # matcher.
    local NL=$'\n'
    local matcher=""
    [ -n "$measurement_type" ] && matcher="${matcher:+$matcher$NL            }.set(MeasurementType::$measurement_type)"
    [ -n "$vif_range" ] && matcher="${matcher:+$matcher$NL            }.set(VIFRange::$vif_range)"
    [ -n "$storage_nr" ] && matcher="${matcher:+$matcher$NL            }.set(StorageNr($storage_nr))"
    [ -n "$add_combinable" ] && matcher="${matcher:+$matcher$NL            }.add(VIFCombinable::$add_combinable)"

    unit=$(unit_c "$display_unit")

    local esc_info
    esc_info=$(c_escape "$info")

    if [ -z "$matcher" ]
    then
        # No matcher: a simple constant-like field.
        printf '    if (%s)\n' "$condition"
        printf '    {\n'
        printf '        addStringField(\n'
        printf '            "%s",\n' "$(c_escape "$name")"
        printf '            "%s"+help,\n' "$esc_info"
        printf '            %s);\n' "$props"
        printf '        markLastFieldAsLibrary();\n'
        printf '    }\n\n'
        return
    fi

    printf '    if (%s)\n' "$condition"
    printf '    {\n'

    local esc_name
    esc_name=$(c_escape "$name")

    if [ -z "$quantity" ] || [ "$quantity" = Text ]
    then
        printf '        addStringFieldWithExtractor(\n'
        printf '            "%s",\n' "$esc_name"
        printf '            "%s"+help,\n' "$esc_info"
        printf '            %s,\n' "$props"
        printf '            FieldMatcher::build()\n'
        printf '            %s            );\n' "$matcher"
    else
        local vsiging ds
        vsiging="VifScaling::${vif_scaling:-Auto}"
        case "${vif_scaling:-Auto}" in
            Auto|None|Unknown) ;;
            *) die "template $id has bad vif_scaling: $vif_scaling" ;;
        esac
        ds="DifSignedness::${dif_signedness:-Signed}"
        case "${dif_signedness:-Signed}" in
            Signed|Unsigned|Unknown) ;;
            *) die "template $id has bad dif_signedness: $dif_signedness" ;;
        esac
        if [ -n "$unit" ]
        then
            printf '        addNumericFieldWithExtractor(\n'
            printf '            "%s",\n' "$esc_name"
            printf '            "%s"+help,\n' "$esc_info"
            printf '            %s,\n' "$props"
            printf '            %s,\n' "$(quantity_c "$quantity")"
            printf '            %s,\n' "$vsiging"
            printf '            %s,\n' "$ds"
            printf '            FieldMatcher::build()\n'
            printf '            %s,\n' "$matcher"
            printf '            %s\n            );\n' "$unit"
        else
            printf '        addNumericFieldWithExtractor(\n'
            printf '            "%s",\n' "$esc_name"
            printf '            "%s"+help,\n' "$esc_info"
            printf '            %s,\n' "$props"
            printf '            %s,\n' "$(quantity_c "$quantity")"
            printf '            %s,\n' "$vsiging"
            printf '            %s,\n' "$ds"
            printf '            FieldMatcher::build()\n'
            printf '            %s            );\n' "$matcher"
        fi
    fi

    if [ -n "$change" ] && [ "$change" != Unknown ] && [ "$quantity" != Text ]
    then
        printf '        lastAddedField()->setChange(Change::%s);\n' "$change"
    fi

    printf '        markLastFieldAsLibrary();\n'
    printf '    }\n\n'
}

cat > "$OUT" <<EOF
/*
 Copyright (C) 2026 Fredrik Öhrström (gpl-3.0-or-later)

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

// This source is generated from drivers/library.xmq
// Run "cd drivers; make library" to regenerate this file.

#include"meters_common_implementation.h"
#include"util.h"

using namespace std;

namespace
{

bool checkIf(set<string> &fields, const char *s)
{
    if (fields.count(s) > 0)
    {
        fields.erase(s);
        return true;
    }

    return false;
}

bool checkFieldsEmpty(set<string> &fields, string driver_name)
{
    if (fields.size() > 0)
    {
        string info;
        for (auto &s : fields) { info += s+" "; }

        warning("(meter) when adding common fields to driver %s, these fields were not found: %s\n",
                driver_name.c_str(),
                info.c_str());
        return false;
    }
    return true;
}

} // end anonymous namespace

bool MeterCommonImplementation::addOptionalLibraryFields(string field_names)
{
    // Old C++ driver passes fields like: "target_date,target_volume_m3"
    // New xmq driver passes single field like: "target_date|Special help for target date."
    // or just: "target_date"

    set<string> fields = splitStringIntoSet(field_names, ',');
    string help;
    vector<string> helps = splitString(field_names, '|');
    if (helps.size() == 2)
    {
        fields.clear();
        fields.insert(helps[0]);
        help = " "+helps[1];
    }
    if (helps.size() > 2)
    {
        error(EXIT_DRIVER_ERROR, "Bad library field, only zero or one pipe | symbol is allowed: %s", field_names.c_str());
    }

EOF

while IFS= read -r -d '' BLOCK
do
    emit_template >> "$OUT"
done < <(template_blocks)

cat >> "$OUT" <<EOF
    if (!checkFieldsEmpty(fields, name()))
    {
        return false;
    }
    return true;
}
EOF
