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
    DEBUG_FLAGS=-Os -g
    STRIP_BINARY=cp $(BUILD)/wmbusmeters $(BUILD)/wmbusmeters.g; $(STRIP) $(BUILD)/wmbusmeters
    GCOV=To_run_gcov_add_DEBUG=true
endif

$(shell mkdir -p $(BUILD))

COMMIT_HASH?=$(shell git log --pretty=format:'%H' -n 1)
TAG?=$(shell git describe --tags)
BRANCH?=$(shell git rev-parse --abbrev-ref HEAD)
CHANGES?=$(shell git status -s | grep -v '?? ')

ifeq ($(BRANCH),master)
  BRANCH:=
else
  BRANCH:=$(BRANCH)_
endif

VERSION:=$(BRANCH)$(TAG)
DEBVERSION:=$(BRANCH)$(TAG)

ifneq ($(strip $(CHANGES)),)
  # There are local un-committed changes.
  VERSION:=$(VERSION) with local changes
  COMMIT_HASH:=$(COMMIT_HASH) with local changes
  DEBVERSION:=$(DEBVERSION)l
endif

$(shell echo "#define VERSION \"$(VERSION)\"" > $(BUILD)/version.h.tmp)
$(shell echo "#define COMMIT \"$(COMMIT_HASH)\"" >> $(BUILD)/version.h.tmp)

PREV_VERSION:=$(shell cat -n $(BUILD)/version.h 2> /dev/null)
CURR_VERSION:=$(shell cat -n $(BUILD)/version.h.tmp 2>/dev/null)
ifneq ($(PREV_VERSION),$(CURR_VERSION))
$(shell mv $(BUILD)/version.h.tmp $(BUILD)/version.h)
else
$(shell rm $(BUILD)/version.h.tmp)
endif

$(info Building $(VERSION))

FUZZFLAGS ?= -DFUZZING=false
CXXFLAGS ?= $(DEBUG_FLAGS) $(FUZZFLAGS) -fPIC -std=c++11 -Wall -Werror=format-security
CXXFLAGS += -I$(BUILD)
LDFLAGS  ?= $(DEBUG_LDFLAGS)

USBLIB = -lusb-1.0

ifeq ($(shell uname -s),FreeBSD)
    CXXFLAGS += -I/usr/local/include
    LDFLAGS  += -L/usr/local/lib
    USBLIB    =  -lusb
endif

$(BUILD)/%.o: src/%.cc $(wildcard src/%.h)
	$(CXX) $(CXXFLAGS) $< -c -E > $@.src
	$(CXX) $(CXXFLAGS) $< -MMD -c -o $@

PROG_OBJS:=\
	$(BUILD)/aes.o \
	$(BUILD)/aescmac.o \
	$(BUILD)/bus.o \
	$(BUILD)/cmdline.o \
	$(BUILD)/config.o \
	$(BUILD)/dvparser.o \
	$(BUILD)/mbus_rawtty.o \
	$(BUILD)/meters.o \
	$(BUILD)/manufacturer_specificities.o \
	$(BUILD)/printer.o \
	$(BUILD)/rtlsdr.o \
	$(BUILD)/serial.o \
	$(BUILD)/shell.o \
	$(BUILD)/sha256.o \
	$(BUILD)/threads.o \
	$(BUILD)/translatebits.o \
	$(BUILD)/util.o \
	$(BUILD)/units.o \
	$(BUILD)/wmbus.o \
	$(BUILD)/wmbus_amb8465.o \
	$(BUILD)/wmbus_im871a.o \
	$(BUILD)/wmbus_cul.o \
	$(BUILD)/wmbus_rtlwmbus.o \
	$(BUILD)/wmbus_rtl433.o \
	$(BUILD)/wmbus_simulator.o \
	$(BUILD)/wmbus_rawtty.o \
	$(BUILD)/wmbus_rc1180.o \
	$(BUILD)/wmbus_utils.o \

ifeq ($(DRIVER),)
	DRIVER_OBJS:=$(wildcard src/meter_*.cc)
else
    $(info Building a single driver $(DRIVER))
	DRIVER_OBJS:=src/meter_auto.cc src/meter_unknown.cc src/meter_$(DRIVER).cc
endif
DRIVER_OBJS:=$(patsubst src/%.cc,$(BUILD)/%.o,$(DRIVER_OBJS))

all: $(BUILD)/wmbusmeters $(BUILD)/wmbusmetersd $(BUILD)/wmbusmeters.g $(BUILD)/wmbusmeters-admin $(BUILD)/testinternals

deb: wmbusmeters_$(DEBVERSION)_$(DEBARCH).deb

check_deb:
	lintian --no-tag-display-limit wmbusmeters_$(DEBVERSION)_$(DEBARCH).deb

clean_deb:
	rm -f wmbusmeters_$(DEBVERSION)_$(DEBARCH).deb

# Check docs verifies that all options in the source have been mentioned in the README and in the man page.
# Also any option not in the source but mentioned in the docs is warned for as well.
check_docs:
	@rm -f /tmp/options_in_*
	@cat src/cmdline.cc  | grep -o -- '--[a-z][a-z]*' | sort | uniq | grep -v internaltesting > /tmp/options_in_code
	@cat wmbusmeters.1   | grep -o -- '--[a-z][a-z]*' | sort | uniq | grep -v internaltesting > /tmp/options_in_man
	@cat README.md | grep -o -- '--[a-z][a-z]*' | sort | uniq | grep -v internaltesting > /tmp/options_in_readme
	@./build/wmbusmeters --help | grep -o -- '--[a-z][a-z]*' | sort | uniq | grep -v internaltesting > /tmp/options_in_binary
	@diff /tmp/options_in_code /tmp/options_in_man || echo CODE_VS_MAN
	@diff /tmp/options_in_code /tmp/options_in_readme || echo CODE_VS_README
	@diff /tmp/options_in_code /tmp/options_in_binary || echo CODE_VS_BINARY
	@echo "OK docs"

install: $(BUILD)/wmbusmeters check_docs
	@./install.sh $(BUILD)/wmbusmeters $(DESTDIR) $(EXTRA_INSTALL_OPTIONS)

uninstall:
	@./uninstall.sh /

wmbusmeters_$(DEBVERSION)_$(DEBARCH).deb:
	@./deb/create_deb.sh $(BUILD) $@ $(DEBVERSION) $(DEBARCH)

snapcraft:
	snapcraft

$(BUILD)/main.o: $(BUILD)/short_manual.h $(BUILD)/version.h

# Build binary with debug information. ~15M size binary.
$(BUILD)/wmbusmeters.g: $(PROG_OBJS) $(DRIVER_OBJS) $(BUILD)/main.o $(BUILD)/short_manual.h
	$(CXX) -o $(BUILD)/wmbusmeters.g $(PROG_OBJS) $(DRIVER_OBJS) $(BUILD)/main.o $(LDFLAGS) -lrtlsdr $(USBLIB) -lpthread

# Production build will have debug information stripped. ~1.5M size binary.
# DEBUG=true builds, which has address sanitizer code, will always keep the debug information.
$(BUILD)/wmbusmeters: $(BUILD)/wmbusmeters.g
	cp $(BUILD)/wmbusmeters.g $(BUILD)/wmbusmeters
	$(STRIP_BINARY)

$(BUILD)/wmbusmetersd: $(BUILD)/wmbusmeters
	cp $(BUILD)/wmbusmeters $(BUILD)/wmbusmetersd

ifeq ($(shell uname -s),Darwin)
$(BUILD)/wmbusmeters-admin:
	touch $(BUILD)/wmbusmeters-admin
else
$(BUILD)/wmbusmeters-admin: $(PROG_OBJS) $(DRIVER_OBJS) $(BUILD)/admin.o $(BUILD)/ui.o $(BUILD)/short_manual.h
	$(CXX) -o $(BUILD)/wmbusmeters-admin.g $(PROG_OBJS) $(DRIVER_OBJS) $(BUILD)/admin.o $(BUILD)/ui.o $(LDFLAGS) -lmenu -lform -lncurses -lrtlsdr $(USBLIB) -lpthread
endif

$(BUILD)/short_manual.h: README.md
	echo 'R"MANUAL(' > $(BUILD)/short_manual.h
	sed -n '/wmbusmeters version/,/```/p' README.md \
	| grep -v 'wmbusmeters version' \
	| grep -v '```' >> $(BUILD)/short_manual.h
	echo ')MANUAL";' >> $(BUILD)/short_manual.h

$(BUILD)/testinternals: $(PROG_OBJS) $(DRIVER_OBJS) $(BUILD)/testinternals.o
	$(CXX) -o $(BUILD)/testinternals $(PROG_OBJS) $(DRIVER_OBJS) $(BUILD)/testinternals.o $(LDFLAGS) -lrtlsdr $(USBLIB) -lpthread

$(BUILD)/fuzz: $(PROG_OBJS) $(DRIVER_OBJS) $(BUILD)/fuzz.o
	$(CXX) -o $(BUILD)/fuzz $(PROG_OBJS) $(DRIVER_OBJS) $(BUILD)/fuzz.o $(LDFLAGS) -lrtlsdr -lpthread

clean:
	rm -rf build/* build_arm/* build_debug/* build_arm_debug/* *~

clean_cc:
	find . -name "*.gcov" -delete
	find . -name "*.gcda" -delete

# This generates annotated source files ending in .gcov
# inside the build_debug where non-executed source lines are marked #####
gcov:
	@if [ "$(DEBUG)" = "" ]; then echo "You have to run \"make gcov DEBUG=true\""; exit 1; fi
	$(GCOV) -o build_debug $(PROG_OBJS) $(DRIVER_OBJS)
	mv *.gcov build_debug

lcov:
	@if [ "$(DEBUG)" = "" ]; then echo "You have to run \"make lcov DEBUG=true\""; exit 1; fi
	lcov --directory . -c --no-external --output-file build_debug/lcov.info
	(cd build_debug; genhtml lcov.info)
	xdg-open build_debug/src/index.html

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


GCC_MAJOR_VERSION:=$(shell gcc --version | head -n 1 | sed 's/.* \([0-9]\)\.[0-9]\.[0-9]$$/\1/')
AFL_HOME:=AFLplusplus

$(AFL_HOME)/src/afl-cc.c:
	mkdir -p AFLplusplus
	if ! dpkg -s gcc-$(GCC_MAJOR_VERSION)-plugin-dev 2>/dev/null >/dev/null ; then echo "Please run: sudo apt install gcc-$(GCC_MAJOR_VERSION)-plugin-dev"; exit 1; fi
	git clone https://github.com/AFLplusplus/AFLplusplus.git

afl_prepared:  AFLplusplus/src/afl-cc.c
	(cd AFLplusplus; make)
	touch afl_prepared

build_fuzz: afl_prepared
	$(MAKE) AFL_HARDEN=1 CXX=$(AFL_HOME)/afl-g++-fast FUZZFLAGS=-DFUZZING=true $(BUILD)/fuzz
	$(MAKE) AFL_HARDEN=1 CXX=$(AFL_HOME)/afl-g++-fast FUZZFLAGS=-DFUZZING=true $(BUILD)/wmbusmeters

run_fuzz_difvifparser:
	${AFL_HOME}/afl-fuzz -i fuzz_testcases/difvifparser -o fuzz_findings_difvifparser/ build/fuzz

run_fuzz_telegrams: extract_fuzz_telegram_seeds
	${AFL_HOME}/afl-fuzz -i fuzz_testcases/telegrams -o fuzz_findings_telegrams/ build/wmbusmeters --listento=any stdin

extract_fuzz_telegram_seeds:
	@cat simulations/simulation_* | grep "^telegram=" | tr -d '|' | sed 's/^telegram=//' > $(BUILD)/seeds
	@mkdir -p fuzz_testcases/telegrams
	@rm -f fuzz_testcases/telegrams/seed_*
	@SEED=1; while read -r line; do echo "$${line}" | xxd -r -p - > "fuzz_testcases/telegrams/seed_$${SEED}"; SEED=$$((SEED + 1)); done < $(BUILD)/seeds; echo "Extracted $${SEED} seeds from simulations."

relay: utils/relay.c
	gcc -g utils/relay.c -o relay -O0 -ggdb -fsanitize=address -fno-omit-frame-pointer -fprofile-arcs -ftest-coverage

# Include dependency information generated by gcc in a previous compile.
include $(wildcard $(patsubst %.o,%.d,$(PROG_OBJS) $(DRIVER_OBJS)))
