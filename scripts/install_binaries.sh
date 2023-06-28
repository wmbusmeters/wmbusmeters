# Copyright (C) 2021-2023 Fredrik Öhrström (gpl-3.0-or-later)

# Command binary /usr/bin/wmbusmeters
wmbusmeters_dir="${ROOT}/usr/bin"
wmbusmeters_path="${wmbusmeters_dir}/wmbusmeters"

# Daemon /usr/sbin/wmbusmetersd which is a symlink to the command binary.
wmbusmetersd_dir="${ROOT}/usr/sbin"
wmbusmetersd_path="${wmbusmetersd_dir}/wmbusmetersd"

# Remove any existing installed components.
rm -f "$wmbusmeters_path" "$wmbusmetersd_path" || exit $?

# Install the command binary and create the bin directory if necessary.
install -D -m 755 "$SRC" "$wmbusmeters_path" || exit $?

# Create the sbin directory if necessary.
mkdir -p "$wmbusmetersd_dir" || exit $?

# Calculate the relative symlink from sbin to bin.
wmbusmetersd_target="$(realpath -s --relative-to="${wmbusmetersd_dir}" "${wmbusmeters_path}")"

# Create the actual link.
ln -s "$wmbusmetersd_target" "$wmbusmetersd_path" || exit $?

echo "binaries: installed ${wmbusmeters_path} ${wmbusmetersd_path}"
