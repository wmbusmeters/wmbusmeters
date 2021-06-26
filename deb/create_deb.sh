#!/bin/sh

BUILD=$1
DEB=$2
DEBVERSION=$3
DEBARCH=$4

if [ -z "$BUILD" ]
then
    echo You must supply a directory for the deb contents.
    exit 1
fi

if [ -z "$DEB" ]
then
    echo you must supply a name for the deb file.
    exit 1
fi

if [ -z "$DEBVERSION" ]
then
    echo you must supply deb version.
    exit 1
fi

if [ -z "$DEBARCH" ]
then
    echo you must supply a deb arch.
    exit 1
fi

rm -rf $BUILD/debian/wmbusmeters
mkdir -p $BUILD/debian/wmbusmeters/DEBIAN
./install.sh $BUILD/wmbusmeters $BUILD/debian/wmbusmeters --no-adduser
#   Remove the conf files, they are to be installed by postinst.
rm $BUILD/debian/wmbusmeters/etc/wmbusmeters.conf
rm -rf $BUILD/debian/wmbusmeters/etc/wmbusmetersd
#   Build the control file
echo "Package: wmbusmeters" >> $BUILD/debian/wmbusmeters/DEBIAN/control
echo "Version: $DEBVERSION" >> $BUILD/debian/wmbusmeters/DEBIAN/control
echo "Maintainer: Fredrik Öhrström <oehrstroemgmail.com>" >> $BUILD/debian/wmbusmeters/DEBIAN/control
echo "Architecture: $DEBARCH" >> $BUILD/debian/wmbusmeters/DEBIAN/control
echo "Installed-Size: 1" >> $BUILD/debian/wmbusmeters/DEBIAN/control
echo "Depends: libc6 (>= 2.27)" >> $BUILD/debian/wmbusmeters/DEBIAN/control
echo "Section: kernel" >> $BUILD/debian/wmbusmeters/DEBIAN/control
echo "Priority: optional" >> $BUILD/debian/wmbusmeters/DEBIAN/control
echo "Homepage: https://github.com/weetmuts/wmbusmeters" >> $BUILD/debian/wmbusmeters/DEBIAN/control
echo "Description: A tool to read wireless mbus telegrams from utility meters." >> $BUILD/debian/wmbusmeters/DEBIAN/control
mkdir -p $BUILD/debian/wmbusmeters/usr/share/doc/wmbusmeters
#   Install the changelog
cp    deb/changelog $BUILD/debian/wmbusmeters/usr/share/doc/wmbusmeters/changelog.Debian
gzip -v9 $BUILD/debian/wmbusmeters/usr/share/doc/wmbusmeters/changelog.Debian
#   Collect copyright information
./deb/collect_copyrights.sh
cp /tmp/copyright $BUILD/debian/wmbusmeters/usr/share/doc/wmbusmeters/copyright
#   Install the install/remove scripts.
for x in preinst postinst prerm postrm ; do \
	cp deb/$x $BUILD/debian/wmbusmeters/DEBIAN/ ; \
	chmod 555 $BUILD/debian/wmbusmeters/DEBIAN/$x ; \
done
#   Package the deb
(cd $BUILD/debian; dpkg-deb --build wmbusmeters .)
mv $BUILD/debian/wmbusmeters_${DEBVERSION}_${DEBARCH}.deb .
echo Built package $
echo But the deb package is not yet working correctly! Work in progress.
