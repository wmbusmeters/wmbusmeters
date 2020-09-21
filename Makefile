# Copyright (C) 2017-2020 Fredrik Öhrström
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

DESTDIR?=/

ifeq "$(HOST)" "arm"
    CXX?=arm-linux-gnueabihf-g++
    STRIP?=arm-linux-gnueabihf-strip
    BUILD=build_arm
    DEBARCH=armhf
else
    CXX?=g++
    STRIP?=strip
#--strip-unneeded --remove-section=.comment --remove-section=.note
    BUILD=build
    DEBARCH=amd64
endif

ifeq "$(DEBUG)" "true"
    DEBUG_FLAGS=-O0 -ggdb -fsanitize=address -fno-omit-frame-pointer -fprofile-arcs -ftest-coverage
    STRIP_BINARY=
    BUILD:=$(BUILD)_debug
    ifneq '' '$(findstring clang++,$(CXX))'
        DEBUG_LDFLAGS=-fsanitize=address --coverage
        GCOV?=llvm-cov gcov
    else
        DEBUG_LDFLAGS=-lasan -lgcov --coverage
        GCOV?=gcov
    endif
else
    DEBUG_FLAGS=-Os
    STRIP_BINARY=$(STRIP) $(BUILD)/wmbusmeters
    GCOV=To_run_gcov_add_DEBUG=true
endif

$(shell mkdir -p $(BUILD))

COMMIT_HASH?=$(shell git log --pretty=format:'%H' -n 1)
TAG?=$(shell git describe --tags)
BRANCH?=$(shell git rev-parse --abbrev-ref HEAD)
CHANGES?=$(shell git status -s | grep -v '?? ')
TAG_COMMIT_HASH?=$(shell git show-ref --tags | grep $(TAG) | cut -f 1 -d ' ')

ifeq ($(BRANCH),master)
  BRANCH:=
else
  BRANCH:=$(BRANCH)_
endif

ifeq ($(COMMIT),$(TAG_COMMIT))
  # Exactly on the tagged commit. The version is the tag!
  VERSION:=$(BRANCH)$(TAG)
  DEBVERSION:=$(BRANCH)$(TAG)
else
  VERSION:=$(BRANCH)$(TAG)++
  DEBVERSION:=$(BRANCH)$(TAG)++
endif

ifneq ($(strip $(CHANGES)),)
  # There are changes, signify that with a +changes
  VERSION:=$(VERSION) with local changes
  COMMIT_HASH:=$(COMMIT_HASH) with local changes
  DEBVERSION:=$(DEBVERSION)l
endif

$(shell echo "#define VERSION \"$(VERSION)\"" > $(BUILD)/version.h.tmp)
$(shell echo "#define COMMIT \"$(COMMIT_HASH)\"" >> $(BUILD)/version.h.tmp)

PREV_VERSION=$(shell cat -n $(BUILD)/version.h 2> /dev/null)
CURR_VERSION=$(shell cat -n $(BUILD)/version.h.tmp 2>/dev/null)
ifneq ($(PREV_VERSION),$(CURR_VERSION))
$(shell mv $(BUILD)/version.h.tmp $(BUILD)/version.h)
else
$(shell rm $(BUILD)/version.h.tmp)
endif

$(info Building $(VERSION))

CXXFLAGS ?= $(DEBUG_FLAGS) -fPIC -std=c++11 -Wall -Werror=format-security
CXXFLAGS += -I$(BUILD)

LDFLAGS  ?= $(DEBUG_LDFLAGS)

$(BUILD)/%.o: src/%.cc $(wildcard src/%.h)
	$(CXX) $(CXXFLAGS) $< -c -E > $@.src
	$(CXX) $(CXXFLAGS) $< -MMD -c -o $@

METER_OBJS:=\
	$(BUILD)/aes.o \
	$(BUILD)/aescmac.o \
	$(BUILD)/cmdline.o \
	$(BUILD)/config.o \
	$(BUILD)/dvparser.o \
	$(BUILD)/meters.o \
	$(BUILD)/meter_amiplus.o \
	$(BUILD)/meter_apator08.o \
	$(BUILD)/meter_apator162.o \
	$(BUILD)/meter_cma12w.o \
	$(BUILD)/meter_compact5.o \
	$(BUILD)/meter_ebzwmbe.o \
	$(BUILD)/meter_ehzp.o \
	$(BUILD)/meter_esyswm.o \
	$(BUILD)/meter_eurisii.o \
	$(BUILD)/meter_fhkvdataiii.o \
	$(BUILD)/meter_hydrus.o \
	$(BUILD)/meter_hydrodigit.o \
	$(BUILD)/meter_iperl.o \
	$(BUILD)/meter_izar.o \
	$(BUILD)/meter_izar3.o \
	$(BUILD)/meter_lansendw.o \
	$(BUILD)/meter_lansensm.o \
	$(BUILD)/meter_lansenth.o \
	$(BUILD)/meter_lansenpu.o \
	$(BUILD)/meter_mkradio3.o \
	$(BUILD)/meter_multical21.o \
	$(BUILD)/meter_multical302.o \
	$(BUILD)/meter_multical403.o \
	$(BUILD)/meter_omnipower.o \
	$(BUILD)/meter_q400.o \
	$(BUILD)/meter_qcaloric.o \
	$(BUILD)/meter_rfmamb.o \
	$(BUILD)/meter_rfmtx1.o \
	$(BUILD)/meter_supercom587.o \
	$(BUILD)/meter_topaseskr.o \
	$(BUILD)/meter_vario451.o \
	$(BUILD)/meter_waterstarm.o \
	$(BUILD)/printer.o \
	$(BUILD)/serial.o \
	$(BUILD)/shell.o \
	$(BUILD)/threads.o \
	$(BUILD)/util.o \
	$(BUILD)/units.o \
	$(BUILD)/wmbus.o \
	$(BUILD)/wmbus_amb8465.o \
	$(BUILD)/wmbus_im871a.o \
	$(BUILD)/wmbus_cul.o \
	$(BUILD)/wmbus_d1tc.o \
	$(BUILD)/wmbus_rtlwmbus.o \
	$(BUILD)/wmbus_rtl433.o \
	$(BUILD)/wmbus_simulator.o \
	$(BUILD)/wmbus_rawtty.o \
	$(BUILD)/wmbus_wmb13u.o \
	$(BUILD)/wmbus_utils.o

all: $(BUILD)/wmbusmeters $(BUILD)/wmbusmeters-admin $(BUILD)/testinternals
	@$(STRIP_BINARY)
	@cp $(BUILD)/wmbusmeters $(BUILD)/wmbusmetersd

dist: wmbusmeters_$(DEBVERSION)_$(DEBARCH).deb

install: $(BUILD)/wmbusmeters
	@./install.sh $(BUILD)/wmbusmeters $(DESTDIR) $(EXTRA_INSTALL_OPTIONS)

uninstall:
	@./uninstall.sh /

wmbusmeters_$(DEBVERSION)_$(DEBARCH).deb:
	@rm -rf $(BUILD)/debian/wmbusmeters
	@mkdir -p $(BUILD)/debian/wmbusmeters/DEBIAN
	@./install.sh $(BUILD)/wmbusmeters $(BUILD)/debian/wmbusmeters --no-adduser
	@rm -f $(BUILD)/debian/wmbusmeters/DEBIAN/control
	@echo "Package: wmbusmeters" >> $(BUILD)/debian/wmbusmeters/DEBIAN/control
	@echo "Version: $(DEBVERSION)" >> $(BUILD)/debian/wmbusmeters/DEBIAN/control
	@echo "Maintainer: Fredrik Öhrström" >> $(BUILD)/debian/wmbusmeters/DEBIAN/control
	@echo "Architecture: $(DEBARCH)" >> $(BUILD)/debian/wmbusmeters/DEBIAN/control
	@echo "Description: A tool to read wireless mbus telegrams from utility meters." >> $(BUILD)/debian/wmbusmeters/DEBIAN/control
	@(cd $(BUILD)/debian; dpkg-deb --build wmbusmeters .)
	@mv $(BUILD)/debian/wmbusmeters_$(DEBVERSION)_$(DEBARCH).deb .
	@echo Built package $@
	@echo But the deb package is not yet working correctly! Work in progress.

snapcraft:
	snapcraft --config snap/snapcraft.yaml

$(BUILD)/main.o: $(BUILD)/short_manual.h $(BUILD)/version.h

$(BUILD)/wmbusmeters: $(METER_OBJS) $(BUILD)/main.o $(BUILD)/short_manual.h
	$(CXX) -o $(BUILD)/wmbusmeters $(METER_OBJS) $(BUILD)/main.o $(LDFLAGS) -lpthread

$(BUILD)/wmbusmeters-admin: $(METER_OBJS) $(BUILD)/admin.o $(BUILD)/ui.o $(BUILD)/short_manual.h
	$(CXX) -o $(BUILD)/wmbusmeters-admin $(METER_OBJS) $(BUILD)/admin.o $(BUILD)/ui.o $(LDFLAGS) -lmenu -lform -lncurses -lpthread

$(BUILD)/short_manual.h: README.md
	echo 'R"MANUAL(' > $(BUILD)/short_manual.h
	sed -n '/wmbusmeters version/,/```/p' README.md \
	| grep -v 'wmbusmeters version' \
	| grep -v '```' >> $(BUILD)/short_manual.h
	echo ')MANUAL";' >> $(BUILD)/short_manual.h

$(BUILD)/testinternals: $(METER_OBJS) $(BUILD)/testinternals.o
	$(CXX) -o $(BUILD)/testinternals $(METER_OBJS) $(BUILD)/testinternals.o $(LDFLAGS) -lpthread

$(BUILD)/fuzz: $(METER_OBJS) $(BUILD)/fuzz.o
	$(CXX) -o $(BUILD)/fuzz $(METER_OBJS) $(BUILD)/fuzz.o $(LDFLAGS) -lpthread

clean:
	rm -rf build/* build_arm/* build_debug/* build_arm_debug/* *~

clean_cc:
	find . -name "*.gcov" -delete
	find . -name "*.gcda" -delete

gcov:
	$(GCOV) -o build_debug $(METER_OBJS)
	mv *.gcov build_debug

test:
	@./test.sh build/wmbusmeters

testd:
	@./test.sh build_debug/wmbusmeters

update_manufacturers:
	iconv -f utf-8 -t ascii//TRANSLIT -c DLMS_Flagids.csv -o tmp.flags
	cat tmp.flags | grep -v ^# | cut -f 1 > list.flags
	cat tmp.flags | grep -v ^# | cut -f 2 > names.flags
	cat tmp.flags | grep -v ^# | cut -f 3 > countries.flags
	cat countries.flags | sort -u | grep -v '^$$' > uniquec.flags
	cat names.flags | tr -d "'" | tr -c 'a-zA-Z0-9\n' ' ' | tr -s ' ' | sed 's/^ //g' | sed 's/ $$//g' > ansi.flags
	cat ansi.flags | sed 's/\(^.......[^0123456789]*\)[0123456789]\+.*/\1/g' > cleaned.flags
	cat cleaned.flags | sed -e "$$(sed 's:.*:s/&//Ig:' uniquec.flags)" > cleanedc.flags
	cat cleanedc.flags | sed \
	-e 's/ ab\( \|$$\)/ /Ig' \
	-e 's/ ag\( \|$$\)/ /Ig' \
	-e 's/ a \?s\( \|$$\)/ /Ig' \
	-e 's/ co\( \|$$\)/ /Ig' \
	-e 's/ b \?v\( \|$$\)/ /Ig' \
	-e 's/ bvba\( \|$$\)/ /Ig' \
	-e 's/ corp\( \|$$\)/ /Ig' \
	-e 's/ d \?o \?o\( \|$$\)/ /g' \
	-e 's/ d \?d\( \|$$\)/ /g' \
	-e 's/ gmbh//Ig' \
	-e 's/ gbr//Ig' \
	-e 's/ inc\( \|$$\)/ /Ig' \
	-e 's/ kg\( \|$$\)/ /Ig' \
	-e 's/ llc/ /Ig' \
	-e 's/ ltd//Ig' \
	-e 's/ limited//Ig' \
	-e 's/ nv\( \|$$\)/ /Ig' \
	-e 's/ oy//Ig' \
	-e 's/ ood\( \|$$\)/ /Ig' \
	-e 's/ooo\( \|$$\)/ /Ig' \
	-e 's/ pvt\( \|$$\)/ /Ig' \
	-e 's/ pte\( \|$$\)/ /Ig' \
	-e 's/ pty\( \|$$\)/ /Ig' \
	-e 's/ plc\( \|$$\)/ /Ig' \
	-e 's/ private\( \|$$\)/ /Ig' \
	-e 's/ s \?a\( \|$$\)/ /Ig' \
	-e 's/ sarl\( \|$$\)/ /Ig' \
	-e 's/ sagl\( \|$$\)/ /Ig' \
	-e 's/ s c ul//Ig' \
	-e 's/ s \?l\( \|$$\)/ /Ig' \
	-e 's/ s \?p \?a\( \|$$\)/ /Ig' \
	-e 's/ sp j\( \|$$\)/ /Ig' \
	-e 's/ sp z o o//Ig' \
	-e 's/ s r o//Ig' \
	-e 's/ s \?r \?l//Ig' \
	-e 's/ ug\( \|$$\)/ /Ig' \
	> trimmed.flags
	cat trimmed.flags | tr -s ' ' | sed 's/^ //g' | sed 's/ $$//g' > done.flags
	paste -d '|,' list.flags done.flags countries.flags | sed 's/,/, /g' | sed 's/ |/|/g' > manufacturers.txt
	echo '#ifndef MANUFACTURERS_H' > m.h
	echo '#define MANUFACTURERS_H' >> m.h
	echo '#define MANFCODE(a,b,c) ((a-64)*1024+(b-64)*32+(c-64))' >> m.h
	echo "#define LIST_OF_MANUFACTURERS \\" >> m.h
	cat manufacturers.txt | sed -e "s/\(.\)\(.\)\(.\).\(.*\)/X(\1\2\3,MANFCODE('\1','\2','\3'),\"\4\")\\\\/g" | sed 's/, ")/")/' >> m.h
	echo >> m.h
	cat manufacturers.txt | sed -e "s/\(.\)\(.\)\(.\).*/#define MANUFACTURER_\1\2\3 MANFCODE('\1','\2','\3')/g" >> m.h
	echo >> m.h
	echo '#endif' >> m.h
	mv m.h src/manufacturers.h
	rm *.flags manufacturers.txt

build_fuzz:
	@if [ "${AFLHOME}" = "" ]; then echo 'You must supply aflhome "make build_fuzz AFLHOME=/home/afl"'; exit 1; fi
	$(MAKE) AFL_HARDEN=1 CXX=$(AFLHOME)/afl-g++ $(BUILD)/fuzz
	$(MAKE) AFL_HARDEN=1 CXX=$(AFLHOME)/afl-g++ $(BUILD)/wmbusmeters

run_fuzz_difvifparser:
	@if [ "${AFLHOME}" = "" ]; then echo 'You must supply aflhome "make run_fuzz AFLHOME=/home/afl"'; exit 1; fi
	${AFLHOME}/afl-fuzz -i fuzz_testcases/difvifparser -o fuzz_findings/ build/fuzz

run_fuzz_telegrams:
	@if [ "${AFLHOME}" = "" ]; then echo 'You must supply aflhome "make run_fuzz AFLHOME=/home/afl"'; exit 1; fi
	${AFLHOME}/afl-fuzz -i fuzz_testcases/telegrams -o fuzz_findings/ build/wmbusmeters --listento=any stdin

# Include dependency information generated by gcc in a previous compile.
include $(wildcard $(patsubst %.o,%.d,$(METER_OBJS)))
