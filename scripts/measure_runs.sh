#!/usr/bin/env bash

set -euo pipefail

# Detect UTF‑8 support
if [[ "$(locale charmap 2>/dev/null)" == "UTF-8" ]] || [[ "$LANG" =~ UTF-8 ]]; then
    USE_UNICODE=true
    FULL_BLOCK='█'
    EMPTY_BLOCK='░'
else
    USE_UNICODE=false
    FULL_BLOCK='#'
    EMPTY_BLOCK='-'
fi

# Determine number of runs
if [[ $# -gt 0 && "$1" =~ ^[0-9]+$ ]]; then
    RUNS="$1"
    shift
else
    RUNS=10
fi

# Determine command
if [[ $# -eq 0 ]]; then
    CMD="./build/wmbusmeters --version"
else
    CMD="$*"
fi

# Check executable
FIRST_WORD="${CMD%% *}"
if ! command -v "$FIRST_WORD" &>/dev/null && ! [[ -x "$FIRST_WORD" ]]; then
    printf "Error: Command not found or not executable: %s\n" "$FIRST_WORD" >&2
    exit 1
fi

# Nanosecond support
if date +%s%N &>/dev/null; then
    HAVE_NANOSECONDS=true
else
    HAVE_NANOSECONDS=false
fi

# ANSI color codes
GREEN='\033[0;32m'
CYAN='\033[0;36m'
YELLOW='\033[1;33m'
RESET='\033[0m'

declare -a times_ns
declare -a times_ms

# Print header with colors using printf (interprets \033)
printf "Measuring startup time of: ${CYAN}%s${RESET}\n" "$CMD"
printf "Runs: ${YELLOW}%s${RESET}\n" "$RUNS"
echo

# Spinner characters
if $USE_UNICODE; then
    SPINNER=('⠋' '⠙' '⠹' '⠸' '⠼' '⠴' '⠦' '⠧' '⠇' '⠏')
else
    SPINNER=('|' '/' '-' '\\')
fi

# Helper: build a progress bar
build_bar() {
    local percent=$1
    local width=40
    local filled=$((percent * width / 100))
    local empty=$((width - filled))
    local bar=""
    for ((j=0; j<filled; j++)); do
        bar+="$FULL_BLOCK"
    done
    for ((j=0; j<empty; j++)); do
        bar+="$EMPTY_BLOCK"
    done
    printf "%s" "$bar"
}

for ((i=1; i<=RUNS; i++)); do
    # Measure time
    if $HAVE_NANOSECONDS; then
        start=$(date +%s%N)
        eval "$CMD" >/dev/null 2>&1
        end=$(date +%s%N)
        elapsed_ns=$((end - start))
        elapsed_ms=$((elapsed_ns / 1000000))
        times_ns+=("$elapsed_ns")
        times_ms+=("$elapsed_ms")
    else
        start=$(date +%s.%N 2>/dev/null || date +%s)
        eval "$CMD" >/dev/null 2>&1
        end=$(date +%s.%N 2>/dev/null || date +%s)
        elapsed=$(echo "$end - $start" | bc)
        elapsed_ms=$(echo "$elapsed * 1000" | bc | cut -d'.' -f1)
        times_ms+=("$elapsed_ms")
    fi

    # Progress
    percent=$((i * 100 / RUNS))
    bar=$(build_bar "$percent")
    spin_idx=$(( (i-1) % ${#SPINNER[@]} ))
    spinner="${SPINNER[$spin_idx]}"

    # Overwrite line using printf with \r and color codes
    printf "\r${GREEN}%s${RESET} Progress: [${CYAN}%s${RESET}] %3d%%  (%d/%d) " \
           "$spinner" "$bar" "$percent" "$i" "$RUNS"
done

printf "\n\n"

# Statistics function
compute_stats() {
    local -n arr=$1
    local sum=0 min=${arr[0]} max=${arr[0]}
    for val in "${arr[@]}"; do
        sum=$((sum + val))
        ((val < min)) && min=$val
        ((val > max)) && max=$val
    done
    local avg=$((sum / RUNS))
    local sum_sq=0
    for val in "${arr[@]}"; do
        local diff=$((val - avg))
        sum_sq=$((sum_sq + diff * diff))
    done
    local stddev
    if command -v bc &>/dev/null; then
        stddev=$(echo "sqrt($sum_sq / $RUNS)" | bc)
    else
        stddev=$(( (sum_sq / RUNS) ** 1 / 1 ))
    fi
    printf "%d %d %d %d" "$min" "$max" "$avg" "$stddev"
}

if $HAVE_NANOSECONDS; then
    read -r min_ns max_ns avg_ns stddev_ns <<< "$(compute_stats times_ns)"
    read -r min_ms max_ms avg_ms stddev_ms <<< "$(compute_stats times_ms)"
    printf "${YELLOW}Results in milliseconds:${RESET}\n"
    printf "  Runs    : ${CYAN}%d${RESET}\n" "$RUNS"
    printf "  Min     : ${GREEN}%d ms${RESET}  (%'d ns)\n" "$min_ms" "$min_ns"
    printf "  Max     : ${GREEN}%d ms${RESET}  (%'d ns)\n" "$max_ms" "$max_ns"
    printf "  Average : ${GREEN}%d ms${RESET}  (%'d ns)\n" "$avg_ms" "$avg_ns"
    printf "  Std Dev : ${GREEN}%d ms${RESET}  (%d ns)\n" "$stddev_ms" "$stddev_ns"
else
    read -r min_ms max_ms avg_ms stddev_ms <<< "$(compute_stats times_ms)"
    printf "${YELLOW}Results in milliseconds:${RESET}\n"
    printf "  Runs    : ${CYAN}%d${RESET}\n" "$RUNS"
    printf "  Min     : ${GREEN}%d ms${RESET}\n" "$min_ms"
    printf "  Max     : ${GREEN}%d ms${RESET}\n" "$max_ms"
    printf "  Average : ${GREEN}%d ms${RESET}\n" "$avg_ms"
    printf "  Std Dev : ${GREEN}%d ms${RESET}\n" "$stddev_ms"
fi