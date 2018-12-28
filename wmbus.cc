/*
 Copyright (C) 2017-2018 Fredrik Öhrström

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
#include<stdarg.h>
#include<unistd.h>

const char *LinkModeNames[] = {
#define X(name) #name ,
LIST_OF_LINK_MODES
#undef X
};

struct Manufacturer {
    char code[4];
    int m_field;
    char name[64];
};

Manufacturer manufacturers[] = {
#define X(key,code,name) {#key,code,#name},
LIST_OF_MANUFACTURERS
#undef X
{"",0,""}
};

struct Initializer { Initializer(); };

static Initializer initializser_;

Initializer::Initializer() {
    for (auto &m : manufacturers) {
	m.m_field = \
	    (m.code[0]-64)*1024 +
	    (m.code[1]-64)*32 +
	    (m.code[2]-64);
    }
}

void Telegram::print() {
    printf("Received telegram from: %02x%02x%02x%02x\n",
	   a_field_address[0], a_field_address[1], a_field_address[2], a_field_address[3]);
    printf("          manufacturer: (%s) %s\n",
           manufacturerFlag(m_field).c_str(),
	   manufacturer(m_field).c_str());
    printf("           device type: %s\n",
	   deviceType(m_field, a_field_device_type).c_str());
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
            deviceType(m_field, a_field_device_type).c_str(),
            ci_field,
            ciType(ci_field).c_str());

    if (ci_field == 0x78) {
        // No data error and no encryption possible.
    }

    if (ci_field == 0x7a) {
        // Short data header
        verbose(" CC-field=%02x (%s) ACC=%02x ",
                cc_field, ccType(cc_field).c_str(),
                acc,
                sn[3],sn[2],sn[1],sn[0]);
    }

    if (ci_field == 0x8d) {
        verbose(" CC-field=%02x (%s) ACC=%02x SN=%02x%02x%02x%02x",
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
    for (auto &m : manufacturers) {
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

string deviceType(int m_field, int a_field_device_type) {
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
    }
    return "Unknown";
}

string mediaType(int m_field, int a_field_device_type) {
    switch (a_field_device_type) {
    case 0: return "other";
    case 1: return "oil";
    case 2: return "electricity";
    case 3: return "gas";
    case 4: return "heat";
    case 5: return "steam";
    case 6: return "warm water";
    case 7: return "water";
    case 8: return "heat cost";
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
    }
    return "Unknown";
}

bool detectIM871A(string device, SerialCommunicationManager *handler);
bool detectAMB8465(string device, SerialCommunicationManager *handler);

pair<MBusDeviceType,string> detectMBusDevice(string device, SerialCommunicationManager *handler)
{
    // If auto, then assume that uev has been configured with
    // with the file: `/etc/udev/rules.d/99-usb-serial.rules` containing
    // SUBSYSTEM=="tty", ATTRS{idVendor}=="10c4", ATTRS{idProduct}=="ea60", SYMLINK+="im871a",MODE="0660", GROUP="yourowngroup"
    // SUBSYSTEM=="tty", ATTRS{idVendor}=="0403", ATTRS{idProduct}=="6001", SYMLINK+="amb8465",MODE="0660", GROUP="yourowngroup"

    if (device == "auto")
    {
        if (detectIM871A("/dev/im871a", handler))
        {
            return { DEVICE_IM871A, "/dev/im871a" };
        }
        if (detectAMB8465("/dev/amb8465", handler))
        {
            return { DEVICE_AMB8465, "/dev/amb8465" };
        }
        return { DEVICE_UNKNOWN, "" };
    }

    if (checkIfSimulationFile(device.c_str()))
    {
        return { DEVICE_SIMULATOR, device };
    }

    // If not auto, then test the device, is it a character device?
    checkCharacterDeviceExists(device.c_str(), true);

    // If im87a is tested first, a delay of 1s must be inserted
    // before amb8465 is tested, lest it will not respond properly.
    // It really should not matter, but perhaps is the uart of the amber
    // confused by the 57600 speed....or maybe there is some other reason.
    // Anyway by testing for the amb8465 first, we can immediately continue
    // with the test for the im871a, without the need for a 1s delay.

    // Talk amb8465 with it...
    // assumes this device is configured for 9600 bps, which seems to be the default.
    if (detectAMB8465(device, handler))
    {
        return { DEVICE_AMB8465, device };
    }
    // Talk im871a with it...
    // assumes this device is configured for 57600 bps, which seems to be the default.
    if (detectIM871A(device, handler))
    {
        return { DEVICE_IM871A, device };
    }
    return { DEVICE_UNKNOWN, "" };
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
            p.second += buf;
            found = true;
        }
    }

    if (!found) {
        error("(wmbus) cannot find offset %d to add more explanation \"%s\"\n", pos, buf);
    }
}

void Telegram::parse(vector<uchar> &frame)
{
    vector<uchar>::iterator bytes = frame.begin();
    parsed.clear();
    len = frame[0];
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

    a_field_version = frame[4+4];
    a_field_device_type = frame[4+5];
    addExplanation(bytes, 1, "%02x a-field-version", frame[8]);
    addExplanation(bytes, 1, "%02x a-field-type (%s)", frame[9], deviceType(m_field, a_field_device_type).c_str());

    ci_field=frame[10];
    addExplanation(bytes, 1, "%02x ci-field (%s)", ci_field, ciType(ci_field).c_str());

    int header_size = 0;
    if (ci_field == 0x78) {
        header_size = 0; // And no encryption possible.
    } else
    if (ci_field == 0x7a) {
        acc = frame[11];
        addExplanation(bytes, 1, "%02x acc", acc);
        status = frame[12];
        addExplanation(bytes, 1, "%02x status ()", status);
        configuration = frame[13]<<8 | frame[14];
        addExplanation(bytes, 2, "%02x%02x configuration ()", frame[13], frame[14]);
        header_size = 4;
    } else
    if (ci_field == 0x8d || ci_field == 0x8c) {
        cc_field = frame[11];
        addExplanation(bytes, 1, "%02x cc-field (%s)", cc_field, ccType(cc_field).c_str());
        acc = frame[12];
        addExplanation(bytes, 1, "%02x acc", acc);
        header_size = 2;
        if (ci_field == 0x8d) {
            sn[0] = frame[13];
            sn[1] = frame[14];
            sn[2] = frame[15];
            sn[3] = frame[16];
            addExplanation(bytes, 4, "%02x%02x%02x%02x sn", sn[0], sn[1], sn[2], sn[3]);
            header_size = 6;
        }
    } else {
        warning("Unknown ci-field %02x\n", ci_field);
    }

    payload.clear();
    payload.insert(payload.end(), frame.begin()+(11+header_size), frame.end());
    verbose("(wmbus) received telegram");
    verboseFields();
    debugPayload("(wmbus) frame", frame);
    debugPayload("(wmbus) payload", payload);
    if (isDebugEnabled()) {
        explainParse("(wmbus)", 0);
    }
}

void Telegram::explainParse(string intro, int from)
{
    for (auto& p : explanations) {
        if (p.first < from) continue;
        printf("%s %02x: %s\n", intro.c_str(), p.first, p.second.c_str());
    }
    string hex = bin2hex(parsed);
    printf("%s %s\n", intro.c_str(), hex.c_str());
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
    if (dif == 0x44) {
        // Special functions: 0x0F, 0x1F, 0x2F, 0x7F or dif >= 0x3F and dif <= 0x6F
        // 0x44 is used in short frames in the Kamstrup Multical21/FlowIQ3100 to
        // denote the lower 16 bits of data to be combined with the upper 16 bits of a previous value.
        // For long frames in the same meters however, the full 32 bits are stored.
        // Let us return 4 byte here and deal with the truncated version in the meter code for compact frames,
        // through a length override.
        return 4;
    }

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
    /*
    if (dif & 0x40) {
        // Kamstrup uses this bit in the Multical21 to signal that for the
        // full 32 bits of data, the lower 16 bits are from this difvif record,
        // and the high 16 bits are from a different record.
        s += " VendorSpecific";
        }*/
    return s;
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
        case 0xff: return "Kamstrup extension";
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
    case 0x80: return "Address";

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
    case 0x6E: warning("(wmbus) warning: do not scale a HCA type!\n"); return -1.0; // Units for H.C.A.
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
    case 0x80: return "address"; // Address

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
    case 0x80: return ""; // Address

    case 0x7C: // VIF in following string (length in first byte)
    case 0x7E: // Any VIF
    case 0x7F: // Manufacturer specific

    default: warning("(wmbus) warning: generic type %d cannot be scaled!\n", t);
        return "unknown";
    }
}

string vifeType(int vif, int vife)
{
    //int extension = vif & 0x80;
    //int t = vif & 0x7f;

    if (vif == 0x83 && vife == 0x3b) {
        return "Forward flow contribution only";
    }
    if (vif == 0xff) {
        return "?";
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
    switch (link_mode) {
    case LinkModeC1: return "C1";
    case LinkModeT1: return "T1";
    case UNKNOWN_LINKMODE: break;
    }
    return "UnknownLinkMode";
}

WMBus::~WMBus() {
}
