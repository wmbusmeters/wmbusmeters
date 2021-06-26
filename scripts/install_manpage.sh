####################################################################
##
## Intall wmbusmeters manual page
##

rm -f "$ROOT"/usr/share/man/man1/wmbusmeters.1.gz
mkdir -p "$ROOT"/usr/share/man/man1
gzip -v9 -c wmbusmeters.1 > "$ROOT"/usr/share/man/man1/wmbusmeters.1.gz

echo "man page: installed $ROOT/usr/share/man/man1/wmbusmeters.1.gz"
