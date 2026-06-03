#!/bin/sh

PROG="$1"
export TZ=UTC

TEST=testoutput
OUTFILE=$TEST/test_output.txt
CURLLOG=$TEST/curl_called.log
FAKEHOME=$PWD/$TEST/fakehome
FAKEBIN=$PWD/$TEST/fakebin
CACHEDIR=$FAKEHOME/.local/share/wmbusmeters/wmbusmeters.drivers.d

TELEGRAM=1844AE4C4455223399077A55000000_041389E20100023B0000
METER_NAME=Hej
DRIVER=@iporl
METER_ID=33225544
METER_KEY=NOKEY
# This is the known decoded total for TELEGRAM with the iporl fixture driver.
EXPECTED_TOTAL=123.529

RC=0
LAST_CMD_RC=0

run_normal() {
    "$PROG" --format=json "$TELEGRAM" "$METER_NAME" "$DRIVER" "$METER_ID" "$METER_KEY" > "$OUTFILE" 2>&1
    LAST_CMD_RC=$?
}

run_nonet() {
    "$PROG" --nonet --format=json "$TELEGRAM" "$METER_NAME" "$DRIVER" "$METER_ID" "$METER_KEY" > "$OUTFILE" 2>&1
    LAST_CMD_RC=$?
}

run_basicauth() {
    "$PROG" --basicauth=user:pass --format=json "$TELEGRAM" "$METER_NAME" "$DRIVER" "$METER_ID" "$METER_KEY" > "$OUTFILE" 2>&1
    LAST_CMD_RC=$?
}

fail_case() {
    echo "ERROR $1"
    cat "$OUTFILE"
    RC=1
}

assert_exit_zero() {
    if [ "$LAST_CMD_RC" != "0" ]
    then
        return 1
    fi
    return 0
}

assert_exit_nonzero() {
    if [ "$LAST_CMD_RC" = "0" ]
    then
        return 1
    fi
    return 0
}

assert_total_value() {
    if command -v jq > /dev/null 2>&1
    then
        jq -e --argjson expected "$EXPECTED_TOTAL" '.totalitator_m3 == $expected' "$OUTFILE" > /dev/null 2>&1
    else
        grep -F '"totalitator_m3":123.529' "$OUTFILE" > /dev/null
    fi
}

assert_no_such_driver() {
    grep -F 'No such driver @iporl' "$OUTFILE" > /dev/null
}

rm -rf "$TEST"
mkdir -p "$FAKEHOME" "$FAKEBIN"

export FAKE_CURL_LOGFILE="$CURLLOG"

cat > "$FAKEBIN/curl" <<'EOF'
#!/bin/sh

LOGFILE="${FAKE_CURL_LOGFILE:-$PWD/testoutput/curl_called.log}"
MODE="${FAKE_CURL_MODE:-200}"
OUTFILE=""
URL=""
AUTH=""
ALL_ARGS="$*"

while [ "$#" -gt 0 ]
do
    case "$1" in
        -o)
            shift
            OUTFILE="$1"
            ;;
        -u)
            shift
            AUTH="$1"
            ;;
        *)
            URL="$1"
            ;;
    esac
    shift
done

mkdir -p "$(dirname "$LOGFILE")"
echo "MODE=$MODE ARGS=$ALL_ARGS AUTH=$AUTH OUT=$OUTFILE URL=$URL" >> "$LOGFILE"

case "$MODE" in
    200)
        DRIVER_FILE=$(basename "$URL")
        DRIVER_NAME=${DRIVER_FILE%.xmq}
        mkdir -p "$(dirname "$OUTFILE")"
        cat > "$OUTFILE" <<DRIVER_EOF
driver {
    name           = $DRIVER_NAME
    meter_type     = WaterMeter
    default_fields = name,id,total_m3,max_flow_m3h,timestamp
    detect {
        mvt = SEN,99,07
    }
    fields {
        field {
            name     = totalitator
            quantity = Volume
            match {
                measurement_type = Instantaneous
                vif_range        = Volume
            }
            about {
                en = 'The total water consumption recorded by this meter.'
            }
        }
        field {
            name     = max_flowwor
            quantity = Flow
            match {
                measurement_type = Instantaneous
                vif_range        = VolumeFlow
            }
            about {
                en = 'The maximum flow recorded during previous period.'
            }
        }
    }
}
DRIVER_EOF
        printf 200
        exit 0
        ;;
    304)
        printf 304
        exit 0
        ;;
    404)
        printf 404
        exit 22
        ;;
    500)
        printf 500
        exit 22
        ;;
    fail)
        exit 22
        ;;
    *)
        printf 500
        exit 22
        ;;
esac
EOF

chmod +x "$FAKEBIN/curl"

export HOME="$FAKEHOME"
export PATH="$FAKEBIN:$PATH"

FAKE_CURL_MODE=200
export FAKE_CURL_MODE
run_normal
if assert_exit_zero &&
   assert_total_value &&
    [ -d "$CACHEDIR" ] &&
   [ -f "$CACHEDIR/iporl.xmq" ] &&
   [ -s "$CURLLOG" ]
then
    echo "OK: Test downloaded driver (200, no auth)"
else
    fail_case "Failed downloaded driver (200, no auth)!"
fi

FAKE_CURL_MODE=304
export FAKE_CURL_MODE
run_normal
if assert_exit_zero &&
    assert_total_value &&
    grep -F -- '-z ' "$CURLLOG" > /dev/null
then
    echo "OK: Test driver cache validation (304)"
else
    fail_case "Failed driver cache validation (304)!"
fi

: > "$CURLLOG"
FAKE_CURL_MODE=200
export FAKE_CURL_MODE
run_nonet
if assert_exit_zero &&
   assert_total_value &&
   [ ! -s "$CURLLOG" ]
then
    echo "OK: Test --nonet uses cache without curl"
else
    fail_case "Failed --nonet cache check!"
fi

rm -f "$CACHEDIR/iporl.xmq"
: > "$CURLLOG"
run_nonet
if assert_exit_nonzero &&
   grep -F 'no driver iporl found in cache and no network access' "$OUTFILE" > /dev/null &&
    assert_no_such_driver &&
   [ ! -s "$CURLLOG" ]
then
    echo "OK: Test --nonet without cache"
else
    fail_case "Failed --nonet without cache check!"
fi

rm -f "$CACHEDIR/iporl.xmq"
FAKE_CURL_MODE=404
export FAKE_CURL_MODE
run_normal
if assert_exit_nonzero &&
   assert_no_such_driver
then
    echo "OK: Test download 404 not found"
else
    fail_case "Failed 404 not found check!"
fi

rm -f "$CACHEDIR/iporl.xmq"
FAKE_CURL_MODE=500
export FAKE_CURL_MODE
run_normal
if assert_exit_nonzero &&
   assert_no_such_driver
then
    echo "OK: Test download 500 server error"
else
    fail_case "Failed 500 server error check!"
fi

rm -f "$CACHEDIR/iporl.xmq"
FAKE_CURL_MODE=fail
export FAKE_CURL_MODE
run_normal
if assert_exit_nonzero &&
   grep -F 'failed to fetch .xmq using this command' "$OUTFILE" > /dev/null &&
   assert_no_such_driver
then
    echo "OK: Test download command failure without status output"
else
    fail_case "Failed download command failure check!"
fi

: > "$CURLLOG"
rm -f "$CACHEDIR/iporl.xmq"
FAKE_CURL_MODE=200
export FAKE_CURL_MODE
run_basicauth
if assert_exit_zero &&
    assert_total_value &&
    grep -F -- '-u user:pass' "$CURLLOG" > /dev/null
then
    echo "OK: Test basicauth passthrough"
else
    fail_case "Failed basicauth passthrough check!"
fi

exit $RC