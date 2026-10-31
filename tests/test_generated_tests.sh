#!/bin/bash
# Run all driver tests in tests/generated_tests.xmq in parallel.
# Tests are split per driver so each driver's tests run in the
# same (sequential) order as before, with up to one driver per core.

PROG="$1"
XMQ=./build/xmq
SRC=tests/generated_tests.xmq

DIR=$(mktemp -d)
trap 'rm -rf "$DIR"' EXIT

# Split the test file into one file per driver.
# The driver name is the 2nd field in each test's args line.
awk -v dir="$DIR" '
function flush() {
    if (have) {
        f = sprintf("%s/%s.xmq", dir, drv)
        printf "%s", buf > f
        buf = ""
        have = 0
    }
}
/^test \{/ { flush() }
{
    line = $0 "\n"
    if (have) buf = buf line; else { have = 1; buf = line }
}
/^[[:space:]]*args[[:space:]]*=/ {
    split($0, a, "\047")
    split(a[2], b, " ")
    drv = b[2]
}
END { flush() }
' "$SRC"

# xmq complains when it cannot detect the terminal background color
# (not connected to a terminal when run in parallel).
export XMQ_BG=mono

JOBS=$(nproc 2> /dev/null || sysctl -n hw.ncpu 2> /dev/null || echo 2)

for i in "$DIR"/*.xmq ; do echo "$i" ; done | \
    xargs -P "$JOBS" -n 1 -I{} \
    $XMQ {} for-each /test --shell='./tests/testit.sh '$PROG' "${args}" "${telegram}" "${json}" "${fields}"'
RC=$?

# On failure, which driver group failed is printed by testit.sh above.
exit $([ "$RC" = "0" ] && echo 0 || echo 1)
