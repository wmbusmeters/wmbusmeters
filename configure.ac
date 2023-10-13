# Copyright (C) 2020 Fredrik Öhrström
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.
#

AC_PREREQ([2.69])
AC_INIT(wmbusmeters, wmbusmeters, oehrstroem@gmail.com,,https://github.com/wmbusmeters/wmbusmeters)

AC_CONFIG_AUX_DIR([autoconf])
AC_CANONICAL_BUILD
AC_CANONICAL_HOST

m4_include([autoconf/pkg.m4])

PKG_PREREQ(0.29)

SRC_ROOT="$(pwd -L)"

CONF_NAME="${host_cpu}-${host_vendor}-${host_os}"
BUILD_ROOT="$SRC_ROOT/build"
OUTPUT_ROOT="$SRC_ROOT/build/${CONF_NAME}"

mkdir -p "$OUTPUT_ROOT"

AC_LANG_PUSH([C++])

case $host_os in
freebsd*)
	CFLAGS="$CFLAGS -I/usr/local/include"
	CXXFLAGS="$CXXFLAGS -I/usr/local/include"
	LDFLAGS="$LDFLAGS -L/usr/local/lib"
;;
esac

#C_CHECK_LIB(usb, usb_init, [],
#
#  C_MSG_ERROR([Could  not find libusb library. Try: sudo apt install libusb-dev])
#)

AC_CHECK_LIB(rtlsdr, rtlsdr_get_device_count, [],
[
  AC_MSG_ERROR([Could  not find rtlsdr library. Try: sudo apt install librtlsdr-dev])
])

AC_CHECK_LIB(xml2, xmlFreeDoc, [],
[
  AC_MSG_ERROR([Could  not find libxml2 library. Try: sudo apt install libxml2-dev])
])

AC_CONFIG_FILES([$OUTPUT_ROOT/spec.gmk:$SRC_ROOT/autoconf/spec.gmk.in])
AC_CONFIG_FILES([$OUTPUT_ROOT/Makefile:$SRC_ROOT/autoconf/Makefile.in])

# Make sure config.status ends up in the build directory instead of the src root.
CONFIG_STATUS="$OUTPUT_ROOT/config.status"
# Write out spec.gmk and build/Makefile
AC_OUTPUT
