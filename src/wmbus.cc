/*
 Copyright (C) 2017-2020 Fredrik Öhrström

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

#include"aescmac.h"
#include"timings.h"
#include"meters.h"
#include"wmbus.h"
#include"wmbus_utils.h"
#include"dvparser.h"
#include<assert.h>
#include<semaphore.h>
#include<stdarg.h>
#include<string.h>
#include<sys/stat.h>
#include<sys/types.h>
#include<unistd.h>

struct LinkModeInfo
{
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
    char *saveptr {};
    const char *tok = strtok_r(buf, ",", &saveptr);
    while (tok != NULL)
    {
        LinkMode lm = isLinkMode(tok);
        if (lm == LinkMode::UNKNOWN)
        {
            error("(wmbus) not a valid link mode: %s\n", tok);
        }
        lms.addLinkMode(lm);
        tok = strtok_r(NULL, ",", &saveptr);
    }
    return lms;
}

bool isValidLinkModes(string m)
{
    LinkModeSet lms;
    char buf[m.length()+1];
    strcpy(buf, m.c_str());
    char *saveptr {};
    const char *tok = strtok_r(buf, ",", &saveptr);
    while (tok != NULL)
    {
        LinkMode lm = isLinkMode(tok);
        if (lm == LinkMode::UNKNOWN)
        {
            return false;
        }
        lms.addLinkMode(lm);
        tok = strtok_r(NULL, ",", &saveptr);
    }
    return true;
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

void Telegram::print()
{
    uchar a=0, b=0, c=0, d=0;
    if (dll_id.size() >= 4)
    {
        a = dll_id[0];
        b = dll_id[1];
        c = dll_id[2];
        d = dll_id[3];
    }
    notice("Received telegram from: %02x%02x%02x%02x\n", a,b,c,d);
    notice("          manufacturer: (%s) %s (0x%02x)\n",
           manufacturerFlag(dll_mfct).c_str(),
           manufacturer(dll_mfct).c_str(),
           dll_mfct);
    notice("                  type: %s (0x%02x)\n", mediaType(dll_type).c_str(), dll_type);

    notice("                   ver: 0x%02x\n", dll_version);

    if (about.device != "")
    {
        notice("                device: %s\n", about.device.c_str());
        notice("                  rssi: %d dBm\n", about.rssi_dbm);
    }
    string possible_drivers = autoDetectPossibleDrivers();
    notice("                driver: %s\n", possible_drivers.c_str());
}

void Telegram::printDLL()
{
    string possible_drivers = autoDetectPossibleDrivers();

    string man = manufacturerFlag(dll_mfct);
    verbose("(telegram) DLL L=%02x C=%02x (%s) M=%04x (%s) A=%02x%02x%02x%02x VER=%02x TYPE=%02x (%s) (driver %s) DEV=%s RSSI=%d\n",
            dll_len,
            dll_c, cType(dll_c).c_str(),
            dll_mfct,
            man.c_str(),
            dll_id[0], dll_id[1], dll_id[2], dll_id[3],
            dll_version,
            dll_type,
            mediaType(dll_type).c_str(),
            possible_drivers.c_str(),
            about.device.c_str(),
            about.rssi_dbm);
}

void Telegram::printELL()
{
    if (ell_ci == 0) return;

    string ell_cc_info = ccType(ell_cc);
    verbose("(telegram) ELL CI=%02x CC=%02x (%s) ACC=%02x",
            ell_ci, ell_cc, ell_cc_info.c_str(), ell_acc);

    if (ell_ci == 0x8d || ell_ci == 0x8f)
    {
        string ell_sn_info = toStringFromELLSN(ell_sn);

        verbose(" SN=%02x%02x%02x%02x (%s) CRC=%02x%02x",
                ell_sn_b[0], ell_sn_b[1], ell_sn_b[2], ell_sn_b[3], ell_sn_info.c_str(),
                ell_pl_crc_b[0], ell_pl_crc_b[1]);
    }
    if (ell_ci == 0x8e || ell_ci == 0x8f)
    {
        string man = manufacturerFlag(ell_mfct);
        verbose(" M=%02x%02x (%s) ID=%02x%02x%02x%02x",
                ell_mfct_b[0], ell_mfct_b[1], man.c_str(),
                ell_id_b[0], ell_id_b[1], ell_id_b[2], ell_id_b[3]);
    }
    verbose("\n");
}

void Telegram::printNWL()
{
    if (nwl_ci == 0) return;

    verbose("(telegram) NWL CI=%02x\n",
            nwl_ci);
}

void Telegram::printAFL()
{
    if (afl_ci == 0) return;

    verbose("(telegram) AFL CI=%02x\n",
            afl_ci);

}

void Telegram::printTPL()
{
    if (tpl_ci == 0) return;

    verbose("(telegram) TPL CI=%02x", tpl_ci);

    if (tpl_ci == 0x7a || tpl_ci == 0x72)
    {
        string tpl_cfg_info = toStringFromTPLConfig(tpl_cfg);
        verbose(" ACC=%02x STS=%02x CFG=%04x (%s)",
                tpl_acc, tpl_sts, tpl_cfg, tpl_cfg_info.c_str());
    }

    if (tpl_ci == 0x72)
    {
        string info = mediaType(tpl_type);
        verbose(" ID=%02x%02x%02x%02x MFT=%02x%02x VER=%02x TYPE=%02x (%s)",
                tpl_id_b[0], tpl_id_b[1], tpl_id_b[2], tpl_id_b[3],
                tpl_mfct_b[0], tpl_mfct_b[1],
                tpl_version, tpl_type, info.c_str());
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

    // Techem FHKV.
    case 0x80: return "Heat Cost Allocator"; // FHKV data ii/iii

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

    // Techem FHKV.
    case 0x80: return "Heat Cost Allocator"; // FHKV data ii/iii

    // Techem Vario 4 Typ 4.5.1 manufacturer specific codes:
    case 0xC3: return "heat";

    }
    return "Unknown";
}

/*
    X(0x72, TPL_72,  "TPL: APL follows", return "EN 13757-3 Application Layer (long tplh)";
    case 0x73: return "EN 13757-3 Application Layer with Compact frame and long Transport Layer";
*/

#define LIST_OF_CI_FIELDS \
    X(0x51, TPL_51,  "TPL: APL follows", 0, CI_TYPE::TPL, "")       \
    X(0x72, TPL_72,  "TPL: long header APL follows", 0, CI_TYPE::TPL, "") \
    X(0x78, TPL_78,  "TPL: no header APL follows", 0, CI_TYPE::TPL, "") \
    X(0x79, TPL_79,  "TPL: compact APL follows", 0, CI_TYPE::TPL, "") \
    X(0x7A, TPL_7A,  "TPL: short header APL follows", 0, CI_TYPE::TPL, "") \
    X(0x8C, ELL_I,   "ELL: I",    2, CI_TYPE::ELL, "CC, ACC") \
    X(0x8D, ELL_II,  "ELL: II",   8, CI_TYPE::ELL, "CC, ACC, SN, Payload CRC") \
    X(0x8E, ELL_III, "ELL: III", 10, CI_TYPE::ELL, "CC, ACC, M2, A2") \
    X(0x8F, ELL_IV,  "ELL: IV",  16, CI_TYPE::ELL, "CC, ACC, M2, A2, SN, Payload CRC") \
    X(0x86, ELL_V,   "ELL: V",   -1, CI_TYPE::ELL, "Variable length") \
    X(0x90, AFL,     "AFL", 10, CI_TYPE::AFL, "") \
    X(0xA0, MFCT_SPECIFIC_A0, "MFCT SPECIFIC", 0, CI_TYPE::TPL, "") \
    X(0xA1, MFCT_SPECIFIC_A1, "MFCT SPECIFIC", 0, CI_TYPE::TPL, "") \
    X(0xA2, MFCT_SPECIFIC_A2, "MFCT SPECIFIC", 0, CI_TYPE::TPL, "")

enum CI_Field_Values {
#define X(val,name,cname,len,citype,explain) name = val,
LIST_OF_CI_FIELDS
#undef X
};

bool isCiFieldOfType(int ci_field, CI_TYPE type)
{
#define X(val,name,cname,len,citype,explain) if (ci_field == val && type == citype) return true;
LIST_OF_CI_FIELDS
#undef X
    return false;
}

int ciFieldLength(int ci_field)
{
#define X(val,name,cname,len,citype,explain) if (ci_field == val) return len;
LIST_OF_CI_FIELDS
#undef X
    return -2;
}

string ciType(int ci_field)
{
    if (ci_field >= 0xA0 && ci_field <= 0xB7) {
        return "Mfct specific";
    }
    if (ci_field >= 0x00 && ci_field <= 0x1f) {
        return "Reserved for DLMS";
    }

    if (ci_field >= 0x20 && ci_field <= 0x4f) {
        return "Reserved";
    }

    switch (ci_field) {
    case 0x50: return "Application reset or select to device (no tplh)";
    case 0x51: return "Command to device (no tplh)"; // Only for mbus, not wmbus.
    case 0x52: return "Selection of device (no tplh)";
    case 0x53: return "Application reset or select to device (long tplh)";
    case 0x54: return "Request of selected application to device (no tplh)";
    case 0x55: return "Request of selected application to device (long tplh)";
    case 0x56: return "Reserved";
    case 0x57: return "Reserved";
    case 0x58: return "Reserved";
    case 0x59: return "Reserved";
    case 0x5a: return "Command to device (short tplh)";
    case 0x5b: return "Command to device (long tplh)";
    case 0x5c: return "Sync action (no tplh)";
    case 0x5d: return "Reserved";
    case 0x5e: return "Reserved";
    case 0x5f: return "Specific usage";
    case 0x60: return "COSEM Data sent by the Readout device to the meter (long tplh)";
    case 0x61: return "COSEM Data sent by the Readout device to the meter (short tplh)";
    case 0x62: return "?";
    case 0x63: return "?";
    case 0x64: return "Reserved for OBIS-based Data sent by the Readout device to the meter (long tplh)";
    case 0x65: return "Reserved for OBIS-based Data sent by the Readout device to the meter (short tplh)";
    case 0x66: return "Response of selected application from device (no tplh)";
    case 0x67: return "Response of selected application from device (short tplh)";
    case 0x68: return "Response of selected application from device (long tplh)";
    case 0x69: return "EN 13757-3 Application Layer with Format frame (no tplh)";
    case 0x6A: return "EN 13757-3 Application Layer with Format frame (short tplh)";
    case 0x6B: return "EN 13757-3 Application Layer with Format frame (long tplh)";
    case 0x6C: return "Clock synchronisation (absolute) (long tplh)";
    case 0x6D: return "Clock synchronisation (relative) (long tplh)";
    case 0x6E: return "Application error from device (short tplh)";
    case 0x6F: return "Application error from device (long tplh)";
    case 0x70: return "Application error from device without Transport Layer";
    case 0x71: return "Reserved for Alarm Report";
    case 0x72: return "EN 13757-3 Application Layer (long tplh)";
    case 0x73: return "EN 13757-3 Application Layer with Compact frame and long Transport Layer";
    case 0x74: return "Alarm from device (short tplh)";
    case 0x75: return "Alarm from device (long tplh)";
    case 0x76: return "?";
    case 0x77: return "?";
    case 0x78: return "EN 13757-3 Application Layer (no tplh)";
    case 0x79: return "EN 13757-3 Application Layer with Compact frame (no tplh)";
    case 0x7A: return "EN 13757-3 Application Layer (short tplh)";
    case 0x7B: return "EN 13757-3 Application Layer with Compact frame (short tplh)";
    case 0x7C: return "COSEM Application Layer (long tplh)";
    case 0x7D: return "COSEM Application Layer (short tplh)";
    case 0x7E: return "Reserved for OBIS-based Application Layer (long tplh)";
    case 0x7F: return "Reserved for OBIS-based Application Layer (short tplh)";
    case 0x80: return "EN 13757-3 Transport Layer (long tplh) from other device to the meter";

    case 0x81: return "Network Layer data";
    case 0x82: return "Network management data to device (short tplh)";
    case 0x83: return "Network Management data to device (no tplh)";
    case 0x84: return "Transport layer to device (compact frame) (long tplh)";
    case 0x85: return "Transport layer to device (format frame) (long tplh)";
    case 0x86: return "Extended Link Layer V (variable length)";
    case 0x87: return "Network management data from device (long tplh)";
    case 0x88: return "Network management data from device (short tplh)";
    case 0x89: return "Network management data from device (no tplh)";
    case 0x8A: return "EN 13757-3 Transport Layer (short tplh) from the meter to the other device"; // No application layer, e.g. ACK
    case 0x8B: return "EN 13757-3 Transport Layer (long tplh) from the meter to the other device"; // No application layer, e.g. ACK

    case 0x8C: return "ELL: Extended Link Layer I (2 Byte)"; // CC, ACC
    case 0x8D: return "ELL: Extended Link Layer II (8 Byte)"; // CC, ACC, SN, Payload CRC
    case 0x8E: return "ELL: Extended Link Layer III (10 Byte)"; // CC, ACC, M2, A2
    case 0x8F: return "ELL: Extended Link Layer IV (16 Byte)"; // CC, ACC, M2, A2, SN, Payload CRC

    case 0x90: return "AFL: Authentication and Fragmentation Sublayer";
    case 0x91: return "Reserved";
    case 0x92: return "Reserved";
    case 0x93: return "Reserved";
    case 0x94: return "Reserved";
    case 0x95: return "Reserved";
    case 0x96: return "Reserved";
    case 0x97: return "Reserved";
    case 0x98: return "?";
    case 0x99: return "?";

    case 0xB8: return "Set baud rate to 300";
    case 0xB9: return "Set baud rate to 600";
    case 0xBA: return "Set baud rate to 1200";
    case 0xBB: return "Set baud rate to 2400";
    case 0xBC: return "Set baud rate to 4800";
    case 0xBD: return "Set baud rate to 9600";
    case 0xBE: return "Set baud rate to 19200";
    case 0xBF: return "Set baud rate to 38400";
    case 0xC0: return "Image transfer to device (long tplh)";
    case 0xC1: return "Image transfer from device (short tplh)";
    case 0xC2: return "Image transfer from device (long tplh)";
    case 0xC3: return "Security info transfer to device (long tplh)";
    case 0xC4: return "Security info transfer from device (short tplh)";
    case 0xC5: return "Security info transfer from device (long tplh)";
    }
    return "?";
}

void Telegram::addExplanationAndIncrementPos(vector<uchar>::iterator &pos, int len, const char* fmt, ...)
{
    char buf[1024];
    buf[1023] = 0;

    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, 1023, fmt, args);
    va_end(args);

    explanations.push_back({parsed.size(), buf});
    parsed.insert(parsed.end(), pos, pos+len);
    pos += len;
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

bool expectedMore(int line)
{
    verbose("(wmbus) parser expected more data! (%d)\n", line);
    return false;
}

bool Telegram::parseDLL(vector<uchar>::iterator &pos)
{
    int remaining = distance(pos, frame.end());
    if (remaining == 0) return expectedMore(__LINE__);

    debug("(wmbus) parseDLL @%d %d\n", distance(frame.begin(), pos), remaining);
    dll_len = *pos;
    if (remaining < dll_len) return expectedMore(__LINE__);
    addExplanationAndIncrementPos(pos, 1, "%02x length (%d bytes)", dll_len, dll_len);

    dll_c = *pos;
    addExplanationAndIncrementPos(pos, 1, "%02x dll-c (%s)", dll_c, cType(dll_c).c_str());

    dll_mfct_b[0] = *(pos+0);
    dll_mfct_b[1] = *(pos+1);
    dll_mfct = dll_mfct_b[1] <<8 | dll_mfct_b[0];
    string man = manufacturerFlag(dll_mfct);
    addExplanationAndIncrementPos(pos, 2, "%02x%02x dll-mfct (%s)",
                                  dll_mfct_b[0], dll_mfct_b[1], man.c_str());

    dll_a.resize(6);
    dll_id.resize(4);
    for (int i=0; i<6; ++i)
    {
        dll_a[i] = *(pos+i);
        if (i<4)
        {
            dll_id_b[i] = *(pos+i);
            dll_id[i] = *(pos+3-i);
        }
    }
    strprintf(id, "%02x%02x%02x%02x", *(pos+3), *(pos+2), *(pos+1), *(pos+0));
    addExplanationAndIncrementPos(pos, 4, "%02x%02x%02x%02x dll-id (%s)",
                                  *(pos+0), *(pos+1), *(pos+2), *(pos+3), id.c_str());

    dll_version = *(pos+0);
    dll_type = *(pos+1);
    addExplanationAndIncrementPos(pos, 1, "%02x dll-version", dll_version);
    addExplanationAndIncrementPos(pos, 1, "%02x dll-type (%s)", dll_type,
                                  mediaType(dll_type).c_str());

    return true;
}

string Telegram::toStringFromELLSN(int sn)
{
    int session = (sn >> 0)  & 0x0f; // lowest 4 bits
    int time = (sn >> 4)  & 0x1ffffff; // next 25 bits
    int sec  = (sn >> 29) & 0x7; // next 3 bits.
    string info;
    ELLSecurityMode esm = fromIntToELLSecurityMode(sec);
    info += toString(esm);
    info += " session=";
    info += to_string(session);
    info += " time=";
    info += to_string(time);
    return info;
}

bool Telegram::parseELL(vector<uchar>::iterator &pos)
{
    int remaining = distance(pos, frame.end());
    if (remaining == 0) return false;

    debug("(wmbus) parseELL @%d %d\n", distance(frame.begin(), pos), remaining);
    int ci_field = *pos;
    if (!isCiFieldOfType(ci_field, CI_TYPE::ELL)) return true;
    addExplanationAndIncrementPos(pos, 1, "%02x ell-ci-field (%s)",
                                  ci_field, ciType(ci_field).c_str());
    ell_ci = ci_field;
    int len = ciFieldLength(ell_ci);

    if (remaining < len+1) return expectedMore(__LINE__);

    // All ELL:s (including ELL I) start with cc,acc.

    ell_cc = *pos;
    addExplanationAndIncrementPos(pos, 1, "%02x ell-cc (%s)", ell_cc, ccType(ell_cc).c_str());

    ell_acc = *pos;
    addExplanationAndIncrementPos(pos, 1, "%02x ell-acc", ell_acc);

    bool has_target_mft_address = false;
    bool has_session_number_pl_crc = false;

    switch (ell_ci)
    {
    case CI_Field_Values::ELL_I:
        // Already handled above.
        break;
    case CI_Field_Values::ELL_II:
        has_session_number_pl_crc = true;
        break;
    case CI_Field_Values::ELL_III:
        has_target_mft_address = true;
        break;
    case CI_Field_Values::ELL_IV:
        has_session_number_pl_crc = true;
        has_target_mft_address = true;
        break;
    case CI_Field_Values::ELL_V:
        verbose("ELL V not yet handled\n");
        return false;
    }

    if (has_target_mft_address)
    {
        ell_mfct_b[0] = *(pos+0);
        ell_mfct_b[1] = *(pos+1);
        ell_mfct = ell_mfct_b[1] << 8 | ell_mfct_b[0];
        string man = manufacturerFlag(ell_mfct);
        addExplanationAndIncrementPos(pos, 2, "%02x%02x ell-mfct (%s)",
                                      ell_mfct_b[0], ell_mfct_b[1], man.c_str());

        ell_id_found = true;
        ell_id_b[0] = *(pos+0);
        ell_id_b[1] = *(pos+1);
        ell_id_b[2] = *(pos+2);
        ell_id_b[3] = *(pos+3);

        addExplanationAndIncrementPos(pos, 4, "%02x%02x%02x%02x ell-id",
                                      ell_id_b[0], ell_id_b[1], ell_id_b[2], ell_id_b[3]);

        ell_version = *pos;
        addExplanationAndIncrementPos(pos, 1, "%02x ell-version", ell_version);

        ell_type = *pos;
        addExplanationAndIncrementPos(pos, 1, "%02x ell-type");
    }

    if (has_session_number_pl_crc)
    {
        string sn_info;
        ell_sn_b[0] = *(pos+0);
        ell_sn_b[1] = *(pos+1);
        ell_sn_b[2] = *(pos+2);
        ell_sn_b[3] = *(pos+3);
        ell_sn = ell_sn_b[3]<<24 | ell_sn_b[2]<<16 | ell_sn_b[1] << 8 | ell_sn_b[0];

        ell_sn_session = (ell_sn >> 0)  & 0x0f; // lowest 4 bits
        ell_sn_time = (ell_sn >> 4)  & 0x1ffffff; // next 25 bits
        ell_sn_sec = (ell_sn >> 29) & 0x7; // next 3 bits.
        ell_sec_mode = fromIntToELLSecurityMode(ell_sn_sec);
        string info = toString(ell_sec_mode);
        addExplanationAndIncrementPos(pos, 4, "%02x%02x%02x%02x sn (%s)",
                                      ell_sn_b[0], ell_sn_b[1], ell_sn_b[2], ell_sn_b[3], info.c_str());

        if (ell_sec_mode == ELLSecurityMode::AES_CTR)
        {
            bool decrypt_ok = decrypt_ELL_AES_CTR(this, frame, pos, meter_keys->confidentiality_key);
            // Actually this ctr decryption always succeeds, if wrong key, it will decrypt to garbage.
            if (!decrypt_ok)
            {
                decryption_failed = true;
                return true;
            }
            // Now the frame from pos and onwards has been decrypted, perhaps.
        }

        ell_pl_crc_b[0] = *(pos+0);
        ell_pl_crc_b[1] = *(pos+1);
        ell_pl_crc = (ell_pl_crc_b[1] << 8) | ell_pl_crc_b[0];

        int dist = distance(frame.begin(), pos+2);
        int len = distance(pos+2, frame.end());
        uint16_t check = crc16_EN13757(&(frame[dist]), len);

        addExplanationAndIncrementPos(pos, 2, "%02x%02x payload crc (calculated %02x%02x %s)",
                                      ell_pl_crc_b[0], ell_pl_crc_b[1],
                                      check  & 0xff, check >> 8, (ell_pl_crc==check?"OK":"ERROR"));

        if (ell_pl_crc != check)
        {
            // Ouch, checksum of the payload does not match.
            // A wrong key was probably used for decryption.
            decryption_failed = true;
            if (parser_warns_)
            {
                warning("(wmbus) decrypted payload crc failed check, did you use the correct decryption key? Ignoring telegram.\n");
            }
        }
    }

    return true;
}

bool Telegram::parseNWL(vector<uchar>::iterator &pos)
{
    return true;
}

bool Telegram::parseAFL(vector<uchar>::iterator &pos)
{
    // 90 0F (len) 002C (fc) 25 (mc) 49EE 0A00 77C1 9D3D 1A08 ABCD --- 729067296179161102F
    // 90 0F (len) 002C (fc) 25 (mc) 0C39 0000 ED17 6BBB B159 1ADB --- 7A1D003007103EA

    int remaining = distance(pos, frame.end());
    if (remaining == 0) return false;

    debug("(wmbus) parseAFL @%d %d\n", distance(frame.begin(), pos), remaining);

    int ci_field = *pos;
    if (!isCiFieldOfType(ci_field, CI_TYPE::AFL)) return true;
    addExplanationAndIncrementPos(pos, 1, "%02x afl-ci-field (%s)",
                                  ci_field, ciType(ci_field).c_str());
    afl_ci = ci_field;

    afl_len = *pos;
    addExplanationAndIncrementPos(pos, 1, "%02x afl-len (%d)",
                                  afl_len, afl_len);

    int len = ciFieldLength(afl_ci);
    if (remaining < len) return expectedMore(__LINE__);

    afl_fc_b[0] = *(pos+0);
    afl_fc_b[1] = *(pos+1);
    afl_fc = afl_fc_b[1] << 8 | afl_fc_b[0];
    string afl_fc_info = toStringFromAFLFC(afl_fc);
    addExplanationAndIncrementPos(pos, 2, "%02x%02x afl-fc (%s)",
                                  afl_fc_b[0], afl_fc_b[1], afl_fc_info.c_str());

    bool has_key_info = afl_fc & 0x0200;
    bool has_mac = afl_fc & 0x0400;
    bool has_counter = afl_fc & 0x0800;
    //bool has_len = afl_fc & 0x1000;
    bool has_control = afl_fc & 0x2000;
    //bool has_more_fragments = afl_fc & 0x4000;

    if (has_control)
    {
        afl_mcl = *pos;
        string afl_mcl_info = toStringFromAFLMC(afl_mcl);
        addExplanationAndIncrementPos(pos, 1, "%02x afl-mcl (%s)",
                                      afl_mcl, afl_mcl_info.c_str());
    }

    if (has_key_info)
    {
        afl_ki_b[0] = *(pos+0);
        afl_ki_b[1] = *(pos+1);
        afl_ki = afl_ki_b[1] << 8 | afl_ki_b[0];
        string afl_ki_info = "";
        addExplanationAndIncrementPos(pos, 2, "%02x%02x afl-ki (%s)",
                                      afl_ki_b[0], afl_ki_b[1], afl_ki_info.c_str());
    }

    if (has_counter)
    {
        afl_counter_b[0] = *(pos+0);
        afl_counter_b[1] = *(pos+1);
        afl_counter_b[2] = *(pos+2);
        afl_counter_b[3] = *(pos+3);
        afl_counter = afl_counter_b[3] << 24 |
            afl_counter_b[2] << 16 |
            afl_counter_b[1] << 8 |
            afl_counter_b[0];

        addExplanationAndIncrementPos(pos, 4, "%02x%02x%02x%02x afl-counter (%u)",
                                      afl_counter_b[0],afl_counter_b[1],
                                      afl_counter_b[2],afl_counter_b[3],
                                      afl_counter);
    }

    if (has_mac)
    {
        int at = afl_mcl & 0x0f;
        AFLAuthenticationType aat = fromIntToAFLAuthenticationType(at);
        int len = toLen(aat);
        if (len != 2 &&
            len != 4 &&
            len != 8 &&
            len != 12 &&
            len != 16)
        {
            warning("(wmbus) bad length of mac\n");
            return false;
        }
        for (int i=0; i<len; ++i)
        {
            afl_mac_b.insert(afl_mac_b.end(), *(pos+i));
        }
        string s = bin2hex(afl_mac_b);
        addExplanationAndIncrementPos(pos, len, "%s afl-mac %d bytes", s.c_str(), len);
        must_check_mac = true;
    }

    return true;
}

string Telegram::toStringFromAFLFC(int fc)
{
    string info = "";
    int fid = fc & 0x00ff; // Fragmend id
    info += to_string(fid);
    info += " ";
    if (fc & 0x0200) info += "KeyInfoInFragment ";
    if (fc & 0x0400) info += "MACInFragment ";
    if (fc & 0x0800) info += "MessCounterInFragment ";
    if (fc & 0x1000) info += "MessLenInFragment ";
    if (fc & 0x2000) info += "MessControlInFragment ";
    if (fc & 0x4000) info += "MoreFragments ";
    else             info += "LastFragment ";
    if (info.length() > 0) info.pop_back();
    return info;
}

string Telegram::toStringFromAFLMC(int mc)
{
    string info = "";
    int at = mc & 0x0f;
    AFLAuthenticationType aat = fromIntToAFLAuthenticationType(at);
    info += toString(aat);
    info += " ";
    if (mc & 0x10) info += "KeyInfo ";
    if (mc & 0x20) info += "MessCounter ";
    if (mc & 0x40) info += "MessLen ";
    if (info.length() > 0) info.pop_back();
    return info;
}

string Telegram::toStringFromTPLConfig(int cfg)
{
    string info = "";
    if (cfg & 0x8000) info += "bidirectional ";
    if (cfg & 0x4000) info += "accessibility ";
    if (cfg & 0x2000) info += "synchronous ";
    if (cfg & 0x1f00)
    {
        int m = (cfg >> 8) & 0x1f;
        TPLSecurityMode tsm = fromIntToTPLSecurityMode(m);
        info += toString(tsm);
        info += " ";
        if (tsm == TPLSecurityMode::AES_CBC_IV)
        {
            int num_blocks = (cfg & 0x00f0) >> 4;
            int cntn = (cfg & 0x000c) >> 2;
            int ra = (cfg & 0x0002) >> 1;
            int hc = cfg & 0x0001;
            info += "nb="+to_string(num_blocks);
            info += " cntn="+to_string(cntn);
            info += " ra="+to_string(ra);
            info += " hc="+to_string(hc);
            info += " ";
        }
    }
    if (info.length() > 0) info.pop_back();
    return info;
}

bool Telegram::parseTPLConfig(std::vector<uchar>::iterator &pos)
{
    CHECK(2);
    uchar cfg1 = *(pos+0);
    uchar cfg2 = *(pos+1);
    tpl_cfg = cfg2 << 8 | cfg1;

    if (tpl_cfg & 0x1f00)
    {
        int m = (tpl_cfg >> 8) & 0x1f;
        tpl_sec_mode = fromIntToTPLSecurityMode(m);
    }
    bool has_cfg_ext = false;
    string info = toStringFromTPLConfig(tpl_cfg);
    info += " ";
    if (tpl_sec_mode == TPLSecurityMode::AES_CBC_IV) // Security mode 5
    {
        tpl_num_encr_blocks = (tpl_cfg >> 4) & 0x0f;
    }
    if (tpl_sec_mode == TPLSecurityMode::AES_CBC_NO_IV) // Security mode 7
    {
        tpl_num_encr_blocks = (tpl_cfg >> 4) & 0x0f;
        has_cfg_ext = true;
    }
    addExplanationAndIncrementPos(pos, 2, "%02x%02x tpl-cfg %04x (%s)", cfg1, cfg2, tpl_cfg, info.c_str());

    if (has_cfg_ext)
    {
        CHECK(1);
        tpl_cfg_ext = *(pos+0);
        tpl_kdf_selection = (tpl_cfg_ext >> 4) & 3;

        addExplanationAndIncrementPos(pos, 1, "%02x tpl-cfg-ext (KDFS=%d)", tpl_cfg_ext, tpl_kdf_selection);

        if (tpl_kdf_selection == 1)
        {
            vector<uchar> input;
            vector<uchar> mac;
            mac.resize(16);

            // DC C ID 0x07 0x07 0x07 0x07 0x07 0x07 0x07
            // Derivation Constant DC = 0x00 = encryption from meter.
            //                          0x01 = mac from meter.
            //                          0x10 = encryption from communication partner.
            //                          0x11 = mac from communication partner.
            input.insert(input.end(), 0x00); // DC 00 = generate ephemereal encryption key from meter.
            // If there is a tpl_counter, then use it, else use afl_counter.
            input.insert(input.end(), afl_counter_b, afl_counter_b+4);
            // If there is a tpl_id, then use it, else use ddl_id.
            if (tpl_id_found)
            {
                input.insert(input.end(), tpl_id_b, tpl_id_b+4);
            }
            else
            {
                input.insert(input.end(), dll_id_b, dll_id_b+4);
            }

            // Pad.
            for (int i=0; i<7; ++i) input.insert(input.end(), 0x07);

            debugPayload("(wmbus) input to kdf for enc", input);

            if (meter_keys->confidentiality_key.size() != 16)
            {
                if (isSimulated())
                {
                    debug("(wmbus) simulation without keys, not generating Kmac and Kenc.\n");
                    return true;
                }
                debug("(wmbus) no key, thus cannot execute kdf.\n");
                return false;
            }
            AES_CMAC(&meter_keys->confidentiality_key[0], &input[0], 16, &mac[0]);
            string s = bin2hex(mac);
            debug("(wmbus) ephemereal Kenc %s\n", s.c_str());
            tpl_generated_key.clear();
            tpl_generated_key.insert(tpl_generated_key.end(), mac.begin(), mac.end());

            input[0] = 0x01; // DC 01 = generate ephemereal mac key from meter.
            mac.clear();
            mac.resize(16);
            debugPayload("(wmbus) input to kdf for mac", input);
            AES_CMAC(&meter_keys->confidentiality_key[0], &input[0], 16, &mac[0]);
            s = bin2hex(mac);
            debug("(wmbus) ephemereal Kmac %s\n", s.c_str());
            tpl_generated_mac_key.clear();
            tpl_generated_mac_key.insert(tpl_generated_mac_key.end(), mac.begin(), mac.end());
        }
    }

    return true;
}

bool Telegram::parseShortTPL(std::vector<uchar>::iterator &pos)
{
    CHECK(1);

    tpl_acc = *pos;
    addExplanationAndIncrementPos(pos, 1, "%02x tpl-acc-field", tpl_acc);

    CHECK(1);
    tpl_sts = *pos;
    addExplanationAndIncrementPos(pos, 1, "%02x tpl-sts-field", tpl_sts);

    bool ok = parseTPLConfig(pos);
    if (!ok) return false;

    return true;
}

bool Telegram::parseLongTPL(std::vector<uchar>::iterator &pos)
{
    CHECK(4);
    tpl_id_found = true;
    tpl_id_b[0] = *(pos+0);
    tpl_id_b[1] = *(pos+1);
    tpl_id_b[2] = *(pos+2);
    tpl_id_b[3] = *(pos+3);

    addExplanationAndIncrementPos(pos, 4, "%02x%02x%02x%02x tpl-id (%02x%02x%02x%02x)", tpl_id_b[0], tpl_id_b[1], tpl_id_b[2], tpl_id_b[3],
                                  tpl_id_b[3], tpl_id_b[2], tpl_id_b[1], tpl_id_b[0]);

    CHECK(2);
    tpl_mfct_b[0] = *(pos+0);
    tpl_mfct_b[1] = *(pos+1);
    tpl_mfct = tpl_mfct_b[1] << 8 | tpl_mfct_b[0];
    string man = manufacturerFlag(tpl_mfct);
    addExplanationAndIncrementPos(pos, 2, "%02x%02x tpl-mfct (%s)", tpl_mfct_b[0], tpl_mfct_b[1], man.c_str());

    CHECK(1);
    tpl_version = *(pos+0);
    addExplanationAndIncrementPos(pos, 1, "%02x tpl-version", tpl_version);

    CHECK(1);
    tpl_type = *(pos+0);
    string info = mediaType(tpl_type);
    addExplanationAndIncrementPos(pos, 1, "%02x tpl-type (%s)", tpl_type, info.c_str());

    bool ok = parseShortTPL(pos);

    return ok;
}

bool Telegram::checkMAC(std::vector<uchar> &frame,
                        std::vector<uchar>::iterator from,
                        std::vector<uchar>::iterator to,
                        std::vector<uchar> &inmac,
                        std::vector<uchar> &mackey)
{
    vector<uchar> input;
    vector<uchar> mac;
    mac.resize(16);

    if (mackey.size() != 16) return false;
    if (inmac.size() == 0) return false;

    // AFL.MAC = CMAC (Kmac/Lmac,
    //                 AFL.MCL || AFL.MCR || {AFL.ML || } NextCI || ... || Last Byte of message)

    input.insert(input.end(), afl_mcl);
    input.insert(input.end(), afl_counter_b, afl_counter_b+4);
    input.insert(input.end(), from, to);
    string s = bin2hex(input);
    debug("(wmbus) input to mac %s\n", s.c_str());
    AES_CMAC(&mackey[0], &input[0], input.size(), &mac[0]);
    string calculated = bin2hex(mac);
    debug("(wmbus) calculated mac %s\n", calculated.c_str());
    string received = bin2hex(inmac);
    debug("(wmbus) received   mac %s\n", received.c_str());
    string truncated = calculated.substr(0, received.length());
    bool ok = truncated == received;
    if (ok) debug("(wmbus) mac ok!\n");
    else {
        debug("(wmbus) mac NOT ok!\n");
        explainParse("BADMAC", 0);
    }
    return ok;
}

bool loadFormatBytesFromSignature(uint16_t format_signature, vector<uchar> *format_bytes);

bool Telegram::potentiallyDecrypt(vector<uchar>::iterator &pos)
{
    if (tpl_sec_mode == TPLSecurityMode::AES_CBC_IV)
    {
        bool ok = decrypt_TPL_AES_CBC_IV(this, frame, pos, meter_keys->confidentiality_key);
        if (!ok) return false;
        // Now the frame from pos and onwards has been decrypted.

        CHECK(2);
        if (*(pos+0) != 0x2f || *(pos+1) != 0x2f)
        {
            if (parser_warns_)
            {
                warning("(wmbus) decrypted content failed check, did you use the correct decryption key? Ignoring telegram.\n");
            }
            return false;
        }
        addExplanationAndIncrementPos(pos, 2, "%02x%02x decrypt check bytes", *(pos+0), *(pos+1));
    }
    else if (tpl_sec_mode == TPLSecurityMode::AES_CBC_NO_IV)
    {
        if (!meter_keys->hasConfidentialityKey() && isSimulated())
        {
            CHECK(2);
            addExplanationAndIncrementPos(pos, 2, "%02x%02x (already) decrypted check bytes", *(pos+0), *(pos+1));
            return true;
        }
        bool mac_ok = checkMAC(frame, tpl_start, frame.end(), afl_mac_b, tpl_generated_mac_key);

        // Do not attempt to decrypt if the mac has failed!
        if (!mac_ok)
        {
            if (parser_warns_)
            {
                warning("(wmbus) telegram mac check failed, did you use the correct decryption key? Ignoring telegram.\n");
            }
            return false;
        }

        bool ok = decrypt_TPL_AES_CBC_NO_IV(this, frame, pos, tpl_generated_key);
        if (!ok) return false;

        // Now the frame from pos and onwards has been decrypted.
        CHECK(2);
        if (*(pos+0) != 0x2f || *(pos+1) != 0x2f)
        {
            if (parser_warns_)
            {
                warning("(wmbus) decrypted content failed check, did you use the correct decryption key? Ignoring telegram.\n");
            }
            return false;
        }
        addExplanationAndIncrementPos(pos, 2, "%02x%02x decrypt check bytes", *(pos+0), *(pos+1));
    }
    return true;
}

bool Telegram::parse_TPL_72(vector<uchar>::iterator &pos)
{
    bool ok = parseLongTPL(pos);
    if (!ok) return false;

    bool decrypt_ok = potentiallyDecrypt(pos);

    header_size = distance(frame.begin(), pos);
    int remaining = distance(pos, frame.end());
    suffix_size = 0;

    if (decrypt_ok)
    {
        parseDV(this, frame, pos, remaining, &values);
    }
    else
    {
        decryption_failed = true;
    }

    return true;
}

bool Telegram::parse_TPL_78(vector<uchar>::iterator &pos)
{
    header_size = distance(frame.begin(), pos);
    int remaining = distance(pos, frame.end());
    suffix_size = 0;
    parseDV(this, frame, pos, remaining, &values);

    return true;
}

bool Telegram::parse_TPL_79(vector<uchar>::iterator &pos)
{
    // Compact frame
    CHECK(2);
    uchar ecrc0 = *(pos+0);
    uchar ecrc1 = *(pos+1);
    addExplanationAndIncrementPos(pos, 2, "%02x%02x format signature", ecrc0, ecrc1);
    format_signature = ecrc1<<8 | ecrc0;

    vector<uchar> format_bytes;
    bool ok = loadFormatBytesFromSignature(format_signature, &format_bytes);
    if (!ok) {
        // We have not yet seen a long frame, but we know the formats for some
        // meter specific hashes.
        ok = findFormatBytesFromKnownMeterSignatures(&format_bytes);
        if (!ok)
        {
            verbose("(wmbus) ignoring compressed telegram since format signature hash 0x%02x is yet unknown.\n"
                    "     this is not a problem, since you only need wait for at most 8 telegrams\n"
                    "     (8*16 seconds) until an full length telegram arrives and then we know\n"
                    "     the format giving this hash and start decoding the telegrams properly.\n",
                    format_signature);
            return false;
        }
    }
    vector<uchar>::iterator format = format_bytes.begin();

    // 2,3 = crc for payload = hash over both DRH and data bytes. Or is it only over the data bytes?
    CHECK(2);
    int ecrc2 = *(pos+0);
    int ecrc3 = *(pos+1);
    addExplanationAndIncrementPos(pos, 2, "%02x%02x data crc", ecrc2, ecrc3);

    header_size = distance(frame.begin(), pos);
    int remaining = distance(pos, frame.end());
    suffix_size = 0;

    parseDV(this, frame, pos, remaining, &values, &format, format_bytes.size());

    return true;
}

bool Telegram::parse_TPL_7A(vector<uchar>::iterator &pos)
{
    bool ok = parseShortTPL(pos);
    if (!ok) return false;

    bool decrypt_ok = potentiallyDecrypt(pos);

    header_size = distance(frame.begin(), pos);
    int remaining = distance(pos, frame.end());
    suffix_size = 0;

    if (decrypt_ok)
    {
        parseDV(this, frame, pos, remaining, &values);
    }
    else
    {
        decryption_failed = true;
    }
    return true;
}

bool Telegram::parseTPL(vector<uchar>::iterator &pos)
{
    int remaining = distance(pos, frame.end());
    if (remaining == 0) return false;

    debug("(wmbus) parseTPL @%d %d\n", distance(frame.begin(), pos), remaining);
    CHECK(1);
    int ci_field = *pos;
    if (!isCiFieldOfType(ci_field, CI_TYPE::TPL))
    {
        warning("(wmbus) Unknown tpl-ci-field %02x\n", ci_field);
        return false;
    }
    tpl_ci = ci_field;
    tpl_start = pos;

    addExplanationAndIncrementPos(pos, 1, "%02x tpl-ci-field (%s)",
                                  tpl_ci, ciType(tpl_ci).c_str());
    int len = ciFieldLength(tpl_ci);

    if (remaining < len+1) return expectedMore(__LINE__);

    switch (tpl_ci)
    {
        case CI_Field_Values::TPL_72: return parse_TPL_72(pos);
        case CI_Field_Values::TPL_78: return parse_TPL_78(pos);
        case CI_Field_Values::TPL_79: return parse_TPL_79(pos);
        case CI_Field_Values::TPL_7A: return parse_TPL_7A(pos);
        case CI_Field_Values::MFCT_SPECIFIC_A1:
        case CI_Field_Values::MFCT_SPECIFIC_A0: {
            bool _ignore_header_change = false;

            if(dll_type == 0x80 && dll_mfct == 0x5068) { // Techem Heat Cost Allocator
                _ignore_header_change = true;
            }

            if(!_ignore_header_change) {
                header_size = distance(frame.begin(), pos);
            }
            suffix_size = 0;
            return true; // Manufacturer specific telegram payload. Oh well....
        }
        case CI_Field_Values::MFCT_SPECIFIC_A2:
        {
            header_size = distance(frame.begin(), pos);
            suffix_size = 0;
            return true; // Manufacturer specific telegram payload. Oh well....
        }
    }

    header_size = distance(frame.begin(), pos);
    suffix_size = 0;
    warning("(wmbus) Not implemented tpl-ci %02x\n", tpl_ci);
    return false;
}

bool Telegram::parseHeader(vector<uchar> &input_frame)
{
    bool ok;
    explanations.clear();
    frame = input_frame;
    vector<uchar>::iterator pos = frame.begin();
    // Parsed accumulates parsed bytes.
    parsed.clear();

    //     ┌──────────────────────────────────────────────┐
    //     │                                              │
    //     │ Parse DLL Data Link Layer for Wireless MBUS. │
    //     │                                              │
    //     └──────────────────────────────────────────────┘

    ok = parseDLL(pos);
    if (!ok) return false;

    return true;
}

bool Telegram::parse(vector<uchar> &input_frame, MeterKeys *mk)
{
    explanations.clear();
    meter_keys = mk;
    assert(meter_keys != NULL);
    bool ok;
    frame = input_frame;
    vector<uchar>::iterator pos = frame.begin();
    // Parsed accumulates parsed bytes.
    parsed.clear();
    //     ┌──────────────────────────────────────────────┐
    //     │                                              │
    //     │ Parse DLL Data Link Layer for Wireless MBUS. │
    //     │                                              │
    //     └──────────────────────────────────────────────┘

    ok = parseDLL(pos);
    if (!ok) return false;

    printDLL();

    //     ┌──────────────────────────────────────────────┐
    //     │                                              │
    //     │ Is this an ELL block?                        │
    //     │                                              │
    //     └──────────────────────────────────────────────┘

    ok = parseELL(pos);
    if (!ok) return false;

    printELL();
    if (decryption_failed) return false;

    //     ┌──────────────────────────────────────────────┐
    //     │                                              │
    //     │ Is this an NWL block?                        │
    //     │                                              │
    //     └──────────────────────────────────────────────┘

    ok = parseNWL(pos);
    if (!ok) return false;

    printNWL();

    //     ┌──────────────────────────────────────────────┐
    //     │                                              │
    //     │ Is this an AFL block?                        │
    //     │                                              │
    //     └──────────────────────────────────────────────┘

    ok = parseAFL(pos);
    if (!ok) return false;

    printAFL();

    //     ┌──────────────────────────────────────────────┐
    //     │                                              │
    //     │ Is this a TPL block? It ought to be!         │
    //     │                                              │
    //     └──────────────────────────────────────────────┘

    ok = parseTPL(pos);
    if (!ok) return false;

    printTPL();
    if (decryption_failed) return false;

    return true;
}

void Telegram::explainParse(string intro, int from)
{
    for (auto& p : explanations) {
        debug("%s %02x: %s\n", intro.c_str(), p.first, p.second.c_str());
    }
}

string Telegram::autoDetectPossibleDrivers()
{
    vector<string> drivers;
    detectMeterDriver(dll_mfct, dll_type, dll_version, &drivers);
    string possibles;
    for (string d : drivers) possibles = possibles+d+" ";
    if (possibles != "") possibles.pop_back();
    else possibles = "unknown!";

    return possibles;
}

string cType(int c_field)
{
    string s;
    if (c_field & 0x80)
    {
        s += "relayed ";
    }

    if (c_field & 0x40)
    {
        s += "from meter ";
    }
    else
    {
        s += "to meter ";
    }

    int code = c_field & 0x0f;

    switch (code) {
    case 0x0: s += "SND_NKE"; break; // to meter, link reset
    case 0x3: s += "SND_UD2"; break; // to meter, command = user data
    case 0x4: s += "SND_NR"; break; // from meter, unsolicited data, no response expected
    case 0x5: s += "SND_UD3"; break; // to multiple meters, command = user data, no response expected
    case 0x6: s += "SND_IR"; break; // from meter, installation request/data
    case 0x7: s += "ACC_NR"; break; // from meter, unsolicited offers to access the meter
    case 0x8: s += "ACC_DMD"; break; // from meter, unsolicited demand to access the meter
    case 0xa: s += "REQ_UD1"; break; // to meter, alarm request
    case 0xb: s += "REQ_UD2"; break; // to meter, data request
    }

    return s;
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

bool Telegram::findFormatBytesFromKnownMeterSignatures(vector<uchar> *format_bytes)
{
    bool ok = true;
    if (format_signature == 0xa8ed)
    {
        hex2bin("02FF2004134413615B6167", format_bytes);
        debug("(wmbus) using hard coded format for hash a8ed\n");
    }
    else if (format_signature == 0xc412)
    {
        hex2bin("02FF20041392013BA1015B8101E7FF0F", format_bytes);
        debug("(wmbus) using hard coded format for hash c412\n");
    }
    else if (format_signature == 0x61eb)
    {
        hex2bin("02FF2004134413A1015B8101E7FF0F", format_bytes);
        debug("(wmbus) using hard coded format for hash 61eb\n");
    }
    else if (format_signature == 0xd2f7)
    {
        hex2bin("02FF2004134413615B5167", format_bytes);
        debug("(wmbus) using hard coded format for hash d2f7\n");
    }
    else if (format_signature == 0xdd34)
    {
        hex2bin("02FF2004134413", format_bytes);
        debug("(wmbus) using hard coded format for hash dd34\n");
    }
    else
    {
        ok = false;
    }
    return ok;
}

WMBusCommonImplementation::~WMBusCommonImplementation()
{
    manager_->listenTo(this->serial(), NULL);
    manager_->onDisappear(this->serial(), NULL);
    debug("(wmbus) deleted %s\n", toString(type()));
}

WMBusCommonImplementation::WMBusCommonImplementation(WMBusDeviceType t,
                                                     shared_ptr<SerialCommunicationManager> manager,
                                                     shared_ptr<SerialDevice> serial)
    : manager_(manager),
      is_working_(true),
      type_(t),
      serial_(serial),
      cached_device_id_(""),
      command_mutex_("wmbus_command_mutex"),
      waiting_for_response_sem_("waiting_for_response_sem")
{
    // Initialize timeout from now.
    last_received_ = time(NULL);
    last_reset_ = time(NULL);
    manager_->listenTo(this->serial(),call(this,processSerialData));
    manager_->onDisappear(this->serial(),call(this,disconnectedFromDevice));
}

WMBusDeviceType WMBusCommonImplementation::type()
{
    return type_;
}

void WMBusCommonImplementation::onTelegram(function<bool(AboutTelegram&,vector<uchar>)> cb)
{
    telegram_listeners_.push_back(cb);
}

bool WMBusCommonImplementation::handleTelegram(AboutTelegram &about, vector<uchar> frame)
{
    bool handled = false;
    last_received_ = time(NULL);

    for (auto f : telegram_listeners_)
    {
        if (f)
        {
            bool h = f(about, frame);
            if (h) handled = true;
        }
    }

    return handled;
}

void WMBusCommonImplementation::protocolErrorDetected()
{
    protocol_error_count_++;
}

void WMBusCommonImplementation::resetProtocolErrorCount()
{
    protocol_error_count_ = 0;
}

void WMBusCommonImplementation::setLinkModes(LinkModeSet lms)
{
    link_modes_ = lms;
    deviceSetLinkModes(lms);
    link_modes_configured_ = true;
}

bool WMBusCommonImplementation::areLinkModesConfigured()
{
    return link_modes_configured_;
}

LinkModeSet WMBusCommonImplementation::protectedGetLinkModes()
{
    return link_modes_;
}

void WMBusCommonImplementation::deviceClose()
{
}

void WMBusCommonImplementation::close()
{
    debug("(wmbus) closing....\n");
    if (serial())
    {
        if (serial()->opened() && serial()->working())
        {
            debug("(wmbus) yes closing....\n");
            serial()->close();
            manager_->removeNonWorking(serial()->device());
            serial_ = NULL;
        }
    }

    // Invoke any other device specific close for this device.
    deviceClose();
}

bool WMBusCommonImplementation::reset()
{
    last_reset_ = time(NULL);
    bool resetting = false;
    if (serial())
    {
        if (serial()->opened() && serial()->working())
        {
            // This is a reset, not an init. Close the serial device.
            resetting = true;
            serial()->resetInitiated();
            serial()->close();
            // Give the device 3 seconds to shut down properly.
            usleep(3000*1000);
        }

        AccessCheck rc = serial()->open(false);

        if (rc != AccessCheck::AccessOK)
        {
            // Ouch....
            return false;
        }
    }

    // Invoke any other device specific resets for this device.
    deviceReset();

    if (resetting) serial()->resetCompleted();

    // If init, then no link modes are configured.
    // If reset, re-initialize the link modes.
    if (areLinkModesConfigured())
    {
        deviceSetLinkModes(protectedGetLinkModes());
    }

    return true;
}

void WMBusCommonImplementation::disconnectedFromDevice()
{
    if (is_working_)
    {
        debug("(wmbus) disconnected %s %s\n", device().c_str(), toString(type()));
        is_working_ = false;
    }
}

bool WMBusCommonImplementation::isWorking()
{
    return is_working_;
}

void WMBusCommonImplementation::checkStatus()
{
    trace("[ALARM] check status\n");

    time_t since_last_reset = time(NULL) - last_reset_;
    if (reset_timeout_ > 1 &&
        since_last_reset > reset_timeout_ &&
        !serial()->checkIfDataIsPending() &&
        !serial()->readonly())
    {
        verbose("(wmbus) regular reset of %s %s\n", device().c_str(), toString(type()));
        bool ok = reset();
        if (ok) return;
        string msg;
        strprintf(msg, "failed regular reset of %s %s", device().c_str(), toString(type()));
        logAlarm(Alarm::RegularResetFailure, msg);
        return;
    }

    if (protocol_error_count_ >= 20)
    {
        string msg;
        strprintf(msg, "too many protocol errors(%d) resetting %s %s", protocol_error_count_, device().c_str(), toString(type()));
        logAlarm(Alarm::DeviceFailure, msg);
        bool ok = reset();
        if (ok)
        {
            warning("(wmbus) successfully reset wmbus device\n");
            resetProtocolErrorCount();
            return;
        }

        strprintf(msg, "failed to reset wmbus device %s %s exiting wmbusmeters", device().c_str(), toString(type()));
        logAlarm(Alarm::DeviceFailure, msg);
        manager_->stop();
        return;
    }

    time_t now = time(NULL);
    time_t then = now - timeout_;
    time_t since = now-last_received_;

    // If no timeout set, just return.
    if (timeout_ == 0) return;

    if (since < timeout_)
    {
        trace("[WMBUS] No timeout since=%d timeout=%d. All ok.\n", since, timeout_);
        return;
    }

    last_received_ = time(NULL);
    debug("(wmbus) updated_last received for %s (%s)\n", toString(type()), device().c_str());

    // The timeout has expired! But is the timeout expected because there should be no activity now?
    // Also, do not sound the alarm unless we actually have a possible timeout within the expected activity,
    // otherwise we will always get an alarm when we enter the expected activity period.
    if (!(isInsideTimePeriod(now, expected_activity_) &&
          isInsideTimePeriod(then, expected_activity_)))
    {
        trace("[WMBUS] hit timeout(%d s) but this is ok, since there is no expected activity.\n", timeout_);
        return;
    }

    // Ok, timeout has triggered for real! Deal with it!
    struct tm nowtm;
    localtime_r(&now, &nowtm);

    string nowtxt = strdatetime(&nowtm);

    string msg;
    strprintf(msg, "%d seconds of inactivity resetting %s %s "
              "(timeout %ds expected %s now %s)",
              since, device().c_str(), toString(type()),
              timeout_, expected_activity_.c_str(), nowtxt.c_str());

    logAlarm(Alarm::DeviceInactivity, msg);

    bool ok = reset();
    if (ok)
    {
        warning("(wmbus) successfully reset wmbus device\n");
    }
    else
    {
        strprintf(msg, "failed to reset wmbus device %s %s exiting wmbusmeters", device().c_str(), toString(type()));
        logAlarm(Alarm::DeviceFailure, msg);
        manager_->stop();
    }
}

void WMBusCommonImplementation::setResetInterval(int seconds)
{
    reset_timeout_ = seconds;
}

void WMBusCommonImplementation::setTimeout(int seconds, string expected_activity)
{
    assert(seconds >= 0);

    timeout_ = seconds;
    if (expected_activity == "")
    {
        expected_activity = "mon-sun(00-23)";
    }
    expected_activity_ = expected_activity;
    if (seconds > 0)
    {
        debug("(wmbus) set timeout %s to \"%d\" with expected activity \"%s\"\n", toString(type_), timeout_, expected_activity_.c_str());
    }
    else
    {
        debug("(wmbus) no alarm (expected activity) for %s\n", toString(type_));
    }
}

bool WMBusCommonImplementation::waitForResponse(int id)
{
    assert(waiting_for_response_id_ == 0 && id != 0);

    waiting_for_response_id_ = id;

    return waiting_for_response_sem_.wait();
}

bool WMBusCommonImplementation::notifyResponseIsHere(int id)
{
    if (id != waiting_for_response_id_) return false;

    waiting_for_response_id_ = 0;
    waiting_for_response_sem_.notify();

    return true;
}

int toInt(TPLSecurityMode tsm)
{
    switch (tsm) {

#define X(name,nr) case TPLSecurityMode::name : return nr;
LIST_OF_TPL_SECURITY_MODES
#undef X
    }

    return 16;
}

const char *toString(TPLSecurityMode tsm)
{
    switch (tsm) {

#define X(name,nr) case TPLSecurityMode::name : return #name;
LIST_OF_TPL_SECURITY_MODES
#undef X
    }

    return "Reserved";
}

TPLSecurityMode fromIntToTPLSecurityMode(int i)
{
    switch (i) {

#define X(name,nr) case nr: return TPLSecurityMode::name;
LIST_OF_TPL_SECURITY_MODES
#undef X
    }

    return TPLSecurityMode::SPECIFIC_16_31;
}

int toInt(ELLSecurityMode esm)
{
    switch (esm) {

#define X(name,nr) case ELLSecurityMode::name : return nr;
LIST_OF_ELL_SECURITY_MODES
#undef X
    }

    return 2;
}

const char *toString(ELLSecurityMode esm)
{
    switch (esm) {

#define X(name,nr) case ELLSecurityMode::name : return #name;
LIST_OF_ELL_SECURITY_MODES
#undef X
    }

    return "?";
}

ELLSecurityMode fromIntToELLSecurityMode(int i)
{
    switch (i) {

#define X(name,nr) case nr: return ELLSecurityMode::name;
LIST_OF_ELL_SECURITY_MODES
#undef X
    }

    return ELLSecurityMode::RESERVED;
}

void Telegram::extractMfctData(vector<uchar> *pl)
{
    pl->clear();
    if (mfct_0f_index == -1) return;

    vector<uchar>::iterator from = frame.begin()+header_size+mfct_0f_index;
    vector<uchar>::iterator to = frame.end()-suffix_size;
    pl->insert(pl->end(), from, to);
}

void Telegram::extractPayload(vector<uchar> *pl)
{
    pl->clear();
    vector<uchar>::iterator from = frame.begin()+header_size;
    vector<uchar>::iterator to = frame.end()-suffix_size;
    pl->insert(pl->end(), from, to);
}

void Telegram::extractFrame(vector<uchar> *fr)
{
    *fr = frame;
}

int toInt(AFLAuthenticationType aat)
{
    switch (aat) {

#define X(name,nr,len) case AFLAuthenticationType::name : return nr;
LIST_OF_AFL_AUTH_TYPES
#undef X
    }

    return 16;
}

int toLen(AFLAuthenticationType aat)
{
    switch (aat) {

#define X(name,nr,len) case AFLAuthenticationType::name : return len;
LIST_OF_AFL_AUTH_TYPES
#undef X
    }

    return 0;
}

const char *toString(AFLAuthenticationType tsm)
{
    switch (tsm) {

#define X(name,nr,len) case AFLAuthenticationType::name : return #name;
LIST_OF_AFL_AUTH_TYPES
#undef X
    }

    return "Reserved";
}

AFLAuthenticationType fromIntToAFLAuthenticationType(int i)
{
    switch (i) {
#define X(name,nr,len) case nr: return AFLAuthenticationType::name;
LIST_OF_AFL_AUTH_TYPES
#undef X
    }

    return AFLAuthenticationType::Reserved1;
}

AccessCheck findAndDetect(shared_ptr<SerialCommunicationManager> manager,
                          string *out_device,
                          function<AccessCheck(string,shared_ptr<SerialCommunicationManager>)> check,
                          string dongle_name,
                          string device_root)
{
    string dev = device_root;
    debug("(%s) exists? %s\n", dongle_name.c_str(), dev.c_str());
    AccessCheck ac = checkIfExistsAndSameGroup(dev);
    *out_device = dev;
    if (ac == AccessCheck::AccessOK)
    {
        debug("(%s) checking %s\n", dongle_name.c_str(), dev.c_str());
        AccessCheck rc = check(dev, manager);
        if (rc == AccessCheck::AccessOK) return AccessCheck::AccessOK;
    }

    if (ac == AccessCheck::NotSameGroup)
    {
        // Device exists, but you do not belong to its group!
        // This will short circuit testing for other devices.
        // But not being in the same group is such a problematic
        // situation, that we can stop early.
        return AccessCheck::NotSameGroup;
    }

    *out_device = "";
    // No device found!
    return AccessCheck::NotThere;
}

AccessCheck checkAccessAndDetect(shared_ptr<SerialCommunicationManager> manager,
                                 function<AccessCheck(string,shared_ptr<SerialCommunicationManager>)> check,
                                 string dongle_name,
                                 string device)
{
    debug("(%s) exists? %s\n", dongle_name.c_str(), device.c_str());
    AccessCheck ac = checkIfExistsAndSameGroup(device);
    if (ac == AccessCheck::AccessOK)
    {
        debug("(%s) checking %s\n", dongle_name.c_str(), device.c_str());
        AccessCheck rc = check(device, manager);
        if (rc == AccessCheck::AccessOK) return AccessCheck::AccessOK;
        return AccessCheck::NotThere;
    }
    if (ac == AccessCheck::NotSameGroup)
    {
        // Device exists, but you do not belong to its group!
        return AccessCheck::NotSameGroup;
    }

    // No device found!
    return AccessCheck::NotThere;
}

bool trimCRCsFrameFormatA(std::vector<uchar> &payload)
{
    if (payload.size() < 12) {
        debug("(wmbus) not enough bytes! expected at least 12 but got (%zu)!\n", payload.size());
        return false;
    }
    size_t len = payload.size();
    debugPayload("(wmbus) trimming frame A", payload);

    vector<uchar> out;

    uint16_t calc_crc = crc16_EN13757(&payload[0], 10);
    uint16_t check_crc = payload[10] << 8 | payload[11];

    if (calc_crc != check_crc)
    {
        debug("(wmbus) ff a dll crc first (calculated %04x) did not match (expected %04x) for bytes 0-%zu!\n", calc_crc, check_crc, 10);
        return false;
    }
    out.insert(out.end(), payload.begin(), payload.begin()+10);
    debug("(wmbus) ff a dll crc 0-%zu %04x ok\n", 10-1, calc_crc);

    size_t pos = 12;
    for (pos = 12; pos+18 <= len; pos += 18)
    {
        size_t to = pos+16;
        calc_crc = crc16_EN13757(&payload[pos], 16);
        check_crc = payload[to] << 8 | payload[to+1];
        if (calc_crc != check_crc)
        {
            debug("(wmbus) ff a dll crc mid (calculated %04x) did not match (expected %04x) for bytes %zu-%zu!\n",
                  calc_crc, check_crc, pos, to-1);
            return false;
        }
        out.insert(out.end(), payload.begin()+pos, payload.begin()+pos+16);
        debug("(wmbus) ff a dll crc mid %zu-%zu %04x ok\n", pos, to-1, calc_crc);
    }

    if (pos < len-2)
    {
        size_t tto = len-2;
        size_t blen = (tto-pos);
        calc_crc = crc16_EN13757(&payload[pos], blen);
        check_crc = payload[tto] << 8 | payload[tto+1];
        if (calc_crc != check_crc)
        {
            debug("(wmbus) ff a dll crc final (calculated %04x) did not match (expected %04x) for bytes %zu-%zu!\n",
                  calc_crc, check_crc, pos, tto-1);
            return false;
        }
        out.insert(out.end(), payload.begin()+pos, payload.begin()+tto);
        debug("(wmbus) ff a dll crc final %zu-%zu %04x ok\n", pos, tto-1, calc_crc);
    }

    out[0] = out.size()-1;
    size_t new_len = out[0]+1;
    size_t old_size = payload.size();
    payload = out;
    size_t new_size = payload.size();

    debug("(wmbus) trimmed %zu crc bytes from frame a and ignored %zu suffix bytes.\n", (len-new_len), (old_size-new_size)-(len-new_len));
    debugPayload("(wmbus) trimmed  frame A", payload);

    return true;
}

bool trimCRCsFrameFormatB(std::vector<uchar> &payload)
{
    if (payload.size() < 12) {
        debug("(wmbus) not enough bytes! expected at least 12 but got (%zu)!\n", payload.size());
        return false;
    }
    size_t len = payload.size();
    debugPayload("(wmbus) trimming frame B", payload);

    vector<uchar> out;
    size_t crc1_pos, crc2_pos;
    if (len <= 128)
    {
        crc1_pos = len-2;
        crc2_pos = 0;
    }
    else
    {
        crc1_pos = 126;
        crc2_pos = len-2;
    }

    uint16_t calc_crc = crc16_EN13757(&payload[0], crc1_pos);
    uint16_t check_crc = payload[crc1_pos] << 8 | payload[crc1_pos+1];

    if (calc_crc != check_crc)
    {
        debug("(wmbus) ff b dll crc (calculated %04x) did not match (expected %04x) for bytes 0-%zu!\n", calc_crc, check_crc, crc1_pos);
        return false;
    }

    out.insert(out.end(), payload.begin(), payload.begin()+crc1_pos);
    debug("(wmbus) ff b dll crc first 0-%zu %04x ok\n", crc1_pos, calc_crc);

    if (crc2_pos > 0)
    {
        calc_crc = crc16_EN13757(&payload[crc1_pos+2], crc2_pos);
        check_crc = payload[crc2_pos] << 8 | payload[crc2_pos+1];

        if (calc_crc != check_crc)
        {
            debug("(wmbus) ff b dll crc (calculated %04x) did not match (expected %04x) for bytes %zu-%zu!\n",
                  calc_crc, check_crc, crc1_pos+2, crc2_pos);
            return false;
        }

        out.insert(out.end(), payload.begin()+crc1_pos+2, payload.begin()+crc2_pos);
        debug("(wmbus) ff b dll crc final %zu-%zu %04x ok\n", crc1_pos+2, crc2_pos, calc_crc);
    }

    out[0] = out.size()-1;
    size_t new_len = out[0]+1;
    size_t old_size = payload.size();
    payload = out;
    size_t new_size = payload.size();

    debug("(wmbus) trimmed %zu crc bytes from frame b and ignored %zu suffix bytes.\n", (len-new_len), (old_size-new_size)-(len-new_len));
    debugPayload("(wmbus) trimmed  frame B", payload);

    return true;
}

FrameStatus checkWMBusFrame(vector<uchar> &data,
                            size_t *frame_length,
                            int *payload_len_out,
                            int *payload_offset)
{
    // Nice clean: 2A442D2C998734761B168D2021D0871921|58387802FF2071000413F81800004413F8180000615B
    // Ugly: 00615B2A442D2C998734761B168D2021D0871921|58387802FF2071000413F81800004413F8180000615B
    // Here the frame is prefixed with some random data.

    debugPayload("(wmbus) checkWMBUSFrame\n", data);

    if (data.size() < 11)
    {
        debug("(wmbus) less than 11 bytes, partial frame\n");
        return PartialFrame;
    }
    int payload_len = data[0];
    int type = data[1];
    int offset = 1;

    if (type != 0x44)
    {
        // Ouch, we are out of sync with the wmbus frames that we expect!
        // Since we currently do not handle any other type of frame, we can
        // look for the byte 0x44 in the buffer. If we find a 0x44 byte and
        // the length byte before it maps to the end of the buffer,
        // then we have found a valid telegram.
        bool found = false;
        for (size_t i = 0; i < data.size()-2; ++i)
        {
            if (data[i+1] == 0x44)
            {
                payload_len = data[i];
                size_t remaining = data.size()-i;
                if (data[i]+1 == (uchar)remaining && data[i+1] == 0x44)
                {
                    found = true;
                    offset = i+1;
                    verbose("(wmbus) out of sync, skipping %d bytes.\n", (int)i);
                    break;
                }
            }
        }
        if (!found)
        {
            // No sensible telegram in the buffer. Flush it!
            verbose("(wmbus) no sensible telegram found, clearing buffer.\n");
            data.clear();
            return ErrorInFrame;
        }
    }
    *payload_len_out = payload_len;
    *payload_offset = offset;
    *frame_length = payload_len+offset;
    if (data.size() < *frame_length)
    {
        debug("(wmbus) not enough bytes, partial frame %d %d\n", data.size(), *frame_length);
        return PartialFrame;
    }

    debug("(wmbus) received full frame.\n");
    return FullFrame;
}

string decodeTPLStatusByte(uchar sts, map<int,string> vendor_lookup)
{
    string s;

    if (sts == 0) return "OK";
    if ((sts & 0x03) == 0x01) s += "BUSY ";
    if ((sts & 0x03) == 0x02) s += "ERROR ";
    if ((sts & 0x03) == 0x03) s += "ALARM ";

    if ((sts & 0x04) == 0x04) s += "POWER_LOW ";
    if ((sts & 0x08) == 0x08) s += "PERMANENT_ERROR ";
    if ((sts & 0x10) == 0x10) s += "TEMPORARY_ERROR ";

    if ((sts & 0xe0) != 0)
    {
        // Vendor specific bits are set, lets translate them.
        int v = sts & 0xf8; // Zero the 3 lowest bits.
        if (vendor_lookup.count(v) != 0)
        {
            s += vendor_lookup[v];
            s += " ";
        }
        else
        {
            // We could not translate, just print the bits.
            string tmp;
            strprintf(tmp, "%02x ", sts);
            s += tmp;
        }
    }

    s.pop_back();
    return s;
}

const char *toString(WMBusDeviceType t)
{
    switch (t)
    {
#define X(name,text) case DEVICE_ ## name: return #name;
LIST_OF_MBUS_DEVICES
#undef X

    }
    return "?";
}

const char *toLowerCaseString(WMBusDeviceType t)
{
    switch (t)
    {
#define X(name,text) case DEVICE_ ## name: return #text;
LIST_OF_MBUS_DEVICES
#undef X

    }
    return "?";
}

WMBusDeviceType toWMBusDeviceType(string &t)
{
#define X(name,text) if (t == #text) return DEVICE_ ## name;
LIST_OF_MBUS_DEVICES
#undef X
    return DEVICE_UNKNOWN;
}

bool is_command(string b, string *cmd)
{
    // Check if CMD(.)
    if (b.length() < 6) return false;
    if (b.rfind("CMD(", 0) != 0) return false;
    if (b.back() != ')') return false;
    *cmd = b.substr(4, b.length()-5);
    return true;
}

bool is_bps(string b)
{
    if (b == "300") return true;
    if (b == "600") return true;
    if (b == "1200") return true;
    if (b == "2400") return true;
    if (b == "4800") return true;
    if (b == "9600") return true;
    if (b == "14400") return true;
    if (b == "19200") return true;
    if (b == "38400") return true;
    if (b == "57600") return true;
    if (b == "115200") return true;
    return false;
}

bool check_file(string f, bool *is_tty, bool *is_stdin, bool *is_file, bool *is_simulation)
{
    *is_tty = *is_stdin = *is_file = *is_simulation = false;
    if (f == "stdin")
    {
        *is_stdin = true;
        return true;
    }
    if (checkIfSimulationFile(f.c_str()))
    {
        *is_simulation = true;
        return true;
    }
    if (checkCharacterDeviceExists(f.c_str(), false))
    {
        *is_tty = true;
        return true;
    }
    if (checkFileExists(f.c_str()))
    {
        *is_file = true;
        return true;
    }
    if (f.find("/dev") != string::npos)
    {
        // Meter names are forbidden to have slashes in their names.
        // This is probably a path to /dev/ttyUSB0 that does not exist right now.
        *is_tty = true;
        return true;
    }
    return false;
}

bool is_type_id(string t, string *out_type, string *out_id)
{
    // im871a im871a(12345678)
    // auto
    // rtlwmbus
    if (t == "auto")
    {
        *out_type = t;
        *out_id = "";
        return true;
    }

    size_t pps = t.find('[');
    size_t ppe = t.find(']');

    if (pps == string::npos && ppe == string::npos)
    {
        // No parentheses found, is t a known wmbus device? like im871a amb8465 etc....
        WMBusDeviceType tt = toWMBusDeviceType(t);
        if (tt == DEVICE_UNKNOWN) return false;
        *out_type = t;
        *out_id = "";
        return true;
    }

    if (pps != string::npos && ppe != string::npos &&
        pps > 0  && pps < ppe && ppe == t.length()-1)
    {
        string type = t.substr(0, pps);
        string id = t.substr(pps+1, ppe-pps-1);
        WMBusDeviceType tt = toWMBusDeviceType(type);
        if (tt == DEVICE_UNKNOWN) return false;
        if (!isValidId(id, true)) return false;
        *out_type = type;
        *out_id = id;
        return true;
    }

    // Some oddball combination of parentheses were found, give up.
    return false;
}

void SpecifiedDevice::clear()
{
    file = "";
    type = "";
    id = "";
    fq = "";
    linkmodes = "";
}

string SpecifiedDevice::str()
{
    string r;
    if (file != "") r += file+":";
    if (type != "")
    {
        r += type;
        if (id != "")
        {
            r += "["+id+"]";
        }
        r += ":";
    }
    if (bps != "") r += bps+":";
    if (fq != "") r += fq+":";
    if (linkmodes != "") r += linkmodes+":";
    if (command != "") r += "CMD("+command+"):";

    if (r.size() > 0) r.pop_back();

    if (r == "") return "auto";

    return r;
}

bool SpecifiedDevice::parse(string &arg)
{
    clear();

    bool file_checked = false;
    bool typeid_checked = false;
    bool bps_checked = false;
    bool fq_checked = false;
    bool linkmodes_checked = false;
    bool command_checked = false;

    // For the moment the colon : is forbidden in file names and commands.
    // It cannot occur in type,fq or bps.
    vector<string> parts = splitString(arg, ':');

    // Most maxed out device spec, though not valid, since file+cmd is not allowed.
    // Example /dev/ttyUSB0:im871a(12345678):9600:868.95M:c1,t1:CMD(rtl_433 -F csv -f 123M)

    //         file         type   id        bps  fq     linkmodes command
    for (auto& p : parts)
    {
        if (file_checked && typeid_checked && file == "" && type == "" && id == "")
        {
            // There must be either a file and/or type(id). If none are found,
            // then the specified device string is faulty.
            return false;
        }
        if (!file_checked && check_file(p, &is_tty, &is_stdin, &is_file, &is_simulation))
        {
            file_checked = true;
            file = p;
        }
        else if (!typeid_checked && is_type_id(p, &type, &id))
        {
            file_checked = true;
            typeid_checked = true;
        }
        else if (!bps_checked && is_bps(p))
        {
            file_checked = true;
            typeid_checked = true;
            bps_checked = true;
            bps = p;
        }
        else if (!fq_checked && isFrequency(p))
        {
            file_checked = true;
            typeid_checked = true;
            bps_checked = true;
            fq_checked = true;
            fq = p;
        }
        else if (!linkmodes_checked && isValidLinkModes(p))
        {
            file_checked = true;
            typeid_checked = true;
            bps_checked = true;
            fq_checked = true;
            linkmodes_checked = true;
            linkmodes = p;
        }
        else if (!command_checked && is_command(p, &command))
        {
            file_checked = true;
            typeid_checked = true;
            bps_checked = true;
            fq_checked = true;
            linkmodes_checked = true;
            command_checked = true;
        }
        else
        {
            // Unknown part....
            return false;
        }
    }

    // Auto is only allowed to be combined with linkmodes and/or frequencies!
    if (type == "auto" && (file != "" || bps != "")) return false;
    // You cannot combine a file with a command.
    if (file != "" && command != "") return false;
    return true;
}

Detected detectWMBusDeviceOnTTY(string tty, shared_ptr<SerialCommunicationManager> handler)
{
    Detected detected;
    // Fake a specified device.
    detected.found_file = tty;
    detected.specified_device.is_tty = true;

    // If im87a is tested first, a delay of 1s must be inserted
    // before amb8465 is tested, lest it will not respond properly.
    // It really should not matter, but perhaps is the uart of the amber
    // confused by the 57600 speed....or maybe there is some other reason.
    // Anyway by testing for the amb8465 first, we can immediately continue
    // with the test for the im871a, without the need for a 1s delay.

    // Talk amb8465 with it...
    // assumes this device is configured for 9600 bps, which seems to be the default.
    if (detectAMB8465(&detected, handler) == AccessCheck::AccessOK)
    {
        return detected;
    }

    // Talk RC1180 with it...
    // assumes this device is configured for 19200 bps, which seems to be the default.
    if (detectRC1180(&detected, handler) == AccessCheck::AccessOK)
    {
        return detected;
    }

    // Talk im871a with it...
    // assumes this device is configured for 57600 bps, which seems to be the default.
    if (detectIM871A(&detected, handler) == AccessCheck::AccessOK)
    {
        return detected;
    }

    // Talk CUL with it...
    // assumes this device is configured for 38400 bps, which seems to be the default.
    if (detectCUL(&detected, handler) == AccessCheck::AccessOK)
    {
        return detected;
    }

    // We could not auto-detect either. default is DEVICE_UNKNOWN.
    return detected;
}

Detected detectWMBusDeviceWithFile(SpecifiedDevice &specified_device,
                                   shared_ptr<SerialCommunicationManager> handler)
{
    assert(specified_device.file != "");
    assert(specified_device.command == "");
    debug("(lookup) with file \"%s\"\n", specified_device.str().c_str());

    Detected detected;
    detected.found_file = specified_device.file;
    detected.setSpecifiedDevice(specified_device);

    if (specified_device.is_simulation)
    {
        debug("(lookup) driver: simulation file\n");
        detected.setAsFound("", DEVICE_SIMULATION, 0 , false, false);
        specified_device.linkmodes = "any";
        return detected;
    }

    // Special case to cater for /dev/ttyUSB0:9600, ie the rawtty is implicit.
    if (specified_device.type == "" && specified_device.bps != "" && specified_device.is_tty)
    {
        debug("(lookup) driver: rawtty\n");
        detected.setAsFound("", DEVICE_RAWTTY, atoi(specified_device.bps.c_str()), false, false);
        return detected;
    }

    // Special case to cater for raw_data.bin, ie the rawtty is implicit.
    if (specified_device.type == "" && !specified_device.is_tty)
    {
        debug("(lookup) driver: raw file\n");
        detected.setAsFound("", DEVICE_RAWTTY, 0, true, false);
        return detected;
    }

    // Now handle all files with specified type.
    if (specified_device.type != "")
    {
        debug("(lookup) driver: %s\n", specified_device.type.c_str());
        detected.setAsFound("", toWMBusDeviceType(specified_device.type), 0, true, false);
        return detected;
    }
    // Ok, we are left with a single /dev/ttyUSB0 lets talk to it
    // to figure out what is connected to it.
    return detectWMBusDeviceOnTTY(specified_device.file, handler);
}

Detected detectWMBusDeviceWithCommand(SpecifiedDevice &specified_device,
                                      shared_ptr<SerialCommunicationManager> handler)
{
    assert(specified_device.file == "");
    assert(specified_device.command != "");
    debug("(lookup) with cmd \"%s\"\n", specified_device.str().c_str());

    Detected detected;
    detected.found_command = specified_device.command;
    detected.setSpecifiedDevice(specified_device);
    detected.setAsFound("", toWMBusDeviceType(specified_device.type), 0, false, true);

    return detected;
}

AccessCheck detectUNKNOWN(Detected *detected, shared_ptr<SerialCommunicationManager> handler)
{
    return AccessCheck::NotThere;
}

AccessCheck detectSIMULATION(Detected *detected, shared_ptr<SerialCommunicationManager> handler)
{
    return AccessCheck::NotThere;
}

AccessCheck detectDevice(Detected *detected, shared_ptr<SerialCommunicationManager> handler)
{
    string type = detected->specified_device.type;

#define X(name,text) if (type == #text) return detect ## name(detected,handler);
LIST_OF_MBUS_DEVICES
#undef X

    assert(0);

    return AccessCheck::NotThere;
}
