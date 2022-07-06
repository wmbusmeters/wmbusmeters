# Copyright (C) 2021-2022 Fredrik Öhrström (gpl-3.0-or-later)

rm -f "$ROOT"/usr/bin/wmbusmeters "$ROOT"/usr/sbin/wmbusmetersd "$ROOT"/usr/bin/wmbusmeters-admin
mkdir -p "$ROOT"/usr/bin
mkdir -p "$ROOT"/usr/sbin
cp "$SRC" "$ROOT"/usr/bin/wmbusmeters
cp "${SRC}-admin" "$ROOT"/usr/bin/wmbusmeters-admin

(cd "$ROOT"/usr/sbin; ln -s ../bin/wmbusmeters wmbusmetersd)

echo "binaries: installed $ROOT/usr/bin/wmbusmeters $ROOT/usr/sbin/wmbusmetersd $ROOT/usr/bin/wmbusmeters-admin"
