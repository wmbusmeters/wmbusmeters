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
#   Remove the conf and log files, they are to be installed by postinst.
rm $BUILD/debian/wmbusmeters/etc/wmbusmeters.conf
rm -rf $BUILD/debian/wmbusmeters/etc/wmbusmetersd
rm -rf $BUILD/debian/wmbusmeters/var/log/wmbusmeters
#   Build the control file
cat >> $BUILD/debian/wmbusmeters/DEBIAN/control << EOF
Package: wmbusmeters
Version: $DEBVERSION
Maintainer: Fredrik Öhrström <oehrstroem@gmail.com>
Architecture: $DEBARCH
Installed-Size: $ISIZE
Depends: libc6 (>= 2.27)
Section: kernel
Priority: optional
Homepage: https://github.com/weetmuts/wmbusmeters
Description: read wireless and wired mbus telegrams from utility meters
 Wmbusmeters receives and decodes C1,T1 or S1 telegrams (using
 the wireless or wired mbus protocol) to acquire utility meter
 readings. The readings can then be published using MQTT,
 curled to a REST api, inserted into a database or stored in a log file.
 .
 Installing this package results in a full installation, including the
 configuration files and the daemon. Configuration files for an existing
 installation are preserved.
EOF
cat >> $BUILD/debian/wmbusmeters/DEBIAN/conffiles << EOF
/etc/logrotate.d/wmbusmeters
EOF
mkdir -p $BUILD/debian/wmbusmeters/usr/share/doc/wmbusmeters
#   Install the changelog
cp    deb/changelog $BUILD/debian/wmbusmeters/usr/share/doc/wmbusmeters/changelog.Debian
gzip -v9 -n $BUILD/debian/wmbusmeters/usr/share/doc/wmbusmeters/changelog.Debian
#   Collect copyright information
./deb/collect_copyrights.sh
cp /tmp/copyright $BUILD/debian/wmbusmeters/usr/share/doc/wmbusmeters/copyright
#   Install the install/remove scripts.
for x in preinst postinst prerm postrm ; do \
	cp deb/$x $BUILD/debian/wmbusmeters/DEBIAN/ ; \
	chmod 555 $BUILD/debian/wmbusmeters/DEBIAN/$x ; \
    done
# Change owner to root
chown -R root:root $BUILD/debian/wmbusmeters
#   Package the deb
(cd $BUILD/debian; dpkg-deb --build wmbusmeters .)
mv $BUILD/debian/wmbusmeters_${DEBVERSION}_${DEBARCH}.deb .
echo Built package $
echo But the deb package is not yet working correctly! Work in progress.
