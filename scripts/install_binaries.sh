# Copyright (C) 2021-2023 Fredrik Öhrström (gpl-3.0-or-later)

wmbusmeters_dir="$ROOT"/usr/bin
wmbusmeters_path="$wmbusmeters_dir"/wmbusmeters
wmbusmetersd_dir="$ROOT"/usr/sbin
wmbusmetersd_path="$wmbusmetersd_dir"/wmbusmetersd
wmbusmetersd_target=$(realpath -s --relative-to="$wmbusmetersd_dir" "$wmbusmeters_path")

rm -f "$wmbusmeters_path" "$wmbusmetersd_path" || exit $?⏎

install -D -m 755 "$SRC" "$wmbusmeters_path" || exit $?⏎

mkdir -p "$wmbusmetersd_dir" || exit $?

ln -s "$wmbusmetersd_target" "$wmbusmetersd_path" || exit $?

echo "binaries: installed '$wmbusmeters_path' '$wmbusmetersd_path'"
