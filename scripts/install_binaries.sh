# Copyright (C) 2021 Fredrik Öhrström (gpl-3.0-or-later)

rm -f "$ROOT"/usr/bin/wmbusmeters "$ROOT"/usr/sbin/wmbusmetersd
mkdir -p "$ROOT"/usr/bin
mkdir -p "$ROOT"/usr/sbin
cp "$SRC" "$ROOT"/usr/bin/wmbusmeters
(cd "$ROOT"/usr/sbin; ln -s ../bin/wmbusmeters wmbusmetersd)

echo "binaries: installed $ROOT/usr/bin/wmbusmeters and $ROOT/usr/sbin/wmbusmetersd"
