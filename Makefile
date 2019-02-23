# Copyright (C) 2017-2019 Fredrik Öhrström
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

# To compile for Raspberry PI ARM:
# make HOST=arm
#
# To build with debug information:
# make DEBUG=true
# make DEBUG=true HOST=arm

VERSION=0.8.4

ifeq "$(HOST)" "arm"
    CXX=arm-linux-gnueabihf-g++
    STRIP=arm-linux-gnueabihf-strip
    BUILD=build_arm
	DEBARCH=armhf
else
    CXX=g++
    STRIP=strip --strip-unneeded --remove-section=.comment --remove-section=.note
    BUILD=build
	DEBARCH=amd64
endif

ifeq "$(DEBUG)" "true"
    DEBUG_FLAGS=-O0 -ggdb -fsanitize=address -fno-omit-frame-pointer
    STRIP_BINARY=
    BUILD:=$(BUILD)_debug
    DEBUG_LDFLAGS=-lasan
else
    DEBUG_FLAGS=-Os
    STRIP_BINARY=$(STRIP) $(BUILD)/wmbusmeters
endif

$(shell mkdir -p $(BUILD))

#EXTRAS=-Wno-maybe-uninitialized

CXXFLAGS := $(DEBUG_FLAGS) -fPIC -fmessage-length=0 -std=c++11 -Wall -Wno-unused-function "-DWMBUSMETERS_VERSION=\"$(VERSION)\""

$(BUILD)/%.o: src/%.cc $(wildcard src/%.h)
	$(CXX) $(CXXFLAGS) $< -c -o $@

METERS_OBJS:=\
	$(BUILD)/aes.o \
	$(BUILD)/cmdline.o \
	$(BUILD)/config.o \
	$(BUILD)/dvparser.o \
	$(BUILD)/meters.o \
	$(BUILD)/meter_multical21.o \
	$(BUILD)/meter_multical302.o \
	$(BUILD)/meter_omnipower.o \
	$(BUILD)/meter_supercom587.o \
	$(BUILD)/meter_iperl.o \
	$(BUILD)/meter_qcaloric.o \
	$(BUILD)/printer.o \
	$(BUILD)/serial.o \
	$(BUILD)/shell.o \
	$(BUILD)/util.o \
	$(BUILD)/wmbus.o \
	$(BUILD)/wmbus_amb8465.o \
	$(BUILD)/wmbus_im871a.o \
	$(BUILD)/wmbus_simulator.o \
	$(BUILD)/wmbus_utils.o

all: $(BUILD)/wmbusmeters $(BUILD)/testinternals
	@$(STRIP_BINARY)

dist: wmbusmeters_$(VERSION)_$(DEBARCH).deb

install: $(BUILD)/wmbusmeters
	@./install.sh $(BUILD)/wmbusmeters /

uninstall:
	@./uninstall.sh /

wmbusmeters_$(VERSION)_$(DEBARCH).deb:
	@rm -rf $(BUILD)/debian/wmbusmeters
	@mkdir -p $(BUILD)/debian/wmbusmeters/DEBIAN
	@./install.sh --no-adduser $(BUILD)/wmbusmeters $(BUILD)/debian/wmbusmeters
	@rm -f $(BUILD)/debian/wmbusmeters/DEBIAN/control
	@echo "Package: wmbusmeters" >> $(BUILD)/debian/wmbusmeters/DEBIAN/control
	@echo "Version: $(VERSION)" >> $(BUILD)/debian/wmbusmeters/DEBIAN/control
	@echo "Maintainer: Fredrik Öhrström" >> $(BUILD)/debian/wmbusmeters/DEBIAN/control
	@echo "Architecture: $(DEBARCH)" >> $(BUILD)/debian/wmbusmeters/DEBIAN/control
	@echo "Description: A tool to read wireless mbus telegrams from utility meters." >> $(BUILD)/debian/wmbusmeters/DEBIAN/control
	@(cd $(BUILD)/debian; dpkg-deb --build wmbusmeters .)
	@mv $(BUILD)/debian/wmbusmeters_$(VERSION)_$(DEBARCH).deb .
	@echo Built package $@

$(BUILD)/wmbusmeters: $(METERS_OBJS) $(BUILD)/main.o
	$(CXX) -o $(BUILD)/wmbusmeters $(METERS_OBJS) $(BUILD)/main.o $(DEBUG_LDFLAGS) -lpthread


$(BUILD)/testinternals: $(METERS_OBJS) $(BUILD)/testinternals.o
	$(CXX) -o $(BUILD)/testinternals $(METERS_OBJS) $(BUILD)/testinternals.o $(DEBUG_LDFLAGS) -lpthread

clean:
	rm -rf build/* build_arm/* build_debug/* build_arm_debug/* *~

test:
	./build/testinternals
	./test.sh build/wmbusmeters

update_manufacturers:
	wget http://www.m-bus.de/man.html
	echo '// Data downloaded from http://www.m-bus.de/man.html' > m.h
	echo -n '// ' >> m.h
	date --rfc-3339=date >> m.h
	echo >> m.h
	echo '#ifndef MANUFACTURERS_H' >> m.h
	echo '#define MANUFACTURERS_H' >> m.h
	echo '#define MANFCODE(a,b,c) ((a-64)*1024+(b-64)*32+(c-64))' >> m.h
	echo '#define LIST_OF_MANUFACTURERS \' >> m.h
	cat man.html | tr -d '\r\n' | sed \
	-e 's/.*<table>//' \
	-e 's/<\/table>.*//' \
	-e 's/<tr>/X(/g' \
	-e 's/<script[^<]*<\/script>//g' \
	-e 's/<a href=[^>]*>//g' \
	-e 's/<\/a>//g' \
	-e 's/<a name[^>]*>//g' \
	-e 's/<td>/\t/g' \
	-e 's/<\/td>//g' \
	-e 's/&auml;/ä/g' \
	-e 's/&uuml;/ü/g' \
	-e 's/&ouml;/ö/g' \
	-e 's/,/ /g' \
	-e 's/<\/tr>/)\\\n/g' | \
	grep -v '<caption>' | tr -s ' ' | tr -s '\t' | tr '\t' '|' > tmpfile
	echo 'X(|QDS|QUNDIS GmbH)\' >> tmpfile
	cat tmpfile | sed -e "s/X(|\(.\)\(.\)\(.\)/X(\1\2\3|MANFCODE('\1','\2','\3')|/g" | \
	tr -s '|' ',' >> m.h
	echo >> m.h
	cat tmpfile | sed -e "s/X(|\(.\)\(.\)\(.\).*/#define MANUFACTURER_\1\2\3 MANFCODE('\1','\2','\3')/g" \
	>> m.h
	echo >> m.h
	echo '#endif' >> m.h
	rm tmpfile
	mv m.h manufacturers.h
