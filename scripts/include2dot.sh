#!/usr/bin/env bash
set -Eeuo pipefail

###############################################################################
# include2dot.sh
#
# Generates Graphviz DOT dependency graphs for modern C++ projects.
#
# Features:
#   - C++ only (.cpp/.cc/.cxx/.hpp/.hh/.hxx/.h)
#   - Weighted edges (penwidth based on include frequency)
#   - System vs local include coloring
#   - Grouped edges
#   - Directory clustering
#   - Cycle detection
#   - Fan-in / fan-out statistics
#   - JSON export
#   - SVG/PNG/DOT generation
#   - Parallel scanning friendly architecture
#
# Example:
#   ./include2dot.sh \
#       --src . \
#       --group-dirs \
#       --ignore-system \
#       --output svg
#
###############################################################################

VERSION="2.0"

###############################################################################
# Defaults
###############################################################################

SRC="."
OUTPUT="dot"
OUTPUT_FILE="dependency-graph"
IGNORE_SYSTEM=0
GROUP_DIRS=0
DEBUG=0
THEME="dark"
JSON_EXPORT=0
FOCUS=""
EXCLUDE=""
TRANSITIVE_REDUCTION=0

INCLUDE_PATHS=()

###############################################################################
# Helpers
###############################################################################

log() {
    if [[ "$DEBUG" -eq 1 ]]; then
        echo "[debug] $*" >&2
    fi
}

fail() {
    echo "error: $*" >&2
    exit 1
}

usage() {
cat <<EOF
include2dot.sh v${VERSION}

Usage:
  $0 [options]

Options:
  --src DIR               Source directory (default: .)
  --include PATHS         Comma-separated include paths
  --output TYPE           dot|svg|png|json
  --output-file NAME      Output basename
  --ignore-system         Ignore system includes
  --group-dirs            Cluster nodes by directory
  --focus PATH            Only graph matching subtree
  --exclude REGEX         Ignore files matching regex
  --theme THEME           dark|light|mono
  --transitive-reduction  Apply Graphviz tred
  --debug                 Verbose logging
  --help                  Show help

Examples:
  $0 --src . --output svg
  $0 --ignore-system --group-dirs
EOF
}

###############################################################################
# Parse args
###############################################################################

while [[ $# -gt 0 ]]; do
    case "$1" in
        --src)
            SRC="$2"
            shift 2
            ;;

        --include)
            IFS=',' read -ra INCLUDE_PATHS <<< "$2"
            shift 2
            ;;

        --output)
            OUTPUT="$2"
            shift 2
            ;;

        --output-file)
            OUTPUT_FILE="$2"
            shift 2
            ;;

        --ignore-system)
            IGNORE_SYSTEM=1
            shift
            ;;

        --group-dirs)
            GROUP_DIRS=1
            shift
            ;;

        --focus)
            FOCUS="$2"
            shift 2
            ;;

        --exclude)
            EXCLUDE="$2"
            shift 2
            ;;

        --theme)
            THEME="$2"
            shift 2
            ;;

        --transitive-reduction)
            TRANSITIVE_REDUCTION=1
            shift
            ;;

        --debug)
            DEBUG=1
            shift
            ;;

        --help)
            usage
            exit 0
            ;;

        *)
            fail "unknown option: $1"
            ;;
    esac
done

###############################################################################
# Validation
###############################################################################

[[ -d "$SRC" ]] || fail "source directory does not exist"

case "$OUTPUT" in
    dot|svg|png|json)
        ;;
    *)
        fail "invalid output type"
        ;;
esac

###############################################################################
# Theme setup
###############################################################################

case "$THEME" in
    dark)
        GRAPH_BG="#1e1e1e"
        GRAPH_FG="#d0d0d0"
        LOCAL_EDGE="#4FC3F7"
        SYSTEM_EDGE="#777777"
        CYCLE_EDGE="#FF5555"
        NODE_FILL="#2b2b2b"
        ;;

    light)
        GRAPH_BG="#ffffff"
        GRAPH_FG="#202020"
        LOCAL_EDGE="#1565C0"
        SYSTEM_EDGE="#909090"
        CYCLE_EDGE="#D32F2F"
        NODE_FILL="#f4f4f4"
        ;;

    mono)
        GRAPH_BG="#ffffff"
        GRAPH_FG="#000000"
        LOCAL_EDGE="#000000"
        SYSTEM_EDGE="#666666"
        CYCLE_EDGE="#000000"
        NODE_FILL="#ffffff"
        ;;

    *)
        fail "unknown theme"
        ;;
esac

###############################################################################
# Utilities
###############################################################################

normalize_path() {
    local p="$1"
    realpath -m "$p" 2>/dev/null || echo "$p"
}

workspace_relative_path() {
    local p="$1"

    p="$(normalize_path "$p")"

    local src_root
    src_root="$(normalize_path "$SRC")"

    if [[ "$p" == "$src_root"/* ]]; then
        p="${p#"$src_root"/}"
    fi

    echo "$p"
}

escape_dot() {
    local s="$1"
    s="${s//\\/\\\\}"
    s="${s//\"/\\\"}"
    echo "$s"
}

find_include() {
    local include="$1"
    local current_file="$2"

    local candidate

    candidate="$(normalize_path "$(dirname "$current_file")/$include")"
    if [[ -f "$candidate" ]]; then
        echo "$candidate"
        return 0
    fi

    for dir in "${INCLUDE_PATHS[@]}"; do
        candidate="$(normalize_path "$dir/$include")"

        if [[ -f "$candidate" ]]; then
            echo "$candidate"
            return 0
        fi
    done

    if [[ -f "$include" ]]; then
        echo "$include"
        return 0
    fi

    return 1
}

###############################################################################
# File discovery
###############################################################################

log "discovering files"

mapfile -t FILES < <(
    find "$SRC" -type f \( \
        -name "*.cpp" -o \
        -name "*.cc"  -o \
        -name "*.cxx" -o \
        -name "*.hpp" -o \
        -name "*.hh"  -o \
        -name "*.hxx" -o \
        -name "*.h" \
    \)
)

###############################################################################
# Data structures
###############################################################################

declare -A EDGE_COUNT=()
declare -A EDGE_TYPE=()
declare -A FAN_IN=()
declare -A FAN_OUT=()
declare -A NODE_EXISTS=()
declare -A GROUPED=()
declare -a CYCLE_EDGES=()

TOTAL_INCLUDES=0

###############################################################################
# Include parsing
###############################################################################

parse_file() {
    local file="$1"

    log "parsing $file"

    local line
    local local_regex='^[[:space:]]*#[[:space:]]*include[[:space:]]+"([^"]+)"'
    local system_regex='^[[:space:]]*#[[:space:]]*include[[:space:]]+<([^>]+)>'

    while IFS= read -r line; do

        [[ "$line" =~ ^[[:space:]]*// ]] && continue

        local include
        local type

        if [[ "$line" =~ $local_regex ]]; then
            include="${BASH_REMATCH[1]}"
            type="local"

        elif [[ "$line" =~ $system_regex ]]; then
            include="${BASH_REMATCH[1]}"
            type="system"

        else
            continue
        fi

        if [[ "$type" == "system" && "$IGNORE_SYSTEM" -eq 1 ]]; then
            continue
        fi

        local resolved=""

        if [[ "$type" == "local" ]]; then
            resolved="$(find_include "$include" "$file" || true)"

            [[ -z "$resolved" ]] && continue
        else
            resolved="<$include>"
        fi

        if [[ -n "$FOCUS" ]]; then
            if [[ "$file" != *"$FOCUS"* && "$resolved" != *"$FOCUS"* ]]; then
                continue
            fi
        fi

        if [[ -n "$EXCLUDE" ]]; then
            if [[ "$file" =~ $EXCLUDE ]]; then
                continue
            fi
        fi

        local from
        local to

        from="$(workspace_relative_path "$file")"

        if [[ "$resolved" == \<*\> ]]; then
            to="$resolved"
        else
            to="$(workspace_relative_path "$resolved")"
        fi

        NODE_EXISTS["$from"]=1
        NODE_EXISTS["$to"]=1

        local key="$from|||$to"

        EDGE_COUNT["$key"]=$(( ${EDGE_COUNT["$key"]:-0} + 1 ))
        EDGE_TYPE["$key"]="$type"

        FAN_OUT["$from"]=$(( ${FAN_OUT["$from"]:-0} + 1 ))
        FAN_IN["$to"]=$(( ${FAN_IN["$to"]:-0} + 1 ))

        TOTAL_INCLUDES=$(( TOTAL_INCLUDES + 1 ))

    done < "$file"
}

###############################################################################
# Parse all files
###############################################################################

for file in "${FILES[@]}"; do
    parse_file "$file"
done

###############################################################################
# Cycle detection
###############################################################################

log "detecting cycles"

TMP_GRAPH="$(mktemp)"

for key in "${!EDGE_COUNT[@]}"; do
    from="${key%%|||*}"
    to="${key##*|||}"

    if [[ "$to" == \<*\> ]]; then
        continue
    fi

    echo "$from $to" >> "$TMP_GRAPH"
done

if command -v tsort >/dev/null 2>&1; then
    if ! tsort "$TMP_GRAPH" >/dev/null 2>&1; then
        log "cycles detected"
    fi
fi

###############################################################################
# DOT generation
###############################################################################

DOT_FILE="${OUTPUT_FILE}.dot"

log "writing $DOT_FILE"

{

cat <<EOF
digraph "C++ Include Graph" {

    graph [
        bgcolor="$GRAPH_BG"
        fontcolor="$GRAPH_FG"
        fontsize=18
        fontname="Helvetica"
        overlap=false
        splines=true
        rankdir=LR
        label="C++ Include Graph for wmbusmeters"
        labelloc=t
    ];

    node [
        shape=box
        style=filled
        fillcolor="$NODE_FILL"
        fontcolor="$GRAPH_FG"
        color="$GRAPH_FG"
        fontname="Helvetica"
    ];

    edge [
        fontname="Helvetica"
        color="$LOCAL_EDGE"
    ];

EOF

###############################################################################
# Directory clusters
###############################################################################

if [[ "$GROUP_DIRS" -eq 1 ]]; then

    declare -A DIR_NODES

    for node in "${!NODE_EXISTS[@]}"; do

        [[ "$node" == \<*\> ]] && continue

        dir="$(dirname "$node")"
        escaped_node="$(escape_dot "$node")"

        DIR_NODES["$dir"]+="\n\"$escaped_node\";"
    done

    for dir in "${!DIR_NODES[@]}"; do

        escaped_dir="$(escape_dot "$dir")"

        cat <<EOF
    subgraph "cluster_${escaped_dir}" {
        label="$(basename "$dir")";
        color="$GRAPH_FG";
        ${DIR_NODES[$dir]}
    }
EOF

    done
fi

###############################################################################
# Nodes with scaling
###############################################################################

for node in "${!NODE_EXISTS[@]}"; do

    escaped="$(escape_dot "$node")"

    fanin=${FAN_IN["$node"]:-0}
    fanout=${FAN_OUT["$node"]:-0}

    total=$(( fanin + fanout ))

    width=$(awk "BEGIN { print 1 + ($total / 10.0) }")

    cat <<EOF
    "$escaped" [
        width=$width
        tooltip="fanin=$fanin fanout=$fanout"
    ];
EOF

done

###############################################################################
# Group edges by source
###############################################################################

for key in "${!EDGE_COUNT[@]}"; do

    from="${key%%|||*}"
    to="${key##*|||}"

    GROUPED["$from"]+="$to|||"

done

for from in "${!GROUPED[@]}"; do

    escaped_from="$(escape_dot "$from")"

    printf '    "%s" -> { ' "$escaped_from"

    IFS='|||' read -ra targets <<< "${GROUPED[$from]}"

    done_targets=()

    for target in "${targets[@]}"; do

        [[ -z "$target" ]] && continue

        escaped_target="$(escape_dot "$target")"

        printf '"%s" ' "$escaped_target"

    done

    echo '};'

done

###############################################################################
# Weighted edges
###############################################################################

for key in "${!EDGE_COUNT[@]}"; do

    from="${key%%|||*}"
    to="${key##*|||}"

    escaped_from="$(escape_dot "$from")"
    escaped_to="$(escape_dot "$to")"

    count=${EDGE_COUNT[$key]}
    type=${EDGE_TYPE[$key]}

    penwidth=$(awk "BEGIN { print 1 + log($count + 1) }")

    color="$LOCAL_EDGE"

    if [[ "$type" == "system" ]]; then
        color="$SYSTEM_EDGE"
    fi

    cat <<EOF
    "$escaped_from" -> "$escaped_to" [
        color="$color"
        penwidth=$penwidth
        weight=$count
        tooltip="count=$count type=$type"
    ];
EOF

done

###############################################################################
# Legend
###############################################################################

cat <<EOF

    subgraph cluster_legend {
        label="Legend";

        key_local [label="Local include", shape=plaintext];
        key_system [label="System include", shape=plaintext];

        key_local -> key_system [color="$LOCAL_EDGE", penwidth=2];
        key_system -> key_local [color="$SYSTEM_EDGE", style=dashed];
    }

}
EOF

} > "$DOT_FILE"

###############################################################################
# Optional transitive reduction
###############################################################################

if [[ "$TRANSITIVE_REDUCTION" -eq 1 ]]; then

    if command -v tred >/dev/null 2>&1; then

        log "running transitive reduction"

        tred "$DOT_FILE" > "${DOT_FILE}.tmp"
        mv "${DOT_FILE}.tmp" "$DOT_FILE"

    else
        log "tred not installed"
    fi
fi

###############################################################################
# JSON export
###############################################################################

if [[ "$OUTPUT" == "json" ]]; then

    JSON_FILE="${OUTPUT_FILE}.json"

    {
        echo '{'
        echo '  "nodes": ['

        first=1

        for node in "${!NODE_EXISTS[@]}"; do

            [[ $first -eq 0 ]] && echo ','
            first=0

            escaped="$(printf '%s' "$node" | sed 's/"/\\"/g')"

            printf '    {"id":"%s"}' "$escaped"

        done

        echo
        echo '  ],'
        echo '  "edges": ['

        first=1

        for key in "${!EDGE_COUNT[@]}"; do

            from="${key%%|||*}"
            to="${key##*|||}"

            [[ $first -eq 0 ]] && echo ','
            first=0

            count=${EDGE_COUNT[$key]}
            type=${EDGE_TYPE[$key]}

            from_escaped="$(printf '%s' "$from" | sed 's/"/\\"/g')"
            to_escaped="$(printf '%s' "$to" | sed 's/"/\\"/g')"

            printf '    {"from":"%s","to":"%s","count":%s,"type":"%s"}' \
                "$from_escaped" \
                "$to_escaped" \
                "$count" \
                "$type"

        done

        echo
        echo '  ]'
        echo '}'

    } > "$JSON_FILE"

    echo "generated: $JSON_FILE"
    exit 0
fi

###############################################################################
# Render outputs
###############################################################################

case "$OUTPUT" in

    dot)
        echo "generated: $DOT_FILE"
        ;;

    svg)
        dot -Tsvg "$DOT_FILE" -o "${OUTPUT_FILE}.svg"
        echo "generated: ${OUTPUT_FILE}.svg"
        ;;

    png)
        dot -Tpng "$DOT_FILE" -o "${OUTPUT_FILE}.png"
        echo "generated: ${OUTPUT_FILE}.png"
        ;;

esac

###############################################################################
# Statistics
###############################################################################

echo

echo "================ Statistics ================"
echo "Files scanned     : ${#FILES[@]}"
echo "Nodes             : ${#NODE_EXISTS[@]}"
echo "Edges             : ${#EDGE_COUNT[@]}"
echo "Total includes    : $TOTAL_INCLUDES"
echo

echo "Top fan-in files:"

for node in "${!FAN_IN[@]}"; do
    echo "${FAN_IN[$node]} $node"
done | sort -rn | head -10

echo

echo "Top fan-out files:"

for node in "${!FAN_OUT[@]}"; do
    echo "${FAN_OUT[$node]} $node"
done | sort -rn | head -10

echo

echo "Done."

###############################################################################
# End
###############################################################################
