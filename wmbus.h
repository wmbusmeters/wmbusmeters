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

#ifndef WMBUS_H
#define WMBUS_H

#include"manufacturers.h"
#include"serial.h"
#include"util.h"

#include<inttypes.h>

#define LIST_OF_LINK_MODES X(LinkModeC1)X(LinkModeT1)X(UNKNOWN_LINKMODE)

enum LinkMode {
#define X(name) name,
LIST_OF_LINK_MODES
#undef X
};

#define CC_B_BIDIRECTIONAL_BIT 0x80
#define CC_RD_RESPONSE_DELAY_BIT 0x40
#define CC_S_SYNCH_FRAME_BIT 0x20
#define CC_R_RELAYED_BIT 0x10
#define CC_P_HIGH_PRIO_BIT 0x08

// Bits 31-29 in SN, ie 0xc0 of the final byte in the stream,
// since the bytes arrive with the least significant first
// aka little endian.
#define SN_ENC_BITS 0xc0

using namespace std;

struct Telegram {
    int len; // The length of the telegram, 1 byte.
    int c_field; // 1 byte (0x44=telegram, no response expected!)
    int m_field; // Manufacturer 2 bytes
    vector<uchar> a_field; // A field 6 bytes
    // The 6 a field bytes are composed of:
    vector<uchar> a_field_address; // Address in BCD = 8 decimal 00000000...99999999 digits.
    int a_field_version; // 1 byte
    int a_field_device_type; // 1 byte

    int ci_field; // 1 byte
    // When ci_field==0x8d then there are 8 extra header bytes (ELL header?)
    int cc_field; // 1 byte
    int acc; // 1 byte
    uchar sn[4]; // 4 bytes
    // That is 6 bytes (not 8), perhaps the next two bytes (the plcrc?) are
    // part of this ELL header, even though they are inside the encrypted payload?

    vector<uchar> parsed; // Parsed fields
    vector<uchar> payload; // To be parsed.
    vector<uchar> content; // Decrypted content.

    bool handled {}; // Set to true, when a meter has accepted the telegram.

    // The id as written on the physical meter device.
    string id() { return bin2hex(a_field_address); }

    void parse(vector<uchar> &payload);
    void print();
    void verboseFields();

    // A vector of indentations and explanations, to be printed
    // below the raw data bytes to explain the telegram content.
    vector<pair<int,string>> explanations;
    void addExplanation(vector<uchar> &payload, int len, const char* fmt, ...);
    void explainParse(string intro, int from);
};

struct WMBus {
    virtual bool ping() = 0;
    virtual uint32_t getDeviceId() = 0;
    virtual LinkMode getLinkMode() = 0;
    virtual void setLinkMode(LinkMode lm) = 0;
    virtual void onTelegram(function<void(Telegram*)> cb) = 0;
    virtual SerialDevice *serial() = 0;
    virtual void simulate() = 0;
};

#define LIST_OF_MBUS_DEVICES X(DEVICE_IM871A)X(DEVICE_AMB8465)X(DEVICE_SIMULATOR)X(DEVICE_UNKNOWN)

enum MBusDeviceType {
#define X(name) name,
LIST_OF_MBUS_DEVICES
#undef X
};

// The detect function can be supplied the device "auto" and will try default locations for the device.
// Returned is the type and the found device string.
pair<MBusDeviceType,string> detectMBusDevice(string device, SerialCommunicationManager *manager);

WMBus *openIM871A(string device, SerialCommunicationManager *manager);
WMBus *openAMB8465(string device, SerialCommunicationManager *manager);
struct WMBusSimulator;
WMBus *openSimulator(string file, SerialCommunicationManager *manager);

string manufacturer(int m_field);
string manufacturerFlag(int m_field);
string deviceType(int m_field, int a_field_device_type);
string mediaType(int m_field, int a_field_device_type);
string ciType(int ci_field);
string cType(int c_field);
string ccType(int cc_field);
string difType(int dif);
string vifType(int vif);
string vifeType(int vif, int vife);

#endif
