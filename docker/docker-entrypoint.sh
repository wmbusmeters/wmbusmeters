#!/bin/sh
set -e

: ${WMBUSMETERS_DATA:=/wmbusmeters_data}
: ${LOGS_DIR:=${WMBUSMETERS_DATA}/logs}
: ${READINGS_DIR:=${LOGS_DIR}/readings}
: ${ETC_DIR:=${WMBUSMETERS_DATA}/etc}

[ ! -d "$LOGS_DIR" ] && mkdir -p "$LOGS_DIR"
[ ! -d "$READINGS_DIR" ] && mkdir -p "$READINGS_DIR"
[ ! -d "$ETC_DIR/wmbusmeters.d" ] && mkdir -p "$ETC_DIR/wmbusmeters.d"
[ ! -d "$ETC_DIR/wmbusmeters.drivers.d" ] && mkdir -p "$ETC_DIR/wmbusmeters.drivers.d"

if [ ! -f "$ETC_DIR/wmbusmeters.conf" ]; then
cat <<EOF > "$ETC_DIR/wmbusmeters.conf"
loglevel=normal
device=auto:t1
donotprobe=/dev/ttyAMA0
logtelegrams=false
format=json
meterfiles=${READINGS_DIR}
meterfilesaction=overwrite
logfile=${LOGS_DIR}/wmbusmeters.log
EOF
fi

exec /wmbusmeters/wmbusmeters --useconfig="$ETC_DIR"
