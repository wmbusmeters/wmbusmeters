// Copyright (c) 2017 Fredrik Öhrström
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

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

    if (ci_field == 0x8d) {
        verbose(" CC-field=%02x (%s) ACC=%02x SN=%02x%02x%02x%02x",
                cc_field, ccType(cc_field).c_str(),
                acc,
                sn[3],sn[2],sn[1],sn[0]);
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

void Telegram::addExplanation(vector<uchar> &payload, int len, const char* fmt, ...)
{
    char buf[1024];

    buf[1023] = 0;

    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, 1023, fmt, args);
    va_end(args);

    explanations.push_back({parsed.size(), buf});
    parsed.insert(parsed.end(), payload.begin()+parsed.size(), payload.begin()+parsed.size()+len);
}

void Telegram::parse(vector<uchar> &frame)
{
    parsed.clear();
    len = frame[0];
    addExplanation(frame, 1, "%02x length (%d bytes)", len, len);
    c_field = frame[1];
    addExplanation(frame, 1, "%02x c-field (%s)", c_field, cType(c_field).c_str());
    m_field = frame[3]<<8 | frame[2];
    string man = manufacturerFlag(m_field);
    addExplanation(frame, 2, "%02x%02x m-field (%02x=%s)", frame[2], frame[3], m_field, man.c_str());
    a_field.resize(6);
    a_field_address.resize(4);
    for (int i=0; i<6; ++i) {
        a_field[i] = frame[4+i];
        if (i<4) { a_field_address[i] = frame[4+3-i]; }
    }
    addExplanation(frame, 4, "%02x%02x%02x%02x a-field-addr (%02x%02x%02x%02x)", frame[4], frame[5], frame[6], frame[7],
                   frame[7], frame[6], frame[5], frame[4]);

    a_field_version = frame[4+4];
    a_field_device_type = frame[4+5];
    addExplanation(frame, 1, "%02x a-field-version", frame[8]);
    addExplanation(frame, 1, "%02x a-field-type (%s)", frame[9], deviceType(m_field, a_field_device_type).c_str());

    ci_field=frame[10];
    addExplanation(frame, 1, "%02x ci-field (%s)", ci_field, ciType(ci_field).c_str());

    if (ci_field == 0x8d) {
        cc_field = frame[11];
        addExplanation(frame, 1, "%02x cc-field (%s)", cc_field, ccType(cc_field).c_str());
        acc = frame[12];
        addExplanation(frame, 1, "%02x acc", acc);
        sn[0] = frame[13];
        sn[1] = frame[14];
        sn[2] = frame[15];
        sn[3] = frame[16];
        addExplanation(frame, 4, "%02x%02x%02x%02x sn", sn[0], sn[1], sn[2], sn[3]);
    }

    payload.clear();
    payload.insert(payload.end(), frame.begin()+17, frame.end());
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
        printf("%s ", intro.c_str());
        for (int i=0; i<p.first; ++i) {
            printf("  ");
        }
        printf("%s\n", p.second.c_str());
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

    t = dif & 0x30;

    switch (t) {
    case 0x00: s += " Instantaneous value"; break;
    case 0x10: s += "Maximum value"; break;
    case 0x20: s += "Minimum value"; break;
    case 0x30: s+= "Value during error state"; break;
    default: s += "?"; break;
    }
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

string vifeType(int vif, int vife)
{
    //int extension = vif & 0x80;
    //int t = vif & 0x7f;

    if (vif == 0xff) {
        return "?";
    }
    return "?";
}
