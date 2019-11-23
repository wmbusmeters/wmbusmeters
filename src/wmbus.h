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

#ifndef WMBUS_H
#define WMBUS_H

#include"manufacturers.h"
#include"serial.h"
#include"util.h"

#include<inttypes.h>

#define LIST_OF_LINK_MODES \
    X(Any,any,--anylinkmode,0xffff)             \
    X(C1,c1,--c1,0x1)                           \
    X(S1,s1,--s1,0x2)                           \
    X(S1m,s1m,--s1m,0x4)                       \
    X(T1,t1,--t1,0x8)                          \
    X(N1a,n1a,--n1a,0x10)                      \
    X(N1b,n1b,--n1b,0x20)                      \
    X(N1c,n1c,--n1c,0x40)                      \
    X(N1d,n1d,--n1d,0x80)                      \
    X(N1e,n1e,--n1e,0x100)                     \
    X(N1f,n1f,--n1f,0x200)                     \
    X(UNKNOWN,unknown,----,0x0)

// In link mode T1, the meter transmits a telegram every few seconds or minutes.
// Suitable for drive-by/walk-by collection of meter values.

// Link mode C1 is like T1 but uses less energy when transmitting due to
// a different radio encoding.

enum class LinkMode {
#define X(name,lcname,option,val) name,
LIST_OF_LINK_MODES
#undef X
};

enum LinkModeBits {
#define X(name,lcname,option,val) name##_bit = val,
LIST_OF_LINK_MODES
#undef X
};

LinkMode isLinkMode(const char *arg);
LinkMode isLinkModeOption(const char *arg);

struct LinkModeSet
{
    // Add the link mode to the set of link modes.
    void addLinkMode(LinkMode lm);
    void unionLinkModeSet(LinkModeSet lms);
    void disjunctionLinkModeSet(LinkModeSet lms);
    // Does this set support listening to the given link mode set?
    // If this set is C1 and T1 and the supplied set contains just C1,
    // then supports returns true.
    // Or if this set is just T1 and the supplied set contains just C1,
    // then supports returns false.
    // Or if this set is just C1 and the supplied set contains C1 and T1,
    // then supports returns true.
    // Or if this set is S1 and T1, and the supplied set contains C1 and T1,
    // then supports returns true.
    //
    // It will do a bitwise and of the linkmode bits. If the result
    // of the and is not zero, then support returns true.
    bool supports(LinkModeSet lms);
    // Check if this set contains the given link mode.
    bool has(LinkMode lm);
    // Check if all link modes are supported.
    bool hasAll(LinkModeSet lms);

    int bits() { return set_; }

    // Return a human readable string.
    std::string hr();

    LinkModeSet() { }
    LinkModeSet(int s) : set_(s) {}

private:

    int set_ {};
};

LinkModeSet parseLinkModes(string modes);

#define CC_B_BIDIRECTIONAL_BIT 0x80
#define CC_RD_RESPONSE_DELAY_BIT 0x40
#define CC_S_SYNCH_FRAME_BIT 0x20
#define CC_R_RELAYED_BIT 0x10
#define CC_P_HIGH_PRIO_BIT 0x08

// Bits 31-29 in SN, ie 0xc0 of the final byte in the stream,
// since the bytes arrive with the least significant first
// aka little endian.
#define SN_ENC_BITS 0xc0

enum class EncryptionMode
{
    None,
    AES_CBC,
    AES_CTR
};

enum class MeasurementType
{
    Unknown,
    Instantaneous,
    Minimum,
    Maximum,
    AtError
};

using namespace std;

struct Telegram {
    int len {}; // The length of the telegram, 1 byte.
    int c_field {}; // 1 byte (0x44=telegram, no response expected!)
    int m_field {}; // Manufacturer 2 bytes
    vector<uchar> a_field; // A field 6 bytes
    // The 6 a field bytes are composed of:
    vector<uchar> a_field_address; // Address in BCD = 8 decimal 00000000...99999999 digits.
    string id; // the address as a string.
    int a_field_version {}; // 1 byte
    int a_field_device_type {}; // 1 byte

    int ci_field {}; // 1 byte

    // When ci_field==0x7a then there are 4 extra header bytes, short data header
    int acc {}; // 1 byte
    int status {}; // 1 byte
    int config_field {}; // 2 bytes

    // When ci_field==0x8d then there are 8 extra header bytes (ELL header)
    int cc_field {}; // 1 byte
    // acc; // 1 byte
    uchar sn[4] {}; // 4 bytes
    // That is 6 bytes (not 8), the next two bytes, the payload crc
    // part of this ELL header, even though they are inside the encrypted payload.

    // When ci_field==0x72 then there are 12 extra header bytes (LONG TPL header)
    // Id(4bytes) Manuf(2bytes) Ver(1byte) Dev(1byte) Type(1byte) ACC(1byte) STS(1byte) CF/CFE(1byte)
    int sts_field {};
    int cf_cfe_field {};

    vector<uchar> parsed; // Parsed fields
    vector<uchar> payload; // To be parsed.
    vector<uchar> content; // Decrypted content.

    bool handled {}; // Set to true, when a meter has accepted the telegram.


    bool parse(vector<uchar> &payload);
    void print();
    void verboseFields();

    // A vector of indentations and explanations, to be printed
    // below the raw data bytes to explain the telegram content.
    vector<pair<int,string>> explanations;
    void addExplanation(vector<uchar>::iterator &bytes, int len, const char* fmt, ...);
    void addMoreExplanation(int pos, const char* fmt, ...);
    void explainParse(string intro, int from);

    bool isEncrypted() { return is_encrypted_; }
    bool isSimulated() { return is_simulated_; }

    void markAsSimulated() { is_simulated_ = true; }

    void expectVersion(const char *info, int v);

private:

    bool is_encrypted_ {};
    bool is_simulated_ {};
};

struct WMBus {
    virtual bool ping() = 0;
    virtual uint32_t getDeviceId() = 0;
    virtual LinkModeSet getLinkModes() = 0;
    virtual LinkModeSet supportedLinkModes() = 0;
    virtual int numConcurrentLinkModes() = 0;
    virtual bool canSetLinkModes(LinkModeSet lms) = 0;
    virtual void setLinkModes(LinkModeSet lms) = 0;
    virtual void onTelegram(function<void(Telegram*)> cb) = 0;
    virtual SerialDevice *serial() = 0;
    virtual void simulate() = 0;
    virtual ~WMBus() = 0;
};

#define LIST_OF_MBUS_DEVICES X(DEVICE_CUL)X(DEVICE_IM871A)X(DEVICE_AMB8465)X(DEVICE_RFMRX2)X(DEVICE_SIMULATOR)X(DEVICE_RTLWMBUS)X(DEVICE_RAWTTY)X(DEVICE_UNKNOWN)

enum MBusDeviceType {
#define X(name) name,
LIST_OF_MBUS_DEVICES
#undef X
};

struct Detected
{
    MBusDeviceType type;  // IM871A, AMB8465 etc
    string devicefile;    // /dev/ttyUSB0 /dev/ttyACM0 stdin simulation_abc.txt telegrams.raw
    int baudrate;         // If the suffix is a number, store the number here.
    // If the override_tty is true, then do not allow the wmbus driver to open the tty,
    // instead open the devicefile first. This is to allow feeding the wmbus drivers using stdin
    // or a file or for internal testing.
    bool override_tty;
};

Detected detectWMBusDeviceSetting(string devicefile, string suffix,
                                  SerialCommunicationManager *manager);

unique_ptr<WMBus> openIM871A(string device, SerialCommunicationManager *manager,
                             unique_ptr<SerialDevice> serial_override);
unique_ptr<WMBus> openAMB8465(string device, SerialCommunicationManager *manager,
                              unique_ptr<SerialDevice> serial_override);
unique_ptr<WMBus> openRawTTY(string device, int baudrate, SerialCommunicationManager *manager,
                             unique_ptr<SerialDevice> serial_override);
unique_ptr<WMBus> openRTLWMBUS(string device, SerialCommunicationManager *manager, std::function<void()> on_exit,
                               unique_ptr<SerialDevice> serial_override);
unique_ptr<WMBus> openCUL(string device, SerialCommunicationManager *manager,
                              unique_ptr<SerialDevice> serial_override);
unique_ptr<WMBus> openSimulator(string file, SerialCommunicationManager *manager,
                                unique_ptr<SerialDevice> serial_override);

string manufacturer(int m_field);
string manufacturerFlag(int m_field);
string mediaType(int a_field_device_type);
string mediaTypeJSON(int a_field_device_type);
string ciType(int ci_field);
string cType(int c_field);
string ccType(int cc_field);
string difType(int dif);
double vifScale(int vif);
string vifKey(int vif); // E.g. temperature energy power mass_flow volume_flow
string vifUnit(int vif); // E.g. m3 c kwh kw MJ MJh
string vifType(int vif); // Long description
string vifeType(int dif, int vif, int vife); // Long description
string formatData(int dif, int vif, int vife, string data);

double extract8bitAsDouble(int dif, int vif, int vife, string data);
double extract16bitAsDouble(int dif, int vif, int vife, string data);
double extract32bitAsDouble(int dif, int vif, int vife, string data);

int difLenBytes(int dif);
MeasurementType difMeasurementType(int dif);

string linkModeName(LinkMode link_mode);
string measurementTypeName(MeasurementType mt);

#endif
