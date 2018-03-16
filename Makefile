
# To compile for Raspberry PI ARM:
# make HOST=arm
#
# To build with debug information:
# make DEBUG=true
# make DEBUG=true HOST=arm

ifeq "$(HOST)" "arm"
    CXX=arm-linux-gnueabihf-g++
    STRIP=arm-linux-gnueabihf-strip
    BUILD=build_arm
else
    CXX=g++
    STRIP=strip
    BUILD=build
endif

ifeq "$(DEBUG)" "true"
    DEBUG_FLAGS=-O0 -g
    STRIP_BINARY=
    BUILD:=$(BUILD)_debug
else
    DEBUG_FLAGS=-Os
    STRIP_BINARY=$(STRIP) $(BUILD)/wmbusmeters
endif

$(shell mkdir -p $(BUILD))

CXXFLAGS := $(DEBUG_FLAGS) -Wall -fmessage-length=0 -std=c++11 -Wno-unused-function "-DWMBUSMETERS_VERSION=\"0.4\""

$(BUILD)/%.o: %.cc $(wildcard %.h)
	$(CXX) $(CXXFLAGS) $< -c -o $@

METERS_OBJS:=\
	$(BUILD)/aes.o \
	$(BUILD)/cmdline.o \
	$(BUILD)/main.o \
	$(BUILD)/meters.o \
	$(BUILD)/meter_multical21.o \
	$(BUILD)/meter_multical302.o \
	$(BUILD)/meter_omnipower.o \
	$(BUILD)/printer.o \
	$(BUILD)/serial.o \
	$(BUILD)/util.o \
	$(BUILD)/wmbus.o \
	$(BUILD)/wmbus_amb8465.o \
	$(BUILD)/wmbus_im871a.o \
	$(BUILD)/wmbus_simulator.o \
	$(BUILD)/wmbus_utils.o

all: $(BUILD)/wmbusmeters
	$(STRIP_BINARY)

$(BUILD)/wmbusmeters: $(METERS_OBJS)
	$(CXX) -o $(BUILD)/wmbusmeters $(METERS_OBJS) -lpthread

clean:
	rm -f build/* build_arm/* build_debug/* build_arm_debug/* *~

test:
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
	cat tmpfile | sed -e "s/X(|\(.\)\(.\)\(.\)/X(\1\2\3|MANFCODE('\1','\2','\3')|/g" | \
	tr -s '|' ',' >> m.h
	echo >> m.h
	cat tmpfile | sed -e "s/X(|\(.\)\(.\)\(.\).*/#define MANUFACTURER_\1\2\3 MANFCODE('\1','\2','\3')/g" \
	>> m.h
	echo >> m.h
	echo '#endif' >> m.h
	rm tmpfile
	mv m.h manufacturers.h
