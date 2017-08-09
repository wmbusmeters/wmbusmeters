
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
    CXX=clang++
    STRIP=strip
    #CXX=g++
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


CXXFLAGS := $(DEBUG_FLAGS) -Wall -fmessage-length=0 -std=c++11 -Wno-unused-function "-DWMBUSMETERS_VERSION=\"0.1\"" 

$(BUILD)/%.o: %.cc $(wildcard %.h)
	$(CXX) $(CXXFLAGS) $< -c -o $@

METERS_OBJS:=\
	$(BUILD)/main.o \
	$(BUILD)/util.o \
	$(BUILD)/aes.o \
	$(BUILD)/serial.o \
	$(BUILD)/wmbus.o \
	$(BUILD)/wmbus_im871a.o \
	$(BUILD)/meters.o \
	$(BUILD)/meter_multical21.o

all: $(BUILD)/wmbusmeters
	$(STRIP_BINARY)

$(BUILD)/wmbusmeters: $(METERS_OBJS)
	$(CXX) -o $(BUILD)/wmbusmeters $(METERS_OBJS) -lpthread

clean:
	rm -f build/* build_arm/* build_debug/* build_arm_debug/* *~
