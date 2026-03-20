#!/bin/bash
# wmbusmeters WASM build script
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
SRC="$SCRIPT_DIR/../src"
OUT="$SCRIPT_DIR/dist"
OBJ="$SCRIPT_DIR/obj"
LIBXML2="$SCRIPT_DIR/../3rdparty/libxml2"
LIBXML2_A="$LIBXML2/.libs/libxml2.a"
WASM_OPT_FLAGS="-O4 --converge --strip-debug --strip-dwarf --strip-producers"

mkdir -p "$OUT" "$OBJ"

CXX_FLAGS="-std=c++11 -O3 -I$SRC -I$SCRIPT_DIR -I$LIBXML2/include -D__EMSCRIPTEN_BUILD__ -include ./usleep_async.h "

compile_if_needed() {
    local src="$1"
    local extra_flags="${2:-}"
    local obj="$OBJ/$(basename ${src%.*}).o"
    if [ ! -f "$obj" ] || [ "$src" -nt "$obj" ]; then
        echo "  [compile] $(basename $src)" >&2
        em++ $CXX_FLAGS $extra_flags -c "$src" -o "$obj"
    fi
    echo "$obj"
}

# ── 1. libxml2 ────────────────────────────────────────────────────────────────
if [ ! -f "$LIBXML2_A" ]; then
  echo "==> Building libxml2 for WASM..."
  cd "$LIBXML2"
  [ ! -f "./configure" ] && ./autogen.sh
  emconfigure ./configure --enable-static --disable-shared --without-http --without-ftp \
    --without-modules --without-python --without-zlib --without-lzma --without-threads \
    --without-iconv --without-icu --host=wasm32-unknown-emscripten
  emmake make -j$(nproc)
  cd "$SCRIPT_DIR"
else
  echo "==> libxml2 already built, skipping"
fi

# ── 2. xmq.c ─────────────────────────────────────────────────────────────────
XMQ_OBJ="$OBJ/xmq.o"
if [ ! -f "$XMQ_OBJ" ] || [ "$SRC/xmq.c" -nt "$XMQ_OBJ" ]; then
    echo "  [compile] xmq.c" >&2
    emcc -O2 -I"$SRC" -I"$LIBXML2/include" -c "$SRC/xmq.c" -o "$XMQ_OBJ"
fi

# ── 3. Core + Driver ─────────────────────────────────────────────────────────
echo "==> Compiling src/ (only changed files)..."

CORE_SRCS="
  $SRC/dvparser.cc  $SRC/wmbus.cc  $SRC/wmbus_utils.cc  $SRC/wmbus_simulator.cc
  $SRC/address.cc  $SRC/units.cc  $SRC/translatebits.cc  $SRC/formula.cc
  $SRC/meters.cc  $SRC/metermanager.cc  $SRC/printer.cc  $SRC/drivers.cc
  $SRC/manufacturer_specificities.cc  $SRC/util.cc  $SRC/config.cc  $SRC/cmdline.cc
  $SRC/aes.cc  $SRC/aescmac.cc  $SRC/sha256.cc
  $SRC/wmbus_amb8465.cc  $SRC/wmbus_im871a.cc  $SRC/wmbus_iu891a.cc
  $SRC/wmbus_cul.cc  $SRC/wmbus_rc1180.cc  $SRC/wmbus_rawtty.cc
"
DRIVER_SRCS=$(ls "$SRC"/driver_*.cc)

ALL_OBJS="$XMQ_OBJ"
for src in $CORE_SRCS $DRIVER_SRCS; do
    [ -z "$src" ] && continue
    ALL_OBJS="$ALL_OBJS $(compile_if_needed $src)"
done

# ── 4. Stubs ──────────────────────────────────────────────────────────────────
STUBS_OBJ="$OBJ/wasm_stubs.o"
if [ ! -f "$STUBS_OBJ" ] || [ "$SCRIPT_DIR/wasm_stubs.cc" -nt "$STUBS_OBJ" ]; then
    echo "  [compile] wasm_stubs.cc" >&2
    em++ $CXX_FLAGS -c "$SCRIPT_DIR/wasm_stubs.cc" -o "$STUBS_OBJ"
fi
ALL_OBJS="$ALL_OBJS $STUBS_OBJ"

# ── 4b. serial_web ───────────────────────────────────────────────────────────
SERIAL_OBJ="$OBJ/serial_web.o"
if [ ! -f "$SERIAL_OBJ" ] || [ "$SCRIPT_DIR/serial_web.cc" -nt "$SERIAL_OBJ" ]; then
    echo "  [compile] serial_web.cc" >&2
    em++ $CXX_FLAGS -c "$SCRIPT_DIR/serial_web.cc" -o "$SERIAL_OBJ"
fi
ALL_OBJS="$ALL_OBJS $SERIAL_OBJ"

# ── 5. bindings.cpp (immer neu) ──────────────────────────────────────────────
BINDINGS_OBJ="$OBJ/bindings.o"
echo "  [compile] bindings.cpp" >&2
em++ $CXX_FLAGS -c "$SCRIPT_DIR/bindings.cpp" -o "$BINDINGS_OBJ"
ALL_OBJS="$ALL_OBJS $BINDINGS_OBJ"

# ── 6. Linken ─────────────────────────────────────────────────────────────────
echo "==> Linking wmbusmeters WASM..."

em++ \
  $ALL_OBJS \
  "$LIBXML2_A" \
  -s WASM=1 \
  -s MODULARIZE=1 \
  -s ASSERTIONS=1 \
  -s SAFE_HEAP=1 \
  -s ASYNCIFY \
  -s ASYNCIFY_STACK_SIZE=1048576 \
  -s ASYNCIFY_IMPORTS='["emscripten_sleep","usleep"]' \
  -s EXPORT_NAME="WMBusMeters" \
  -s EXPORTED_RUNTIME_METHODS='["ccall","cwrap","UTF8ToString","stringToUTF8","lengthBytesUTF8","allocate","ALLOC_NORMAL","HEAPU8"]' \
  -s ALLOW_MEMORY_GROWTH=1 \
  -s EXPORTED_FUNCTIONS='[
    "_wm_version",
    "_wm_free_result",
    "_wm_list_drivers",
    "_wm_decode",
    "_wm_serial_list",
    "_wm_serial_open",
    "_wm_serial_close",
    "_wm_configure_protocol",
    "_wm_send_command",
    "_wm_process_buffer",
    "_wm_detect_device_at_baud",
    "_serial_on_data",
    "_malloc",
    "_free"
  ]' \
  -o "$OUT/wmbusmeters.js"

echo ""

echo "==> Optimizing with wasm-opt..."
wasm-opt "$OUT/wmbusmeters.wasm" $WASM_OPT_FLAGS -o "$OUT/wmbusmeters.wasm" 
echo "==> Done! Output in wasm/dist/"