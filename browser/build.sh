#!/bin/bash
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
SRC="$SCRIPT_DIR/../src"
OUT="$SCRIPT_DIR/dist"
BUILD="$SCRIPT_DIR/build"
LIBXML2="$SCRIPT_DIR/../3rdparty/libxml2"
LIBXML2_A="$LIBXML2/.libs/libxml2.a"
WASM_OPT_FLAGS="-O4 --converge --strip-debug --strip-dwarf --strip-producers"
VERSION="$(git describe --tags --dirty --always)-wasm"
COMMIT="$(git rev-parse --short HEAD)"

mkdir -p "$OUT" "$BUILD"

echo "#define VERSION \"$VERSION\"" > $BUILD/version.h
echo "#define COMMIT \"$COMMIT\"" >> $BUILD/version.h

CXX_FLAGS="-std=c++20 -Wall -Wextra -O2 -flto -I$SRC -I$BUILD -I$SCRIPT_DIR -I$LIBXML2/include -D__EMSCRIPTEN_BUILD__ -Dexit=wm_exit_trap -include ./usleep_async.h -include $BUILD/version.h"

compile_if_needed() {
    local src="$1"
    local extra_flags="${2:-}"
    local obj="$BUILD/$(basename ${src%.*}).o"
    if [ ! -f "$obj" ] || [ "$src" -nt "$obj" ]; then
        echo "  [compile] $(basename $src)" >&2
        em++ $CXX_FLAGS $extra_flags -c "$src" -o "$obj"
    fi
    echo "$obj"
}

# ── 1. libxml2 ────────────────────────────────────────
if [ ! -f "$LIBXML2_A" ]; then
  echo "==> Building libxml2 for WASM..."

  if [ ! -d "$LIBXML2" ] || [ -z "$(ls -A "$LIBXML2" 2>/dev/null)" ]; then
    echo "INFO: Missing libxml2 sources in $LIBXML2"
    echo "INFO: Cloning from https://gitlab.gnome.org/GNOME/libxml2.git"
    mkdir -p "$SCRIPT_DIR/../3rdparty"
    git clone https://gitlab.gnome.org/GNOME/libxml2.git "$LIBXML2"
  fi

  cd "$LIBXML2"
  if [ ! -f "./configure" ]; then
    if [ -x "./autogen.sh" ]; then
      ./autogen.sh
    elif [ -x "./autogen" ]; then
      ./autogen
    else
      echo "ERROR: Neither ./configure nor ./autogen(.sh) found in $LIBXML2"
      exit 1
    fi
  fi
  emconfigure ./configure --enable-static --disable-shared --without-http --without-ftp \
    --without-modules --without-python --without-zlib --without-lzma --without-threads \
    --without-iconv --without-icu --host=wasm32-unknown-emscripten
  emmake make -j$(nproc)
  cd "$SCRIPT_DIR"
else
  echo "==> libxml2 already built, skipping"
fi

# ── 2. xmq.c ─────────────────────────────────────────
XMQ_OBJ="$BUILD/xmq.o"
if [ ! -f "$XMQ_OBJ" ] || [ "$SRC/xmq.c" -nt "$XMQ_OBJ" ]; then
    echo "  [compile] xmq.c" >&2
    emcc -O2 -flto  -I"$SRC" -I"$LIBXML2/include" -c "$SRC/xmq.c" -o "$XMQ_OBJ"
fi

# ── 3. Core + Drivers + wmbus devices + bus ───────────
echo "==> Compiling src/ (only changed files)..."

CORE_SRCS="
  $SRC/dvparser.cc  $SRC/wmbus.cc  $SRC/wmbus_utils.cc  $SRC/wmbus_simulator.cc
  $SRC/address.cc  $SRC/units.cc  $SRC/translatebits.cc  $SRC/formula.cc
  $SRC/meters.cc  $SRC/metermanager.cc  $SRC/printer.cc  $SRC/drivers.cc
  $SRC/manufacturer_specificities.cc  $SRC/util.cc  $SRC/config.cc  $SRC/cmdline.cc
  $SRC/aes.cc  $SRC/aescmac.cc  $SRC/sha256.cc
  $SRC/wmbus_amb8465.cc  $SRC/wmbus_im871a.cc  $SRC/wmbus_iu891a.cc
  $SRC/wmbus_cul.cc  $SRC/wmbus_rc1180.cc  $SRC/wmbus_rawtty.cc
  $SRC/bus.cc
"
DRIVER_SRCS=$(ls "$SRC"/driver_*.cc)

ALL_OBJS="$XMQ_OBJ"
for src in $CORE_SRCS $DRIVER_SRCS; do
    [ -z "$src" ] && continue
    ALL_OBJS="$ALL_OBJS $(compile_if_needed $src)"
done

# ── 4. main.cc ────────────────────────────────────────
MAIN_OBJ="$BUILD/main.o"
if [ ! -f "$MAIN_OBJ" ] || [ "$SRC/main.cc" -nt "$MAIN_OBJ" ]; then
    echo "  [compile] main.cc (main→_wm_original_main)" >&2
    em++ $CXX_FLAGS -Dmain=_wm_original_main -c "$SRC/main.cc" -o "$MAIN_OBJ"
fi
ALL_OBJS="$ALL_OBJS $MAIN_OBJ"

# ── 5. Stubs ──────────────────────────────────────────
STUBS_OBJ="$BUILD/wasm_stubs.o"
if [ ! -f "$STUBS_OBJ" ] || [ "$SCRIPT_DIR/wasm_stubs.cc" -nt "$STUBS_OBJ" ]; then
    echo "  [compile] wasm_stubs.cc" >&2
    em++ $CXX_FLAGS -c "$SCRIPT_DIR/wasm_stubs.cc" -o "$STUBS_OBJ"
fi
ALL_OBJS="$ALL_OBJS $STUBS_OBJ"

# ── 6. serial_web ────────────────────────────────────
SERIAL_OBJ="$BUILD/serial_web.o"
if [ ! -f "$SERIAL_OBJ" ] || [ "$SCRIPT_DIR/serial_web.cc" -nt "$SERIAL_OBJ" ]; then
    echo "  [compile] serial_web.cc" >&2
    em++ $CXX_FLAGS -c "$SCRIPT_DIR/serial_web.cc" -o "$SERIAL_OBJ"
fi
ALL_OBJS="$ALL_OBJS $SERIAL_OBJ"

# ── 7. bindings.cc ──────────────────────
BINDINGS_OBJ="$BUILD/bindings.o"
echo "  [compile] bindings.cc" >&2
em++ $CXX_FLAGS -c "$SCRIPT_DIR/bindings.cc" -o "$BINDINGS_OBJ"
ALL_OBJS="$ALL_OBJS $BINDINGS_OBJ"

# ── 8. Link ─────────────────────────────────────────
echo "==> Linking wmbusmeters WASM..."

em++ \
  $ALL_OBJS \
  "$LIBXML2_A" \
  -flto \
  -lidbfs.js \
  -s FORCE_FILESYSTEM=1 \
  -s WASM=1 \
  -s MODULARIZE=1 \
  -s ASSERTIONS=1 \
  -s ASYNCIFY \
  -s ASYNCIFY_STACK_SIZE=1048576 \
  -s ASYNCIFY_IMPORTS='["emscripten_sleep", "usleep"]' \
  -s EXPORT_NAME="WMBusMeters" \
  -s EXPORTED_RUNTIME_METHODS='["ccall","cwrap","UTF8ToString","stringToUTF8","lengthBytesUTF8","HEAPU8"]' \
  -s ALLOW_MEMORY_GROWTH=1 \
  -s NO_EXIT_RUNTIME=1 \
  -s EXPORTED_FUNCTIONS='[
    "FS",
    "_wm_version",
    "_wm_main",
    "_wm_stop",
    "_wm_serial_open",
    "_wm_serial_close",
    "_serial_on_data",
    "_malloc",
    "_free"
  ]' \
  -o "$OUT/wmbusmeters.js"

echo ""

# echo "==> Optimizing with wasm-opt..."
# wasm-opt "$OUT/wmbusmeters.wasm" $WASM_OPT_FLAGS -o "$OUT/wmbusmeters.wasm" 
echo "==> Done! Output in wasm/dist/"