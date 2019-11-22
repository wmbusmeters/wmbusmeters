/*
 Copyright (C) 2017-2019 Fredrik Öhrström

 This program is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include"wmbus.h"
#include<assert.h>
#include<stdarg.h>
#include<string.h>
#include<sys/stat.h>
#include<sys/types.h>
#include<unistd.h>

struct LinkModeInfo {
    LinkMode mode;
    const char *name;
    const char *lcname;
    const char *option;
    int val;
};

LinkModeInfo link_modes_[] = {
#define X(name,lcname,option,val) { LinkMode::name, #name , #lcname, #option, val },
LIST_OF_LINK_MODES
#undef X
};

LinkModeInfo *getLinkModeInfo(LinkMode lm);
LinkModeInfo *getLinkModeInfoFromBit(int bit);

LinkModeInfo *getLinkModeInfo(LinkMode lm)
{
    for (auto& s : link_modes_)
    {
        if (s.mode == lm)
        {
            return &s;
        }
    }
    assert(0);
    return NULL;
}

LinkModeInfo *getLinkModeInfoFromBit(int bit)
{
    for (auto& s : link_modes_)
    {
        if (s.val == bit)
        {
            return &s;
        }
    }
    assert(0);
    return NULL;
}

LinkMode isLinkModeOption(const char *arg)
{
    for (auto& s : link_modes_) {
        if (!strcmp(arg, s.option)) {
            return s.mode;
        }
    }
    return LinkMode::UNKNOWN;
}

LinkMode isLinkMode(const char *arg)
{
    for (auto& s : link_modes_) {
        if (!strcmp(arg, s.lcname)) {
            return s.mode;
        }
    }
    return LinkMode::UNKNOWN;
}

LinkModeSet parseLinkModes(string m)
{
    LinkModeSet lms;
    char buf[m.length()+1];
    strcpy(buf, m.c_str());
    const char *tok = strtok(buf, ",");
    while (tok != NULL)
    {
        LinkMode lm = isLinkMode(tok);
        if (lm == LinkMode::UNKNOWN)
        {
            error("(wmbus) not a valid link mode: %s\n", tok);
        }
        lms.addLinkMode(lm);
        tok = strtok(NULL, ",");
    }
    return lms;
}

void LinkModeSet::addLinkMode(LinkMode lm)
{
    for (auto& s : link_modes_) {
        if (s.mode == lm) {
            set_ |= s.val;
        }
    }
}

void LinkModeSet::unionLinkModeSet(LinkModeSet lms)
{
    set_ |= lms.set_;
}

void LinkModeSet::disjunctionLinkModeSet(LinkModeSet lms)
{
    set_ &= lms.set_;
}

bool LinkModeSet::supports(LinkModeSet lms)
{
    // Will return false, if lms is UKNOWN (=0).
    return (set_ & lms.set_) != 0;
}

bool LinkModeSet::has(LinkMode lm)
{
    LinkModeInfo *lmi = getLinkModeInfo(lm);
    return (set_ & lmi->val) != 0;
}

bool LinkModeSet::hasAll(LinkModeSet lms)
{
    return (set_ & lms.set_) == lms.set_;
}

string LinkModeSet::hr()
{
    string r;
    if (set_ == Any_bit) return "any";
    if (set_ == 0) return "none";
    for (auto& s : link_modes_)
    {
        if (s.mode == LinkMode::Any) continue;
        if (set_ & s.val)
        {
            r += s.lcname;
            r += ",";
        }
    }
    r.pop_back();
    return r;
}

struct Manufacturer {
    const char *code;
    int m_field;
    const char *name;

    Manufacturer(const char *c, int m, const char *n) {
        code = c;
        m_field = m;
        name = n;
    }
};

vector<Manufacturer> manufacturers_;

struct Initializer { Initializer(); };

static Initializer initializser_;

Initializer::Initializer() {

#define X(key,code,name) manufacturers_.push_back(Manufacturer(#key,code,name));
LIST_OF_MANUFACTURERS
#undef X

}

void Telegram::print() {
    notice("Received telegram from: %02x%02x%02x%02x\n",
	   a_field_address[0], a_field_address[1], a_field_address[2], a_field_address[3]);
    notice("          manufacturer: (%s) %s\n",
           manufacturerFlag(m_field).c_str(),
	   manufacturer(m_field).c_str());
    notice("           device type: %s\n",
	   mediaType(a_field_device_type).c_str());
}

void Telegram::verboseFields() {
    string man = manufacturerFlag(m_field);
    verbose(" %02x%02x%02x%02x C-field=%02x M-field=%04x (%s) A-field-version=%02x A-field-dev-type=%02x (%s) Ci-field=%02x (%s)",
            a_field_address[0], a_field_address[1], a_field_address[2], a_field_address[3],
            c_field,
            m_field,
            man.c_str(),
            a_field_version,
            a_field_device_type,
            mediaType(a_field_device_type).c_str(),
            ci_field,
            ciType(ci_field).c_str());

    if (ci_field == 0x78) {
        // No data error and no encryption possible.
    }

    if (ci_field == 0x72) {
        // Long tpl header
        verbose(" CC-field=%02x (%s) long tpl header ACC=%02x SN=%02x%02x%02x%02x",
                cc_field, ccType(cc_field).c_str(),
                acc,
                sn[3],sn[2],sn[1],sn[0]);
    }

    if (ci_field == 0x7a) {
        // Short data header
        verbose(" CC-field=%02x (%s) short header ACC=%02x ",
                cc_field, ccType(cc_field).c_str(),
                acc,
                sn[3],sn[2],sn[1],sn[0]);
    }

    if (ci_field == 0x8d) {
        // ELL header
        verbose(" CC-field=%02x (%s) ell header ACC=%02x SN=%02x%02x%02x%02x",
                cc_field, ccType(cc_field).c_str(),
                acc,
                sn[3],sn[2],sn[1],sn[0]);
    }
    if (ci_field == 0x8c) {
        verbose(" CC-field=%02x (%s) ACC=%02x",
                cc_field, ccType(cc_field).c_str(),
                acc);
    }
    verbose("\n");
}

string manufacturer(int m_field) {
    for (auto &m : manufacturers_) {
	if (m.m_field == m_field) return m.name;
    }
    return "Unknown";
}

string manufacturerFlag(int m_field) {
    char a = (m_field/1024)%32+64;
    char b = (m_field/32)%32+64;
    char c = (m_field)%32+64;

    string flag;
    flag += a;
    flag += b;
    flag += c;
    return flag;
}

string mediaType(int a_field_device_type) {
    switch (a_field_device_type) {
    case 0: return "Other";
    case 1: return "Oil meter";
    case 2: return "Electricity meter";
    case 3: return "Gas meter";
    case 4: return "Heat meter";
    case 5: return "Steam meter";
    case 6: return "Warm Water (30°C-90°C) meter";
    case 7: return "Water meter";
    case 8: return "Heat Cost Allocator";
    case 9: return "Compressed air meter";
    case 0x0a: return "Cooling load volume at outlet meter";
    case 0x0b: return "Cooling load volume at inlet meter";
    case 0x0c: return "Heat volume at inlet meter";
    case 0x0d: return "Heat/Cooling load meter";
    case 0x0e: return "Bus/System component";
    case 0x0f: return "Unknown";
    case 0x15: return "Hot water (>=90°C) meter";
    case 0x16: return "Cold water meter";
    case 0x17: return "Hot/Cold water meter";
    case 0x18: return "Pressure meter";
    case 0x19: return "A/D converter";
    case 0x1A: return "Smoke detector";
    case 0x1B: return "Room sensor (eg temperature or humidity)";
    case 0x1C: return "Gas detector";
    case 0x1D: return "Reserved for sensors";
    case 0x1F: return "Reserved for sensors";
    case 0x20: return "Breaker (electricity)";
    case 0x21: return "Valve (gas or water)";
    case 0x22: return "Reserved for switching devices";
    case 0x23: return "Reserved for switching devices";
    case 0x24: return "Reserved for switching devices";
    case 0x25: return "Customer unit (display device)";
    case 0x26: return "Reserved for customer units";
    case 0x27: return "Reserved for customer units";
    case 0x28: return "Waste water";
    case 0x29: return "Garbage";
    case 0x2A: return "Reserved for Carbon dioxide";
    case 0x2B: return "Reserved for environmental meter";
    case 0x2C: return "Reserved for environmental meter";
    case 0x2D: return "Reserved for environmental meter";
    case 0x2E: return "Reserved for environmental meter";
    case 0x2F: return "Reserved for environmental meter";
    case 0x30: return "Reserved for system devices";
    case 0x31: return "Reserved for communication controller";
    case 0x32: return "Reserved for unidirectional repeater";
    case 0x33: return "Reserved for bidirectional repeater";
    case 0x34: return "Reserved for system devices";
    case 0x35: return "Reserved for system devices";
    case 0x36: return "Radio converter (system side)";
    case 0x37: return "Radio converter (meter side)";
    case 0x38: return "Reserved for system devices";
    case 0x39: return "Reserved for system devices";
    case 0x3A: return "Reserved for system devices";
    case 0x3B: return "Reserved for system devices";
    case 0x3C: return "Reserved for system devices";
    case 0x3D: return "Reserved for system devices";
    case 0x3E: return "Reserved for system devices";
    case 0x3F: return "Reserved for system devices";

    // Techem MK Radio 3 manufacturer specific.
    case 0x62: return "Warm water"; // MKRadio3
    case 0x72: return "Cold water"; // MKRadio3

    // Techem Vario 4 Typ 4.5.1 manufacturer specific.
    case 0xC3: return "Heat meter";

    }
    return "Unknown";
}

string mediaTypeJSON(int a_field_device_type)
{
    switch (a_field_device_type) {
    case 0: return "other";
    case 1: return "oil";
    case 2: return "electricity";
    case 3: return "gas";
    case 4: return "heat";
    case 5: return "steam";
    case 6: return "warm water";
    case 7: return "water";
    case 8: return "heat cost allocation";
    case 9: return "compressed air";
    case 0x0a: return "cooling load volume at outlet";
    case 0x0b: return "cooling load volume at inlet";
    case 0x0c: return "heat volume at inlet";
    case 0x0d: return "heat/cooling load";
    case 0x0e: return "bus/system component";
    case 0x0f: return "unknown";
    case 0x15: return "hot water";
    case 0x16: return "cold water";
    case 0x17: return "hot/cold water";
    case 0x18: return "pressure";
    case 0x19: return "a/d converter";
    case 0x1A: return "smoke detector";
    case 0x1B: return "room sensor";
    case 0x1C: return "gas detector";
    case 0x1D: return "reserved";
    case 0x1F: return "reserved";
    case 0x20: return "breaker";
    case 0x21: return "valve";
    case 0x22: return "reserved";
    case 0x23: return "reserved";
    case 0x24: return "reserved";
    case 0x25: return "customer unit (display device)";
    case 0x26: return "reserved";
    case 0x27: return "reserved";
    case 0x28: return "waste water";
    case 0x29: return "garbage";
    case 0x2A: return "reserved";
    case 0x2B: return "reserved";
    case 0x2C: return "reserved";
    case 0x2D: return "reserved";
    case 0x2E: return "reserved";
    case 0x2F: return "reserved";
    case 0x30: return "reserved";
    case 0x31: return "reserved";
    case 0x32: return "reserved";
    case 0x33: return "reserved";
    case 0x34: return "reserved";
    case 0x35: return "reserved";
    case 0x36: return "radio converter (system side)";
    case 0x37: return "radio converter (meter side)";
    case 0x38: return "reserved";
    case 0x39: return "reserved";
    case 0x3A: return "reserved";
    case 0x3B: return "reserved";
    case 0x3C: return "reserved";
    case 0x3D: return "reserved";
    case 0x3E: return "reserved";
    case 0x3F: return "reserved";

    // Techem MK Radio 3 manufacturer specific codes:
    case 0x62: return "warm water";
    case 0x72: return "cold water";

    // Techem Vario 4 Typ 4.5.1 manufacturer specific codes:
    case 0xC3: return "heat";

    }
    return "Unknown";
}


bool detectIM871A(string device, SerialCommunicationManager *handler);
bool detectAMB8465(string device, SerialCommunicationManager *handler);
bool detectRawTTY(string device, int baud, SerialCommunicationManager *handler);
bool detectRTLSDR(string device, SerialCommunicationManager *handler);
bool detectCUL(string device, SerialCommunicationManager *handler);

Detected detectAuto(string devicefile,
                    string suffix,
                    SerialCommunicationManager *handler)
{
    if (suffix != "")
    {
        error("You cannot have a suffix appended to auto.\n");
    }

    if (detectIM871A("/dev/im871a", handler))
    {
        return { DEVICE_IM871A, "/dev/im871a", 0, false };
    }
    else
    {
        AccessCheck ac = checkIfExistsAndSameGroup("/dev/im871a");
        if (ac == AccessCheck::NotSameGroup)
        {
            // The device exists but we cannot read it!
            error("You are not in the same group as the device /dev/im871a\n");
        }
    }

    if (detectAMB8465("/dev/amb8465", handler))
    {
        return { DEVICE_AMB8465, "/dev/amb8465", false };
    }
    else
    {
        AccessCheck ac = checkIfExistsAndSameGroup("/dev/amb8465");
        if (ac == AccessCheck::NotSameGroup)
        {
            // The device exists but we cannot read it!
            error("You are not in the same group as the device /dev/amb8465\n");
        }
    }

    if (detectRawTTY("/dev/rfmrx2", 38400, handler))
    {
        return { DEVICE_RFMRX2, "/dev/rfmrx2", false };
    }
    else
    {
        AccessCheck ac = checkIfExistsAndSameGroup("/dev/rfmrx2");
        if (ac == AccessCheck::NotSameGroup)
        {
            // The device exists but we cannot read it!
            error("You are not in the same group as the device /dev/rfmrx2\n");
        }
    }

    if (detectRTLSDR("/dev/rtlsdr", handler))
    {
        return { DEVICE_RTLWMBUS, "rtlwmbus" };
    }
    else
    {
        AccessCheck ac = checkIfExistsAndSameGroup("/dev/amb8465");
        if (ac == AccessCheck::NotSameGroup)
        {
            // The device exists but we cannot read it!
            error("You are not in the same group as the device /dev/rtlsdr\n");
        }
    }

    if (detectCUL("/dev/ttyUSB0", handler))
    {
        return { DEVICE_CUL, "/dev/ttyUSB0" };
    }
    else
    {
        AccessCheck ac = checkIfExistsAndSameGroup("/dev/ttyUSB0");
        if (ac == AccessCheck::NotSameGroup)
        {
            // The device exists but we cannot read it!
            error("You are not in the same group as the device CUL\n");
        }
    }

    // We could not auto-detect any device.
    return { DEVICE_UNKNOWN, "", false };
}

Detected detectImstAmberCul(string devicefile,
                            string suffix,
                            SerialCommunicationManager *handler)
{
    // If im87a is tested first, a delay of 1s must be inserted
    // before amb8465 is tested, lest it will not respond properly.
    // It really should not matter, but perhaps is the uart of the amber
    // confused by the 57600 speed....or maybe there is some other reason.
    // Anyway by testing for the amb8465 first, we can immediately continue
    // with the test for the im871a, without the need for a 1s delay.

    // Talk amb8465 with it...
    // assumes this device is configured for 9600 bps, which seems to be the default.
    if (detectAMB8465(devicefile, handler))
    {
        return { DEVICE_AMB8465, devicefile, false };
    }
    // Talk im871a with it...
    // assumes this device is configured for 57600 bps, which seems to be the default.
    if (detectIM871A(devicefile, handler))
    {
        return { DEVICE_IM871A, devicefile, false };
    }
    // Talk CUL with it...
    // assumes this device is configured for 38400 bps, which seems to be the default.
    if (detectCUL(devicefile, handler))
    {
        return { DEVICE_CUL, devicefile, false };
    }

    // We could not auto-detect either.
    return { DEVICE_UNKNOWN, "", false };
}

/**
  The devicefile can be:

  auto (to autodetect the device)
  /dev/ttyUSB0 (to use this character device)
  /home/me/simulation.txt or /home/me/simulation_foo.txt (to use the wmbusmeters telegram=|....|+32 format)
  /home/me/telegram.raw (to read bytes from this file)
  stdin (to read bytes from stdin)

  If a suffix he suffix can be:
  im871a
  amb8465
  rfmrx2
  rtlwmbus: the devicefile produces rtlwmbus messages, ie. T1;1;1;2019-04-03 19:00:42.000;97;148;88888888;0x6e440106...ae03a77
  simulation: assume the devicefile produces telegram=|....|+xx lines. This can also pace the simulated telegrams in time.
  a baud rate like 38400: assume the devicefile is a raw tty character device.
*/
Detected detectWMBusDeviceSetting(string devicefile,
                                  string suffix,
                                  SerialCommunicationManager *handler)
{
    debug("(detect) \"%s\" \"%s\"\n", devicefile.c_str(), suffix.c_str());
    // Look for /dev/im871a /dev/amb8465 /dev/rfmrx2 /dev/rtlsdr
    if (devicefile == "auto")
    {
        debug("(detect) driver: auto\n");
        return detectAuto(devicefile, suffix, handler);
    }

    // If the devicefile is rtlwmbus then the suffix can be a frequency
    // or the actual command line to use.
    // E.g. rtlwmbus rtlwmbux:868.95M rtlwmbus:rtl_sdr | rtl_wmbus
    if (devicefile == "rtlwmbus")
    {
        debug("(detect) driver: rtlwmbus\n");
        return { DEVICE_RTLWMBUS, "", false };
    }

    // Is it a file named simulation_xxx.txt ?
    if (checkIfSimulationFile(devicefile.c_str()))
    {
        debug("(detect) driver: simulation file\n");
        return { DEVICE_SIMULATOR, devicefile, false };
    }

    bool is_tty = checkCharacterDeviceExists(devicefile.c_str(), false);
    bool is_stdin = devicefile == "stdin";
    bool is_file = checkFileExists(devicefile.c_str());

    debug("(detect) is_tty=%d is_stdin=%d is_file=%d\n", is_tty, is_stdin, is_file);
    if (!is_tty && !is_stdin && !is_file)
    {
        debug("(detect) not a valid device file %s\n", devicefile.c_str());
        // Oups, not a valid devicefile.
        return { DEVICE_UNKNOWN, "", false };
    }

    bool override_tty = !is_tty;

    if (suffix == "amb8465") return { DEVICE_AMB8465, devicefile, 0, override_tty };
    if (suffix == "im871a") return { DEVICE_IM871A, devicefile, 0, override_tty };
    if (suffix == "rfmrx2") return { DEVICE_RFMRX2, devicefile, 0, override_tty };
    if (suffix == "rtlwmbus") return { DEVICE_RTLWMBUS, devicefile, 0, override_tty };
    if (suffix == "cul") return { DEVICE_CUL, devicefile, 0, override_tty };
    if (suffix == "simulation") return { DEVICE_SIMULATOR, devicefile, 0, override_tty };

    // If the suffix is a number, then assume that it is a baud rate.
    if (isNumber(suffix)) return { DEVICE_RAWTTY, devicefile, atoi(suffix.c_str()), override_tty };

    // If the suffix is empty and its not a tty, then read raw telegrams from stdin or the file.
    if (suffix == "" && !is_tty) return { DEVICE_RAWTTY, devicefile, 0, true };

    if (suffix != "")
    {
        error("Unknown device suffix %s\n", suffix.c_str());
    }

    // Ok, we are left with a single /dev/ttyUSB0 lets talk to it
    // to figure out what is connected to it. We currently only
    // know how to detect Imst, Amber or CUL dongles.
    return detectImstAmberCul(devicefile, suffix, handler);
}


string ciType(int ci_field)
{
    if (ci_field >= 0xA0 && ci_field <= 0xB7) {
        return "Mfct specific";
    }
    switch (ci_field) {
    case 0x60: return "COSEM Data sent by the Readout device to the meter with long Transport Layer";
    case 0x61: return "COSEM Data sent by the Readout device to the meter with short Transport Layer";
    case 0x64: return "Reserved for OBIS-based Data sent by the Readout device to the meter with long Transport Layer";
    case 0x65: return "Reserved for OBIS-based Data sent by the Readout device to the meter with short Transport Layer";
    case 0x69: return "EN 13757-3 Application Layer with Format frame and no Transport Layer";
    case 0x6A: return "EN 13757-3 Application Layer with Format frame and with short Transport Layer";
    case 0x6B: return "EN 13757-3 Application Layer with Format frame and with long Transport Layer";
    case 0x6C: return "Clock synchronisation (absolute)";
    case 0x6D: return "Clock synchronisation (relative)";
    case 0x6E: return "Application error from device with short Transport Layer";
    case 0x6F: return "Application error from device with long Transport Layer";
    case 0x70: return "Application error from device without Transport Layer";
    case 0x71: return "Reserved for Alarm Report";
    case 0x72: return "EN 13757-3 Application Layer with long Transport Layer";
    case 0x73: return "EN 13757-3 Application Layer with Compact frame and long Transport Layer";
    case 0x74: return "Alarm from device with short Transport Layer";
    case 0x75: return "Alarm from device with long Transport Layer";
    case 0x78: return "EN 13757-3 Application Layer without Transport Layer (to be defined)";
    case 0x79: return "EN 13757-3 Application Layer with Compact frame and no header";
    case 0x7A: return "EN 13757-3 Application Layer with short Transport Layer";
    case 0x7B: return "EN 13757-3 Application Layer with Compact frame and short header";
    case 0x7C: return "COSEM Application Layer with long Transport Layer";
    case 0x7D: return "COSEM Application Layer with short Transport Layer";
    case 0x7E: return "Reserved for OBIS-based Application Layer with long Transport Layer";
    case 0x7F: return "Reserved for OBIS-based Application Layer with short Transport Layer";
    case 0x80: return "EN 13757-3 Transport Layer (long) from other device to the meter";
    case 0x81: return "Network Layer data";
    case 0x82: return "For future use";
    case 0x83: return "Network Management application";
    case 0x8A: return "EN 13757-3 Transport Layer (short) from the meter to the other device";
    case 0x8B: return "EN 13757-3 Transport Layer (long) from the meter to the other device";
    case 0x8C: return "Extended Link Layer I (2 Byte)";
    case 0x8D: return "Extended Link Layer II (8 Byte)";
    }
    return "?";
}

void Telegram::addExplanation(vector<uchar>::iterator &bytes, int len, const char* fmt, ...)
{
    char buf[1024];
    buf[1023] = 0;

    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, 1023, fmt, args);
    va_end(args);

    explanations.push_back({parsed.size(), buf});
    parsed.insert(parsed.end(), bytes, bytes+len);
    bytes += len;
}

void Telegram::addMoreExplanation(int pos, const char* fmt, ...)
{
    char buf[1024];

    buf[1023] = 0;

    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, 1023, fmt, args);
    va_end(args);

    bool found = false;
    for (auto& p : explanations) {
        if (p.first == pos) {
            if (p.second[0] == '*') {
                debug("(wmbus) warning: already added more explanations to offset %d!\n");
            }
            p.second = string("* ")+p.second+buf;
            found = true;
        }
    }

    if (!found) {
        debug("(wmbus) warning: cannot find offset %d to add more explanation \"%s\"\n", pos, buf);
    }
}

bool Telegram::parse(vector<uchar> &frame)
{
    vector<uchar>::iterator bytes = frame.begin();
    parsed.clear();
    if (frame.size() == 0) return false;
    if (frame.size() < 11)
    {
        verbose("(wmbus) cannot parse telegram with length %zu\n", frame.size());
        return false;
    }
    len = frame[0];
    if ((int)len+1 > (int)frame.size())
    {
        // I have some bad test data, that needs to be cleaned out...
        verbose("(wmbus) error not enough bytes frame=%zu but len=%zu\n", frame.size(), len);
    }
    if ((int)len+1 != (int)frame.size())
    {
        // I have some bad test data, that needs to be cleaned out...
        verbose("(wmbus) discrepancy frame=%zu should be len=%zu\n", frame.size(), len);
    }
    addExplanation(bytes, 1, "%02x length (%d bytes)", len, len);
    c_field = frame[1];
    addExplanation(bytes, 1, "%02x c-field (%s)", c_field, cType(c_field).c_str());
    m_field = frame[3]<<8 | frame[2];
    string man = manufacturerFlag(m_field);
    addExplanation(bytes, 2, "%02x%02x m-field (%02x=%s)", frame[2], frame[3], m_field, man.c_str());
    a_field.resize(6);
    a_field_address.resize(4);
    for (int i=0; i<6; ++i) {
        a_field[i] = frame[4+i];
        if (i<4) { a_field_address[i] = frame[4+3-i]; }
    }
    addExplanation(bytes, 4, "%02x%02x%02x%02x a-field-addr (%02x%02x%02x%02x)", frame[4], frame[5], frame[6], frame[7],
                   frame[7], frame[6], frame[5], frame[4]);

    strprintf(id, "%02x%02x%02x%02x", frame[7], frame[6], frame[5], frame[4]);
    a_field_version = frame[4+4];
    a_field_device_type = frame[4+5];
    addExplanation(bytes, 1, "%02x a-field-version", frame[8]);
    addExplanation(bytes, 1, "%02x a-field-type (%s)", frame[9], mediaType(a_field_device_type).c_str());

    ci_field=frame[10];
    addExplanation(bytes, 1, "%02x ci-field (%s)", ci_field, ciType(ci_field).c_str());

    int header_size = 0;
    if (ci_field == 0x78)
    {
        header_size = 0; // And no encryption possible.
    }
    else if (ci_field == 0x72)
    {
        if (frame.size() < 22)
        {
            verbose("(wmbus) cannot parse telegram ci=0x72 with length %zu\n", frame.size());
            return false;
        }

        // Example, begins aith frame[11]: 99999999 MMMM VV TT 01 00 0000
        // Ignore 4 id bytes for now, should perhaps check that they are identical with the id.
        // But if they are not, what to do?
        // Ignore 2 mfc bytes for now, check if same as above?
        // Ignore 1 version byte.
        // Ignore 1 dev type byte.
        acc = frame[18];
        addExplanation(bytes, 1, "%02x acc", acc);
        status = frame[19];
        addExplanation(bytes, 1, "%02x status ()", status);
        config_field = frame[20]<<8 | frame[21];
        string config_info = "";
        if (config_field & 0x0f) {
            config_info += "encrypted ";
            is_encrypted_ = true;
        }
        if ((config_field & 0x0f) == 0 || (config_field & 0x0f) == 0x05) {
            if ((config_field & 0x0f) == 0x05) config_info += "AES_CBC ";
            if (config_field & 0x80) config_info += "bidirectional ";
            if (config_field & 0x40) config_info += "accessibility ";
            if (config_field & 0x20) config_info += "synchronous ";
        }
        if (config_info.length() > 0) config_info.pop_back();
        addExplanation(bytes, 2, "%02x%02x config (%s)", frame[13], frame[14], config_info.c_str());
        header_size = 4+8;
    }
    else if (ci_field == 0x7a)
    {
        if (frame.size() < 15)
        {
            verbose("(wmbus) cannot parse telegram ci=0x7a with length %zu\n", frame.size());
            return false;
        }
        acc = frame[11];
        addExplanation(bytes, 1, "%02x acc", acc);
        status = frame[12];
        addExplanation(bytes, 1, "%02x status ()", status);
        config_field = frame[13]<<8 | frame[14];
        string config_info = "";
        if (config_field & 0x0f) {
            config_info += "encrypted ";
            is_encrypted_ = true;
        }
        if ((config_field & 0x0f) == 0 || (config_field & 0x0f) == 0x05) {
            if ((config_field & 0x0f) == 0x05) config_info += "AES_CBC ";
            if (config_field & 0x80) config_info += "bidirectional ";
            if (config_field & 0x40) config_info += "accessibility ";
            if (config_field & 0x20) config_info += "synchronous ";
        }
        if (config_info.length() > 0) config_info.pop_back();
        addExplanation(bytes, 2, "%02x%02x config (%s)", frame[13], frame[14], config_info.c_str());
        header_size = 4;
    }
    else if (ci_field == 0x8d || ci_field == 0x8c)
    {
        if (frame.size() < 13)
        {
            verbose("(wmbus) cannot parse telegram ci=0x8d or 0x8c with length %zu\n", frame.size());
            return false;
        }
        cc_field = frame[11];
        addExplanation(bytes, 1, "%02x cc-field (%s)", cc_field, ccType(cc_field).c_str());
        acc = frame[12];
        addExplanation(bytes, 1, "%02x acc", acc);
        header_size = 2;
        if (ci_field == 0x8d)
        {
            if (frame.size() < 17)
            {
                verbose("(wmbus) cannot parse telegram ci=0x8d with length %zu\n", frame.size());
                return false;
            }
            string sn_info;
            sn[0] = frame[13];
            sn[1] = frame[14];
            sn[2] = frame[15];
            sn[3] = frame[16];
            uint64_t sn_field = sn[3]<<24 | sn[2]<<16 | sn[1] << 8 | sn[0];

            uchar session_field = (sn_field >> 0)  & 0x0f; // lowest 4 bits
            uint64_t time_field = (sn_field >> 4)  & 0x1ffffff; // next 25 bits
            uchar enc_field     = (sn_field >> 29) & 0x7; // next 3 bits.
            if (enc_field != 0)
            {
                sn_info += "encrypted ";
                is_encrypted_ = true;
            }
            sn_info += "session=";
            sn_info += to_string(session_field)+" ";
            sn_info += "time=";
            sn_info += to_string(time_field);
            addExplanation(bytes, 4, "%02x%02x%02x%02x sn (%s)", sn[0], sn[1], sn[2], sn[3], sn_info.c_str());
            header_size = 6;
        }
    }
    else if (ci_field == 0xa2)
    {
        // Manufacturer specific telegram payload. Oh well....
    }
    else
    {
        // Removed because it logs warning for every telegram, even if not 'for me'
        //warning("Unknown ci-field %02x\n", ci_field);
    }

    payload.clear();
    int skip = 11+header_size;
    if (skip < (int)frame.size())
    {
        // The case 11+header_size larger than frame_size is probably due to bad input data
        // that is currently allowed to pass, see above.
        payload.insert(payload.end(), frame.begin()+skip, frame.end());
    }
    verbose("(wmbus) received telegram");
    verboseFields();
    debugPayload("(wmbus) frame", frame);
    debugPayload("(wmbus) payload", payload);
    if (isDebugEnabled()) {
        explainParse("(wmbus)", 0);
    }
    return true;
}

void Telegram::explainParse(string intro, int from)
{
    for (auto& p : explanations) {
        if (p.first < from) continue;
        debug("%s %02x: %s\n", intro.c_str(), p.first, p.second.c_str());
    }
    string hex = bin2hex(parsed);
    debug("%s %s\n", intro.c_str(), hex.c_str());
}

void Telegram::expectVersion(const char *info, int v)
{
    if (v != 0 && a_field_version != v) {
        warning("(%s) expected telegram with version 0x%02x, but got version 0x%02x !\n", info, v, a_field_version);
    }
}

string cType(int c_field)
{
    switch (c_field) {
    case 0x44: return "SND_NR";
    case 0x46: return "SND_IR";
    case 0x48: return "RSP_UD";
    }
    return "?";
}

string ccType(int cc_field)
{
    string s = "";
    if (cc_field & CC_B_BIDIRECTIONAL_BIT) s += "bidir ";
    if (cc_field & CC_RD_RESPONSE_DELAY_BIT) s += "fast_resp ";
    else s += "slow_resp ";
    if (cc_field & CC_S_SYNCH_FRAME_BIT) s += "sync ";
    if (cc_field & CC_R_RELAYED_BIT) s+= "relayed "; // Relayed by a repeater
    if (cc_field & CC_P_HIGH_PRIO_BIT) s+= "prio ";

    if (s.back() == ' ') s.pop_back();
    return s;
}


int difLenBytes(int dif)
{
    int t = dif & 0x0f;
    switch (t) {
    case 0x0: return 0; // No data
    case 0x1: return 1; // 8 Bit Integer/Binary
    case 0x2: return 2; // 16 Bit Integer/Binary
    case 0x3: return 3; // 24 Bit Integer/Binary
    case 0x4: return 4; // 32 Bit Integer/Binary
    case 0x5: return 4; // 32 Bit Real
    case 0x6: return 6; // 48 Bit Integer/Binary
    case 0x7: return 8; // 64 Bit Integer/Binary
    case 0x8: return 0; // Selection for Readout
    case 0x9: return 1; // 2 digit BCD
    case 0xA: return 2; // 4 digit BCD
    case 0xB: return 3; // 6 digit BCD
    case 0xC: return 4; // 8 digit BCD
    case 0xD: return -1; // variable length
    case 0xE: return 6; // 12 digit BCD
    case 0xF: // Special Functions
        if (dif == 0x2f) return 1; // The skip code 0x2f, used for padding.
        return -2;
    }
    // Bad!
    return -2;
}

string difType(int dif)
{
    string s;
    int t = dif & 0x0f;
    switch (t) {
    case 0x0: s+= "No data"; break;
    case 0x1: s+= "8 Bit Integer/Binary"; break;
    case 0x2: s+= "16 Bit Integer/Binary"; break;
    case 0x3: s+= "24 Bit Integer/Binary"; break;
    case 0x4: s+= "32 Bit Integer/Binary"; break;
    case 0x5: s+= "32 Bit Real"; break;
    case 0x6: s+= "48 Bit Integer/Binary"; break;
    case 0x7: s+= "64 Bit Integer/Binary"; break;
    case 0x8: s+= "Selection for Readout"; break;
    case 0x9: s+= "2 digit BCD"; break;
    case 0xA: s+= "4 digit BCD"; break;
    case 0xB: s+= "6 digit BCD"; break;
    case 0xC: s+= "8 digit BCD"; break;
    case 0xD: s+= "variable length"; break;
    case 0xE: s+= "12 digit BCD"; break;
    case 0xF: s+= "Special Functions"; break;
    default: s+= "?"; break;
    }

    if (t != 0xf)
    {
        // Only print these suffixes when we have actual values.
        t = dif & 0x30;

        switch (t) {
        case 0x00: s += " Instantaneous value"; break;
        case 0x10: s += " Maximum value"; break;
        case 0x20: s += " Minimum value"; break;
        case 0x30: s+= " Value during error state"; break;
        default: s += "?"; break;
        }
    }
    if (dif & 0x40) {
        // This is the lsb of the storage nr.
        s += " storagenr=1";
    }
    return s;
}

MeasurementType difMeasurementType(int dif)
{
    int t = dif & 0x30;
    switch (t) {
    case 0x00: return MeasurementType::Instantaneous;
    case 0x10: return MeasurementType::Maximum;
    case 0x20: return MeasurementType::Minimum;
    case 0x30: return MeasurementType::AtError;
    }
    assert(0);
}

string vifType(int vif)
{
    int extension = vif & 0x80;
    int t = vif & 0x7f;

    if (extension) {
        switch(vif) {
        case 0xfb: return "First extension of VIF-codes";
        case 0xfd: return "Second extension of VIF-codes";
        case 0xef: return "Reserved extension";
        case 0xff: return "Vendor extension";
        }
    }

    switch (t) {
    case 0x00: return "Energy mWh";
    case 0x01: return "Energy 10⁻² Wh";
    case 0x02: return "Energy 10⁻¹ Wh";
    case 0x03: return "Energy Wh";
    case 0x04: return "Energy 10¹ Wh";
    case 0x05: return "Energy 10² Wh";
    case 0x06: return "Energy kWh";
    case 0x07: return "Energy 10⁴ Wh";

    case 0x08: return "Energy J";
    case 0x09: return "Energy 10¹ J";
    case 0x0A: return "Energy 10² J";
    case 0x0B: return "Energy kJ";
    case 0x0C: return "Energy 10⁴ J";
    case 0x0D: return "Energy 10⁵ J";
    case 0x0E: return "Energy MJ";
    case 0x0F: return "Energy 10⁷ J";

    case 0x10: return "Volume cm³";
    case 0x11: return "Volume 10⁻⁵ m³";
    case 0x12: return "Volume 10⁻⁴ m³";
    case 0x13: return "Volume l";
    case 0x14: return "Volume 10⁻² m³";
    case 0x15: return "Volume 10⁻¹ m³";
    case 0x16: return "Volume m³";
    case 0x17: return "Volume 10¹ m³";

    case 0x18: return "Mass g";
    case 0x19: return "Mass 10⁻² kg";
    case 0x1A: return "Mass 10⁻¹ kg";
    case 0x1B: return "Mass kg";
    case 0x1C: return "Mass 10¹ kg";
    case 0x1D: return "Mass 10² kg";
    case 0x1E: return "Mass t";
    case 0x1F: return "Mass 10⁴ kg";

    case 0x20: return "On time seconds";
    case 0x21: return "On time minutes";
    case 0x22: return "On time hours";
    case 0x23: return "On time days";

    case 0x24: return "Operating time seconds";
    case 0x25: return "Operating time minutes";
    case 0x26: return "Operating time hours";
    case 0x27: return "Operating time days";

    case 0x28: return "Power mW";
    case 0x29: return "Power 10⁻² W";
    case 0x2A: return "Power 10⁻¹ W";
    case 0x2B: return "Power W";
    case 0x2C: return "Power 10¹ W";
    case 0x2D: return "Power 10² W";
    case 0x2E: return "Power kW";
    case 0x2F: return "Power 10⁴ W";

    case 0x30: return "Power J/h";
    case 0x31: return "Power 10¹ J/h";
    case 0x32: return "Power 10² J/h";
    case 0x33: return "Power kJ/h";
    case 0x34: return "Power 10⁴ J/h";
    case 0x35: return "Power 10⁵ J/h";
    case 0x36: return "Power MJ/h";
    case 0x37: return "Power 10⁷ J/h";

    case 0x38: return "Volume flow cm³/h";
    case 0x39: return "Volume flow 10⁻⁵ m³/h";
    case 0x3A: return "Volume flow 10⁻⁴ m³/h";
    case 0x3B: return "Volume flow l/h";
    case 0x3C: return "Volume flow 10⁻² m³/h";
    case 0x3D: return "Volume flow 10⁻¹ m³/h";
    case 0x3E: return "Volume flow m³/h";
    case 0x3F: return "Volume flow 10¹ m³/h";

    case 0x40: return "Volume flow ext. 10⁻⁷ m³/min";
    case 0x41: return "Volume flow ext. cm³/min";
    case 0x42: return "Volume flow ext. 10⁻⁵ m³/min";
    case 0x43: return "Volume flow ext. 10⁻⁴ m³/min";
    case 0x44: return "Volume flow ext. l/min";
    case 0x45: return "Volume flow ext. 10⁻² m³/min";
    case 0x46: return "Volume flow ext. 10⁻¹ m³/min";
    case 0x47: return "Volume flow ext. m³/min";

    case 0x48: return "Volume flow ext. mm³/s";
    case 0x49: return "Volume flow ext. 10⁻⁸ m³/s";
    case 0x4A: return "Volume flow ext. 10⁻⁷ m³/s";
    case 0x4B: return "Volume flow ext. cm³/s";
    case 0x4C: return "Volume flow ext. 10⁻⁵ m³/s";
    case 0x4D: return "Volume flow ext. 10⁻⁴ m³/s";
    case 0x4E: return "Volume flow ext. l/s";
    case 0x4F: return "Volume flow ext. 10⁻² m³/s";

    case 0x50: return "Mass g/h";
    case 0x51: return "Mass 10⁻² kg/h";
    case 0x52: return "Mass 10⁻¹ kg/h";
    case 0x53: return "Mass kg/h";
    case 0x54: return "Mass 10¹ kg/h";
    case 0x55: return "Mass 10² kg/h";
    case 0x56: return "Mass t/h";
    case 0x57: return "Mass 10⁴ kg/h";

    case 0x58: return "Flow temperature 10⁻³ °C";
    case 0x59: return "Flow temperature 10⁻² °C";
    case 0x5A: return "Flow temperature 10⁻¹ °C";
    case 0x5B: return "Flow temperature °C";

    case 0x5C: return "Return temperature 10⁻³ °C";
    case 0x5D: return "Return temperature 10⁻² °C";
    case 0x5E: return "Return temperature 10⁻¹ °C";
    case 0x5F: return "Return temperature °C";

    case 0x60: return "Temperature difference mK";
    case 0x61: return "Temperature difference 10⁻² K";
    case 0x62: return "Temperature difference 10⁻¹ K";
    case 0x63: return "Temperature difference K";

    case 0x64: return "External temperature 10⁻³ °C";
    case 0x65: return "External temperature 10⁻² °C";
    case 0x66: return "External temperature 10⁻¹ °C";
    case 0x67: return "External temperature °C";

    case 0x68: return "Pressure mbar";
    case 0x69: return "Pressure 10⁻² bar";
    case 0x6A: return "Pressure 10⁻1 bar";
    case 0x6B: return "Pressure bar";

    case 0x6C: return "Date type G";
    case 0x6D: return "Date and time type";

    case 0x6E: return "Units for H.C.A.";
    case 0x6F: return "Reserved";

    case 0x70: return "Averaging duration seconds";
    case 0x71: return "Averaging duration minutes";
    case 0x72: return "Averaging duration hours";
    case 0x73: return "Averaging duration days";

    case 0x74: return "Actuality duration seconds";
    case 0x75: return "Actuality duration minutes";
    case 0x76: return "Actuality duration hours";
    case 0x77: return "Actuality duration days";

    case 0x78: return "Fabrication no";
    case 0x79: return "Enhanced identification";

    case 0x7C: return "VIF in following string (length in first byte)";
    case 0x7E: return "Any VIF";
    case 0x7F: return "Manufacturer specific";
    default: return "?";
    }
}

double vifScale(int vif)
{
    int t = vif & 0x7f;

    switch (t) {
        // wmbusmeters always returns enery as kwh
    case 0x00: return 1000000.0; // Energy mWh
    case 0x01: return 100000.0;  // Energy 10⁻² Wh
    case 0x02: return 10000.0;   // Energy 10⁻¹ Wh
    case 0x03: return 1000.0;    // Energy Wh
    case 0x04: return 100.0;     // Energy 10¹ Wh
    case 0x05: return 10.0;      // Energy 10² Wh
    case 0x06: return 1.0;       // Energy kWh
    case 0x07: return 0.1;       // Energy 10⁴ Wh

        // or wmbusmeters always returns energy as MJ
    case 0x08: return 1000000.0; // Energy J
    case 0x09: return 100000.0;  // Energy 10¹ J
    case 0x0A: return 10000.0;   // Energy 10² J
    case 0x0B: return 1000.0;    // Energy kJ
    case 0x0C: return 100.0;     // Energy 10⁴ J
    case 0x0D: return 10.0;      // Energy 10⁵ J
    case 0x0E: return 1.0;       // Energy MJ
    case 0x0F: return 0.1;       // Energy 10⁷ J

        // wmbusmeters always returns volume as m3
    case 0x10: return 1000000.0; // Volume cm³
    case 0x11: return 100000.0; // Volume 10⁻⁵ m³
    case 0x12: return 10000.0; // Volume 10⁻⁴ m³
    case 0x13: return 1000.0; // Volume l
    case 0x14: return 100.0; // Volume 10⁻² m³
    case 0x15: return 10.0; // Volume 10⁻¹ m³
    case 0x16: return 1.0; // Volume m³
    case 0x17: return 0.1; // Volume 10¹ m³

        // wmbusmeters always returns weight in kg
    case 0x18: return 1000.0; // Mass g
    case 0x19: return 100.0;  // Mass 10⁻² kg
    case 0x1A: return 10.0;   // Mass 10⁻¹ kg
    case 0x1B: return 1.0;    // Mass kg
    case 0x1C: return 0.1;    // Mass 10¹ kg
    case 0x1D: return 0.01;   // Mass 10² kg
    case 0x1E: return 0.001;  // Mass t
    case 0x1F: return 0.0001; // Mass 10⁴ kg

        // wmbusmeters always returns time in hours
    case 0x20: return 3600.0;     // On time seconds
    case 0x21: return 60.0;       // On time minutes
    case 0x22: return 1.0;        // On time hours
    case 0x23: return (1.0/24.0); // On time days

    case 0x24: return 3600.0;     // Operating time seconds
    case 0x25: return 60.0;       // Operating time minutes
    case 0x26: return 1.0;        // Operating time hours
    case 0x27: return (1.0/24.0); // Operating time days

        // wmbusmeters always returns power in kw
    case 0x28: return 1000000.0; // Power mW
    case 0x29: return 100000.0; // Power 10⁻² W
    case 0x2A: return 10000.0; // Power 10⁻¹ W
    case 0x2B: return 1000.0; // Power W
    case 0x2C: return 100.0; // Power 10¹ W
    case 0x2D: return 10.0; // Power 10² W
    case 0x2E: return 1.0; // Power kW
    case 0x2F: return 0.1; // Power 10⁴ W

        // or wmbusmeters always returns power in MJh
    case 0x30: return 1000000.0; // Power J/h
    case 0x31: return 100000.0; // Power 10¹ J/h
    case 0x32: return 10000.0; // Power 10² J/h
    case 0x33: return 1000.0; // Power kJ/h
    case 0x34: return 100.0; // Power 10⁴ J/h
    case 0x35: return 10.0; // Power 10⁵ J/h
    case 0x36: return 1.0; // Power MJ/h
    case 0x37: return 0.1; // Power 10⁷ J/h

        // wmbusmeters always returns volume flow in m3h
    case 0x38: return 1000000.0; // Volume flow cm³/h
    case 0x39: return 100000.0; // Volume flow 10⁻⁵ m³/h
    case 0x3A: return 10000.0; // Volume flow 10⁻⁴ m³/h
    case 0x3B: return 1000.0; // Volume flow l/h
    case 0x3C: return 100.0; // Volume flow 10⁻² m³/h
    case 0x3D: return 10.0; // Volume flow 10⁻¹ m³/h
    case 0x3E: return 1.0; // Volume flow m³/h
    case 0x3F: return 0.1; // Volume flow 10¹ m³/h

        // wmbusmeters always returns volume flow in m3h
    case 0x40: return 600000000.0; // Volume flow ext. 10⁻⁷ m³/min
    case 0x41: return 60000000.0; // Volume flow ext. cm³/min
    case 0x42: return 6000000.0; // Volume flow ext. 10⁻⁵ m³/min
    case 0x43: return 600000.0; // Volume flow ext. 10⁻⁴ m³/min
    case 0x44: return 60000.0; // Volume flow ext. l/min
    case 0x45: return 6000.0; // Volume flow ext. 10⁻² m³/min
    case 0x46: return 600.0; // Volume flow ext. 10⁻¹ m³/min
    case 0x47: return 60.0; // Volume flow ext. m³/min

        // this flow numbers will be small in the m3h unit, but it
        // does not matter since double stores the scale factor in its exponent.
    case 0x48: return 1000000000.0*3600; // Volume flow ext. mm³/s
    case 0x49: return 100000000.0*3600; // Volume flow ext. 10⁻⁸ m³/s
    case 0x4A: return 10000000.0*3600; // Volume flow ext. 10⁻⁷ m³/s
    case 0x4B: return 1000000.0*3600; // Volume flow ext. cm³/s
    case 0x4C: return 100000.0*3600; // Volume flow ext. 10⁻⁵ m³/s
    case 0x4D: return 10000.0*3600; // Volume flow ext. 10⁻⁴ m³/s
    case 0x4E: return 1000.0*3600; // Volume flow ext. l/s
    case 0x4F: return 100.0*3600; // Volume flow ext. 10⁻² m³/s

        // wmbusmeters always returns mass flow as kgh
    case 0x50: return 1000.0; // Mass g/h
    case 0x51: return 100.0; // Mass 10⁻² kg/h
    case 0x52: return 10.0; // Mass 10⁻¹ kg/h
    case 0x53: return 1.0; // Mass kg/h
    case 0x54: return 0.1; // Mass 10¹ kg/h
    case 0x55: return 0.01; // Mass 10² kg/h
    case 0x56: return 0.001; // Mass t/h
    case 0x57: return 0.0001; // Mass 10⁴ kg/h

        // wmbusmeters always returns temperature in °C
    case 0x58: return 1000.0; // Flow temperature 10⁻³ °C
    case 0x59: return 100.0; // Flow temperature 10⁻² °C
    case 0x5A: return 10.0; // Flow temperature 10⁻¹ °C
    case 0x5B: return 1.0; // Flow temperature °C

        // wmbusmeters always returns temperature in c
    case 0x5C: return 1000.0;  // Return temperature 10⁻³ °C
    case 0x5D: return 100.0; // Return temperature 10⁻² °C
    case 0x5E: return 10.0; // Return temperature 10⁻¹ °C
    case 0x5F: return 1.0; // Return temperature °C

        // or if Kelvin is used as a temperature, in K
        // what kind of meter cares about -273.15 °C
        // a flow pump for liquid helium perhaps?
    case 0x60: return 1000.0; // Temperature difference mK
    case 0x61: return 100.0; // Temperature difference 10⁻² K
    case 0x62: return 10.0; // Temperature difference 10⁻¹ K
    case 0x63: return 1.0; // Temperature difference K

        // wmbusmeters always returns temperature in c
    case 0x64: return 1000.0; // External temperature 10⁻³ °C
    case 0x65: return 100.0; // External temperature 10⁻² °C
    case 0x66: return 10.0; // External temperature 10⁻¹ °C
    case 0x67: return 1.0; // External temperature °C

        // wmbusmeters always returns pressure in bar
    case 0x68: return 1000.0; // Pressure mbar
    case 0x69: return 100.0; // Pressure 10⁻² bar
    case 0x6A: return 10.0; // Pressure 10⁻1 bar
    case 0x6B: return 1.0; // Pressure bar

    case 0x6C: warning("(wmbus) warning: do not scale a date type!\n"); return -1.0; // Date type G
    case 0x6E: return 1.0; // Units for H.C.A. are never scaled
    case 0x6F: warning("(wmbus) warning: do not scale a reserved type!\n"); return -1.0; // Reserved

        // wmbusmeters always returns time in hours
    case 0x70: return 3600.0; // Averaging duration seconds
    case 0x71: return 60.0; // Averaging duration minutes
    case 0x72: return 1.0; // Averaging duration hours
    case 0x73: return (1.0/24.0); // Averaging duration days

    case 0x74: return 3600.0; // Actuality duration seconds
    case 0x75: return 60.0; // Actuality duration minutes
    case 0x76: return 1.0; // Actuality duration hours
    case 0x77: return (1.0/24.0); // Actuality duration days

    case 0x78: // Fabrication no
    case 0x79: // Enhanced identification
    case 0x80: // Address

    case 0x7C: // VIF in following string (length in first byte)
    case 0x7E: // Any VIF
    case 0x7F: // Manufacturer specific

    default: warning("(wmbus) warning: type %d cannot be scaled!\n", t);
        return -1;
    }
}

string vifKey(int vif)
{
    int t = vif & 0x7f;

    switch (t) {

    case 0x00:
    case 0x01:
    case 0x02:
    case 0x03:
    case 0x04:
    case 0x05:
    case 0x06:
    case 0x07: return "energy";

    case 0x08:
    case 0x09:
    case 0x0A:
    case 0x0B:
    case 0x0C:
    case 0x0D:
    case 0x0E:
    case 0x0F: return "energy";

    case 0x10:
    case 0x11:
    case 0x12:
    case 0x13:
    case 0x14:
    case 0x15:
    case 0x16:
    case 0x17: return "volume";

    case 0x18:
    case 0x19:
    case 0x1A:
    case 0x1B:
    case 0x1C:
    case 0x1D:
    case 0x1E:
    case 0x1F: return "mass";

    case 0x20:
    case 0x21:
    case 0x22:
    case 0x23: return "on_time";

    case 0x24:
    case 0x25:
    case 0x26:
    case 0x27: return "operating_time";

    case 0x28:
    case 0x29:
    case 0x2A:
    case 0x2B:
    case 0x2C:
    case 0x2D:
    case 0x2E:
    case 0x2F: return "power";

    case 0x30:
    case 0x31:
    case 0x32:
    case 0x33:
    case 0x34:
    case 0x35:
    case 0x36:
    case 0x37: return "power";

    case 0x38:
    case 0x39:
    case 0x3A:
    case 0x3B:
    case 0x3C:
    case 0x3D:
    case 0x3E:
    case 0x3F: return "volume_flow";

    case 0x40:
    case 0x41:
    case 0x42:
    case 0x43:
    case 0x44:
    case 0x45:
    case 0x46:
    case 0x47: return "volume_flow_ext";

    case 0x48:
    case 0x49:
    case 0x4A:
    case 0x4B:
    case 0x4C:
    case 0x4D:
    case 0x4E:
    case 0x4F: return "volume_flow_ext";

    case 0x50:
    case 0x51:
    case 0x52:
    case 0x53:
    case 0x54:
    case 0x55:
    case 0x56:
    case 0x57: return "mass_flow";

    case 0x58:
    case 0x59:
    case 0x5A:
    case 0x5B: return "flow_temperature";

    case 0x5C:
    case 0x5D:
    case 0x5E:
    case 0x5F: return "return_temperature";

    case 0x60:
    case 0x61:
    case 0x62:
    case 0x63: return "temperature_difference";

    case 0x64:
    case 0x65:
    case 0x66:
    case 0x67: return "external_temperature";

    case 0x68:
    case 0x69:
    case 0x6A:
    case 0x6B: return "pressure";

    case 0x6C: return "date"; // Date type G
    case 0x6E: return "hca"; // Units for H.C.A.
    case 0x6F: return "reserved"; // Reserved

    case 0x70:
    case 0x71:
    case 0x72:
    case 0x73: return "average_duration";

    case 0x74:
    case 0x75:
    case 0x76:
    case 0x77: return "actual_duration";

    case 0x78: return "fabrication_no"; // Fabrication no
    case 0x79: return "enhanced_identification"; // Enhanced identification

    case 0x7C: // VIF in following string (length in first byte)
    case 0x7E: // Any VIF
    case 0x7F: // Manufacturer specific

    default: warning("(wmbus) warning: generic type %d cannot be scaled!\n", t);
        return "unknown";
    }
}

string vifUnit(int vif)
{
    int t = vif & 0x7f;

    switch (t) {

    case 0x00:
    case 0x01:
    case 0x02:
    case 0x03:
    case 0x04:
    case 0x05:
    case 0x06:
    case 0x07: return "kwh";

    case 0x08:
    case 0x09:
    case 0x0A:
    case 0x0B:
    case 0x0C:
    case 0x0D:
    case 0x0E:
    case 0x0F: return "MJ";

    case 0x10:
    case 0x11:
    case 0x12:
    case 0x13:
    case 0x14:
    case 0x15:
    case 0x16:
    case 0x17: return "m3";

    case 0x18:
    case 0x19:
    case 0x1A:
    case 0x1B:
    case 0x1C:
    case 0x1D:
    case 0x1E:
    case 0x1F: return "kg";

    case 0x20:
    case 0x21:
    case 0x22:
    case 0x23:
    case 0x24:
    case 0x25:
    case 0x26:
    case 0x27: return "h";

    case 0x28:
    case 0x29:
    case 0x2A:
    case 0x2B:
    case 0x2C:
    case 0x2D:
    case 0x2E:
    case 0x2F: return "kw";

    case 0x30:
    case 0x31:
    case 0x32:
    case 0x33:
    case 0x34:
    case 0x35:
    case 0x36:
    case 0x37: return "MJ";

    case 0x38:
    case 0x39:
    case 0x3A:
    case 0x3B:
    case 0x3C:
    case 0x3D:
    case 0x3E:
    case 0x3F: return "m3/h";

    case 0x40:
    case 0x41:
    case 0x42:
    case 0x43:
    case 0x44:
    case 0x45:
    case 0x46:
    case 0x47: return "m3/h";

    case 0x48:
    case 0x49:
    case 0x4A:
    case 0x4B:
    case 0x4C:
    case 0x4D:
    case 0x4E:
    case 0x4F: return "m3/h";

    case 0x50:
    case 0x51:
    case 0x52:
    case 0x53:
    case 0x54:
    case 0x55:
    case 0x56:
    case 0x57: return "kg/h";

    case 0x58:
    case 0x59:
    case 0x5A:
    case 0x5B: return "c";

    case 0x5C:
    case 0x5D:
    case 0x5E:
    case 0x5F: return "c";

    case 0x60:
    case 0x61:
    case 0x62:
    case 0x63: return "k";

    case 0x64:
    case 0x65:
    case 0x66:
    case 0x67: return "c";

    case 0x68:
    case 0x69:
    case 0x6A:
    case 0x6B: return "bar";

    case 0x6C: return ""; // Date type G
    case 0x6D: return ""; // ??
    case 0x6E: return ""; // Units for H.C.A.
    case 0x6F: return ""; // Reserved

    case 0x70:
    case 0x71:
    case 0x72:
    case 0x73: return "h";

    case 0x74:
    case 0x75:
    case 0x76:
    case 0x77: return "h";

    case 0x78: return ""; // Fabrication no
    case 0x79: return ""; // Enhanced identification

    case 0x7C: // VIF in following string (length in first byte)
    case 0x7E: // Any VIF
    case 0x7F: // Manufacturer specific

    default: warning("(wmbus) warning: generic type %d cannot be scaled!\n", t);
        return "unknown";
    }
}

const char *timeNN(int nn) {
    switch (nn) {
    case 0: return "second(s)";
    case 1: return "minute(s)";
    case 2: return "hour(s)";
    case 3: return "day(s)";
    }
    return "?";
}

const char *timePP(int nn) {
    switch (nn) {
    case 0: return "hour(s)";
    case 1: return "day(s)";
    case 2: return "month(s)";
    case 3: return "year(s)";
    }
    return "?";
}

string vif_FD_ExtensionType(uchar dif, uchar vif, uchar vife)
{
    if ((vife & 0x7c) == 0x00) {
        int nn = vife & 0x03;
        string s;
        strprintf(s, "Credit of 10^%d of the nominal local legal currency units", nn-3);
        return s;
    }

    if ((vife & 0x7c) == 0x04) {
        int nn = vife & 0x03;
        string s;
        strprintf(s, "Debit of 10^%d of the nominal local legal currency units", nn-3);
        return s;
    }

    if ((vife & 0x7f) == 0x08) {
        return "Access Number (transmission count)";
    }

    if ((vife & 0x7f) == 0x09) {
        return "Medium (as in fixed header)";
    }

    if ((vife & 0x7f) == 0x0a) {
        return "Manufacturer (as in fixed header)";
    }

    if ((vife & 0x7f) == 0x0b) {
        return "Parameter set identification";
    }

    if ((vife & 0x7f) == 0x0c) {
        return "Model/Version";
    }

    if ((vife & 0x7f) == 0x0d) {
        return "Hardware version #";
    }

    if ((vife & 0x7f) == 0x0e) {
        return "Firmware version #";
    }

    if ((vife & 0x7f) == 0x0f) {
        return "Software version #";
    }

    if ((vife & 0x7f) == 0x10) {
        return "Customer location";
    }

    if ((vife & 0x7f) == 0x11) {
        return "Customer";
    }

    if ((vife & 0x7f) == 0x12) {
        return "Access Code User";
    }

    if ((vife & 0x7f) == 0x13) {
        return "Access Code Operator";
    }

    if ((vife & 0x7f) == 0x14) {
        return "Access Code System Operator";
    }

    if ((vife & 0x7f) == 0x15) {
        return "Access Code Developer";
    }

    if ((vife & 0x7f) == 0x16) {
        return "Password";
    }

    if ((vife & 0x7f) == 0x17) {
        return "Error flags (binary)";
    }

    if ((vife & 0x7f) == 0x18) {
        return "Error mask";
    }

    if ((vife & 0x7f) == 0x19) {
        return "Reserved";
    }

    if ((vife & 0x7f) == 0x1a) {
        return "Digital Output (binary)";
    }

    if ((vife & 0x7f) == 0x1b) {
        return "Digital Input (binary)";
    }

    if ((vife & 0x7f) == 0x1c) {
        return "Baudrate [Baud]";
    }

    if ((vife & 0x7f) == 0x1d) {
        return "Response delay time [bittimes]";
    }

    if ((vife & 0x7f) == 0x1e) {
        return "Retry";
    }

    if ((vife & 0x7f) == 0x1f) {
        return "Reserved";
    }

    if ((vife & 0x7f) == 0x20) {
        return "First storage # for cyclic storage";
    }

    if ((vife & 0x7f) == 0x21) {
        return "Last storage # for cyclic storage";
    }

    if ((vife & 0x7f) == 0x22) {
        return "Size of storage block";
    }

    if ((vife & 0x7f) == 0x23) {
        return "Reserved";
    }

    if ((vife & 0x7c) == 0x24) {
        int nn = vife & 0x03;
        string s;
        strprintf(s, "Storage interval [%s]", timeNN(nn));
        return s;
    }

    if ((vife & 0x7f) == 0x28) {
        return "Storage interval month(s)";
    }

    if ((vife & 0x7f) == 0x29) {
        return "Storage interval year(s)";
    }

    if ((vife & 0x7f) == 0x2a) {
        return "Reserved";
    }

    if ((vife & 0x7f) == 0x2b) {
        return "Reserved";
    }

    if ((vife & 0x7c) == 0x2c) {
        int nn = vife & 0x03;
        string s;
        strprintf(s, "Duration since last readout [%s]", timeNN(nn));
        return s;
    }

    if ((vife & 0x7f) == 0x30) {
        return "Start (date/time) of tariff";
    }

    if ((vife & 0x7c) == 0x30) {
        int nn = vife & 0x03;
        string s;
        // nn == 0 (seconds) is not expected here! According to m-bus spec.
        strprintf(s, "Duration of tariff [%s]", timeNN(nn));
        return s;
    }

    if ((vife & 0x7c) == 0x34) {
        int nn = vife & 0x03;
        string s;
        strprintf(s, "Period of tariff [%s]", timeNN(nn));
        return s;
    }

    if ((vife & 0x7f) == 0x38) {
        return "Period of tariff months(s)";
    }

    if ((vife & 0x7f) == 0x39) {
        return "Period of tariff year(s)";
    }

    if ((vife & 0x7f) == 0x3a) {
        return "Dimensionless / no VIF";
    }

    if ((vife & 0x7f) == 0x3b) {
        return "Reserved";
    }

    if ((vife & 0x7c) == 0x3c) {
        // int xx = vife & 0x03;
        return "Reserved";
    }

    if ((vife & 0x70) == 0x40) {
        int nnnn = vife & 0x0f;
        string s;
        strprintf(s, "10^%d Volts", nnnn-9);
        return s;
    }

    if ((vife & 0x70) == 0x50) {
        int nnnn = vife & 0x0f;
        string s;
        strprintf(s, "10^%d Ampere", nnnn-12);
    }

    if ((vife & 0x7f) == 0x60) {
        return "Reset counter";
    }

    if ((vife & 0x7f) == 0x61) {
        return "Cumulation counter";
    }

    if ((vife & 0x7f) == 0x62) {
        return "Control signal";
    }

    if ((vife & 0x7f) == 0x63) {
        return "Day of week";
    }

    if ((vife & 0x7f) == 0x64) {
        return "Week number";
    }

    if ((vife & 0x7f) == 0x65) {
        return "Time point of day change";
    }

    if ((vife & 0x7f) == 0x66) {
        return "State of parameter activation";
    }

    if ((vife & 0x7f) == 0x67) {
        return "Special supplier information";
    }

    if ((vife & 0x7c) == 0x68) {
        int pp = vife & 0x03;
        string s;
        strprintf(s, "Duration since last cumulation [%s]", timePP(pp));
        return s;
    }

    if ((vife & 0x7c) == 0x6c) {
        int pp = vife & 0x03;
        string s;
        strprintf(s, "Operating time battery [%s]", timePP(pp));
        return s;
    }

    if ((vife & 0x7f) == 0x70) {
        return "Date and time of battery change";
    }

    if ((vife & 0x7f) >= 0x71) {
        return "Reserved";
    }
    return "?";
}

string vif_FB_ExtensionType(uchar dif, uchar vif, uchar vife)
{
    if ((vife & 0x7e) == 0x00) {
        int n = vife & 0x01;
        string s;
        strprintf(s, "10^%d MWh", n-1);
        return s;
    }

    if (((vife & 0x7e) == 0x02) ||
        ((vife & 0x7c) == 0x04)) {
        return "Reserved";
    }

    if ((vife & 0x7e) == 0x08) {
        int n = vife & 0x01;
        string s;
        strprintf(s, "10^%d GJ", n-1);
        return s;
    }

    if ((vife & 0x7e) == 0x0a ||
        (vife & 0x7c) == 0x0c) {
        return "Reserved";
    }

    if ((vife & 0x7e) == 0x10) {
        int n = vife & 0x01;
        string s;
        strprintf(s, "10^%d m3", n+2);
        return s;
    }

    if ((vife & 0x7e) == 0x12 ||
        (vife & 0x7c) == 0x14) {
        return "Reserved";
    }

    if ((vife & 0x7e) == 0x18) {
        int n = vife & 0x01;
        string s;
        strprintf(s, "10^%d ton", n+2);
        return s;
    }

    if ( (vif & 0x7e) >= 0x1a && (vif & 0x7e) <= 0x20) {
        return "Reserved";
    }

    if ((vife & 0x7f) == 0x21) {
        return "0.1 feet^3";
    }

    if ((vife & 0x7f) == 0x22) {
        return "0.1 american gallon";
    }

    if ((vife & 0x7f) == 0x23) {
        return "american gallon";
    }

    if ((vife & 0x7f) == 0x24) {
        return "0.001 american gallon/min";
    }

    if ((vife & 0x7f) == 0x25) {
        return "american gallon/min";
    }

    if ((vife & 0x7f) == 0x26) {
        return "american gallon/h";
    }

    if ((vife & 0x7f) == 0x27) {
        return "Reserved";
    }

    if ((vife & 0x7f) == 0x20) {
        return "Volume feet";
    }

    if ((vife & 0x7f) == 0x21) {
        return "Volume 0.1 feet";
    }

    if ((vife & 0x7e) == 0x28) {
        // Come again? A unit of 1MW...do they intend to use m-bus to track the
        // output from a nuclear power plant?
        int n = vife & 0x01;
        string s;
        strprintf(s, "10^%d MW", n-1);
        return s;
    }

    if ((vife & 0x7f) == 0x29 ||
        (vife & 0x7c) == 0x2c) {
        return "Reserved";
    }

    if ((vife & 0x7e) == 0x30) {
        int n = vife & 0x01;
        string s;
        strprintf(s, "10^%d GJ/h", n-1);
        return s;
    }

    if ((vife & 0x7f) >= 0x32 && (vife & 0x7c) <= 0x57) {
        return "Reserved";
    }

    if ((vife & 0x7c) == 0x58) {
        int nn = vife & 0x03;
        string s;
        strprintf(s, "Flow temperature 10^%d Fahrenheit", nn-3);
        return s;
    }

    if ((vife & 0x7c) == 0x5c) {
        int nn = vife & 0x03;
        string s;
        strprintf(s, "Return temperature 10^%d Fahrenheit", nn-3);
        return s;
    }

    if ((vife & 0x7c) == 0x60) {
        int nn = vife & 0x03;
        string s;
        strprintf(s, "Temperature difference 10^%d Fahrenheit", nn-3);
        return s;
    }

    if ((vife & 0x7c) == 0x64) {
        int nn = vife & 0x03;
        string s;
        strprintf(s, "External temperature 10^%d Fahrenheit", nn-3);
        return s;
    }

    if ((vife & 0x78) == 0x68) {
        return "Reserved";
    }

    if ((vife & 0x7c) == 0x70) {
        int nn = vife & 0x03;
        string s;
        strprintf(s, "Cold / Warm Temperature Limit 10^%d Fahrenheit", nn-3);
        return s;
    }

    if ((vife & 0x7c) == 0x74) {
        int nn = vife & 0x03;
        string s;
        strprintf(s, "Cold / Warm Temperature Limit 10^%d Celsius", nn-3);
        return s;
    }

    if ((vife & 0x78) == 0x78) {
        int nnn = vife & 0x07;
        string s;
        strprintf(s, "Cumulative count max power 10^%d W", nnn-3);
        return s;
    }

    return "?";
}

string vifeType(int dif, int vif, int vife)
{
    if (vif == 0xfb) {
        return vif_FB_ExtensionType(dif, vif, vife);
    }
    if (vif == 0xfd) {
        return vif_FD_ExtensionType(dif, vif, vife);
    }
    vife = vife & 0x7f; // Strip the bit signifying more vifes after this.
    if (vife == 0x1f) {
        return "Compact profile without register";
    }
    if (vife == 0x13) {
        return "Reverse compact profile without register";
    }
    if (vife == 0x1e) {
        return "Compact profile with register";
    }
    if (vife == 0x20) {
        return "per second";
    }
    if (vife == 0x21) {
        return "per minute";
    }
    if (vife == 0x22) {
        return "per hour";
    }
    if (vife == 0x23) {
        return "per day";
    }
    if (vife == 0x24) {
        return "per week";
    }
    if (vife == 0x25) {
        return "per month";
    }
    if (vife == 0x26) {
        return "per year";
    }
    if (vife == 0x27) {
        return "per revolution/measurement";
    }
    if (vife == 0x28) {
        return "incr per input pulse on input channel 0";
    }
    if (vife == 0x29) {
        return "incr per input pulse on input channel 1";
    }
    if (vife == 0x2a) {
        return "incr per output pulse on input channel 0";
    }
    if (vife == 0x2b) {
        return "incr per output pulse on input channel 1";
    }
    if (vife == 0x2c) {
        return "per litre";
    }
    if (vife == 0x2d) {
        return "per m3";
    }
    if (vife == 0x2e) {
        return "per kg";
    }
    if (vife == 0x2f) {
        return "per kelvin";
    }
    if (vife == 0x30) {
        return "per kWh";
    }
    if (vife == 0x31) {
        return "per GJ";
    }
    if (vife == 0x32) {
        return "per kW";
    }
    if (vife == 0x33) {
        return "per kelvin*litre";
    }
    if (vife == 0x34) {
        return "per volt";
    }
    if (vife == 0x35) {
        return "per ampere";
    }
    if (vife == 0x36) {
        return "multiplied by s";
    }
    if (vife == 0x37) {
        return "multiplied by s/V";
    }
    if (vife == 0x38) {
        return "multiplied by s/A";
    }
    if (vife == 0x39) {
        return "start date/time of a,b";
    }
    if (vife == 0x3a) {
        return "uncorrected meter unit";
    }
    if (vife == 0x3b) {
        return "forward flow";
    }
    if (vife == 0x3c) {
        return "backward flow";
    }
    if (vife == 0x3d) {
        return "reserved for non-metric unit systems";
    }
    if (vife == 0x3e) {
        return "value at base conditions c";
    }
    if (vife == 0x3f) {
        return "obis-declaration";
    }
    if (vife == 0x40) {
        return "obis-declaration";
    }
    if (vife == 0x40) {
        return "lower limit";
    }
    if (vife == 0x48) {
        return "upper limit";
    }
    if (vife == 0x41) {
        return "number of exceeds of lower limit";
    }
    if (vife == 0x49) {
        return "number of exceeds of upper limit";
    }
    if ((vife & 0x72) == 0x42) {
        string msg = "date/time of ";
        if (vife & 0x01) msg += "end ";
        else msg += "beginning ";
        msg +=" of ";
        if (vife & 0x04) msg += "last ";
        else msg += "first ";
        if (vife & 0x08) msg += "upper ";
        else msg += "lower ";
        msg += "limit exceed";
        return msg;
    }
    if ((vife & 0x70) == 0x50) {
        string msg = "duration of limit exceed ";
        if (vife & 0x04) msg += "last ";
        else msg += "first ";
        if (vife & 0x08) msg += "upper ";
        else msg += "lower ";
        int nn = vife & 0x03;
        msg += " is "+to_string(nn);
        return msg;
    }
    if ((vife & 0x78) == 0x60) {
        string msg = "duration of a,b ";
        if (vife & 0x04) msg += "last ";
        else msg += "first ";
        int nn = vife & 0x03;
        msg += " is "+to_string(nn);
        return msg;
    }
    if ((vife & 0x7B) == 0x68) {
        string msg = "value during ";
        if (vife & 0x04) msg += "upper ";
        else msg += "lower ";
        msg += "limit exceed";
        return msg;
    }
    if (vife == 0x69) {
        return "leakage values";
    }
    if (vife == 0x6d) {
        return "overflow values";
    }
    if ((vife & 0x7a) == 0x6a) {
        string msg = "date/time of a: ";
        if (vife & 0x01) msg += "end ";
        else msg += "beginning ";
        msg +=" of ";
        if (vife & 0x04) msg += "last ";
        else msg += "first ";
        if (vife & 0x08) msg += "upper ";
        else msg += "lower ";
        return msg;
    }
    if ((vife & 0x78) == 0x70) {
        int nnn = vife & 0x07;
        return "multiplicative correction factor: 10^"+to_string(nnn-6);
    }
    if ((vife & 0x78) == 0x78) {
        int nn = vife & 0x03;
        return "additive correction constant: unit of VIF * 10^"+to_string(nn-3);
    }
    if (vife == 0x7c) {
        return "extension of combinable vife";
    }
    if (vife == 0x7d) {
        return "multiplicative correction factor for value";
    }
    if (vife == 0x7e) {
        return "future value";
    }
    if (vif == 0x7f) {
        return "manufacturer specific";
    }
    return "?";
}

double toDoubleFromBytes(vector<uchar> &bytes, int len) {
    double d = 0;
    for (int i=0; i<len; ++i) {
        double x = bytes[i];
        d += x*(256^i);
    }
    return d;
}

double toDoubleFromBCD(vector<uchar> &bytes, int len) {
    double d = 0;
    for (int i=0; i<len; ++i) {
        double x = bytes[i]&0xf;
        d += x*(10^(i*2));
        double xx = bytes[i]>>4;
        d += xx*(10^(1+i*2));
    }
    return d;
}

double dataAsDouble(int dif, int vif, int vife, string data)
{
    vector<uchar> bytes;
    hex2bin(data, &bytes);

    int t = dif & 0x0f;
    switch (t) {
    case 0x0: return 0.0;
    case 0x1: return toDoubleFromBytes(bytes, 1);
    case 0x2: return toDoubleFromBytes(bytes, 2);
    case 0x3: return toDoubleFromBytes(bytes, 3);
    case 0x4: return toDoubleFromBytes(bytes, 4);
    case 0x5: return -1;  //  How is REAL stored?
    case 0x6: return toDoubleFromBytes(bytes, 6);
        // Note that for 64 bit data, storing it into a double might lose precision
        // since the mantissa is less than 64 bit. It is unlikely that anyone
        // really needs true 64 bit precision in their measurements from a physical meter though.
    case 0x7: return toDoubleFromBytes(bytes, 8);
    case 0x8: return -1; // Selection for Readout?
    case 0x9: return toDoubleFromBCD(bytes, 1);
    case 0xA: return toDoubleFromBCD(bytes, 2);
    case 0xB: return toDoubleFromBCD(bytes, 3);
    case 0xC: return toDoubleFromBCD(bytes, 4);
    case 0xD: return -1; // variable length
    case 0xE: return toDoubleFromBCD(bytes, 6);
    case 0xF: return -1; // Special Functions
    }
    return -1;
}

uint64_t dataAsUint64(int dif, int vif, int vife, string data)
{
    vector<uchar> bytes;
    hex2bin(data, &bytes);

    int t = dif & 0x0f;
    switch (t) {
    case 0x0: return 0.0;
    case 0x1: return toDoubleFromBytes(bytes, 1);
    case 0x2: return toDoubleFromBytes(bytes, 2);
    case 0x3: return toDoubleFromBytes(bytes, 3);
    case 0x4: return toDoubleFromBytes(bytes, 4);
    case 0x5: return -1;  //  How is REAL stored?
    case 0x6: return toDoubleFromBytes(bytes, 6);
        // Note that for 64 bit data, storing it into a double might lose precision
        // since the mantissa is less than 64 bit. It is unlikely that anyone
        // really needs true 64 bit precision in their measurements from a physical meter though.
    case 0x7: return toDoubleFromBytes(bytes, 8);
    case 0x8: return -1; // Selection for Readout?
    case 0x9: return toDoubleFromBCD(bytes, 1);
    case 0xA: return toDoubleFromBCD(bytes, 2);
    case 0xB: return toDoubleFromBCD(bytes, 3);
    case 0xC: return toDoubleFromBCD(bytes, 4);
    case 0xD: return -1; // variable length
    case 0xE: return toDoubleFromBCD(bytes, 6);
    case 0xF: return -1; // Special Functions
    }
    return -1;
}

string formatData(int dif, int vif, int vife, string data)
{
    string r;

    int t = vif & 0x7f;
    if (t >= 0 && t <= 0x77 && !(t >= 0x6c && t<=0x6f)) {
        // These are vif codes with an understandable key and unit.
        double val = dataAsDouble(dif, vif, vife, data);
        strprintf(r, "%d", val);
        return r;
    }

    return data;
}

string linkModeName(LinkMode link_mode)
{

    for (auto& s : link_modes_) {
        if (link_mode == s.mode) {
            return s.name;
        }
    }
    return "UnknownLinkMode";
}

string measurementTypeName(MeasurementType mt)
{
    switch (mt) {
    case MeasurementType::Instantaneous: return "instantaneous";
    case MeasurementType::Maximum: return "maximum";
    case MeasurementType::Minimum: return "minimum";
    case MeasurementType::AtError: return "aterror";
    case MeasurementType::Unknown: return "unknown";
    }
    assert(0);
}

WMBus::~WMBus() {
}
