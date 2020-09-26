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

#ifndef WMBUS_H
#define WMBUS_H

#include"manufacturers.h"
#include"serial.h"
#include"util.h"

#include<inttypes.h>
#include<map>

// Check and remove the data link layer CRCs from a wmbus telegram.
// If the CRCs do not pass the test, return false.
bool trimCRCsFrameFormatA(std::vector<uchar> &payload);
bool trimCRCsFrameFormatB(std::vector<uchar> &payload);

struct Device
{
    // A typical device is: FILE : SUFFIX : LINKMODES
    //
    //     FILE
    //     auto
    //     simulation_foo.txt
    //     rtlwmbus
    //     stdin
    //
    //     FILE : SUFFIX
    //     /dev/ttyUSB0:im871a
    //     /dev/ttyUSB1:9600
    //     rtlwmbus:434M
    //
    //     FILE : SUFFIX : LINKMODES
    //     /dev/ttyUSB0:amb8465:c1,t1
    //
    std::string file; // auto simulation_meter.txt, stdin, file.raw, rtlwmbus, /dev/ttyUSB0
    std::string suffix; // im871a, rtlwmbus, 9600, 868.9M, rtlwmbus-command line
    std::string linkmodes; // c1,t1,s1

    void clear()
    {
        file = "";
        suffix = "";
        linkmodes = "";
    }

    string str()
    {
        if (linkmodes != "") return file+":"+suffix+":"+linkmodes;
        if (suffix != "") return file+":"+suffix;
        return file;
    }
};

#define LIST_OF_MBUS_DEVICES \
    X(DEVICE_UNKNOWN) \
    X(DEVICE_CUL)\
    X(DEVICE_D1TC)\
    X(DEVICE_IM871A)\
    X(DEVICE_AMB8465)\
    X(DEVICE_RFMRX2)\
    X(DEVICE_SIMULATOR)\
    X(DEVICE_RC1180)\
    X(DEVICE_RTLWMBUS)\
    X(DEVICE_RTL433)\
    X(DEVICE_RAWTTY)\
    X(DEVICE_WMB13U)

enum WMBusDeviceType {
#define X(name) name,
LIST_OF_MBUS_DEVICES
#undef X
};

const char *toString(WMBusDeviceType t);

struct Detected
{
    Device device; // Device information.
    WMBusDeviceType type;  // IM871A, AMB8465 etc.
    int baudrate; // Baudrate to tty.
    // If the override_tty is true, then do not allow the wmbus driver to open the device->file as a tty,
    // instead open the device->file as a file instead . This is to allows feeding the wmbus drivers
    // using stdin or a file. This is primarily used for internal testing.
    bool override_tty;
    bool is_tty;
    bool is_file;
    bool is_stdin;

    void set(WMBusDeviceType t, int br, bool ot)
    {
        type = t;
        baudrate = br;
        override_tty = ot;
        is_tty = is_file = is_stdin = false;
    }
};

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

enum class CONNECTION
{
    MBUS, WMBUS, BOTH
};

enum class CI_TYPE
{
    ELL, NWL, AFL, TPL
};

enum class TPL_LENGTH
{
    NONE, SHORT, LONG
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

#define LIST_OF_ELL_SECURITY_MODES \
    X(NoSecurity, 0) \
    X(AES_CTR, 1) \
    X(RESERVED, 2)

enum class ELLSecurityMode {
#define X(name,nr) name,
LIST_OF_ELL_SECURITY_MODES
#undef X
};

int toInt(ELLSecurityMode esm);
const char *toString(ELLSecurityMode esm);
ELLSecurityMode fromIntToELLSecurityMode(int i);

#define LIST_OF_TPL_SECURITY_MODES \
    X(NoSecurity, 0) \
    X(MFCT_SPECIFIC, 1) \
    X(DES_NO_IV_DEPRECATED, 2) \
    X(DES_IV_DEPRECATED, 3) \
    X(SPECIFIC_4, 4) \
    X(AES_CBC_IV, 5) \
    X(RESERVED_6, 6) \
    X(AES_CBC_NO_IV, 7) \
    X(AES_CTR_CMAC, 8) \
    X(AES_CGM, 9) \
    X(AES_CCM, 10) \
    X(RESERVED_11, 11) \
    X(RESERVED_12, 12) \
    X(SPECIFIC_13, 13) \
    X(RESERVED_14, 14) \
    X(SPECIFIC_15, 15) \
    X(SPECIFIC_16_31, 16)

enum class TPLSecurityMode {
#define X(name,nr) name,
LIST_OF_TPL_SECURITY_MODES
#undef X
};

int toInt(TPLSecurityMode tsm);
TPLSecurityMode fromIntToTPLSecurityMode(int i);
const char *toString(TPLSecurityMode tsm);

#define LIST_OF_AFL_AUTH_TYPES \
    X(NoAuth, 0, 0)             \
    X(Reserved1, 1, 0)          \
    X(Reserved2, 2, 0)          \
    X(AES_CMAC_128_2, 3, 2)     \
    X(AES_CMAC_128_4, 4, 4)     \
    X(AES_CMAC_128_8, 5, 8)     \
    X(AES_CMAC_128_12, 6, 12)   \
    X(AES_CMAC_128_16, 7, 16)   \
    X(AES_GMAC_128_12, 8, 12)

enum class AFLAuthenticationType {
#define X(name,nr,len) name,
LIST_OF_AFL_AUTH_TYPES
#undef X
};

int toInt(AFLAuthenticationType aat);
AFLAuthenticationType fromIntToAFLAuthenticationType(int i);
const char *toString(AFLAuthenticationType aat);
int toLen(AFLAuthenticationType aat);

enum class MeasurementType
{
    Unknown,
    Instantaneous,
    Minimum,
    Maximum,
    AtError
};

struct DVEntry
{
    MeasurementType type {};
    int value_information {};
    int storagenr {};
    int tariff {};
    int subunit {};
    string value;

    DVEntry() {}
    DVEntry(MeasurementType mt, int vi, int st, int ta, int su, string &val) :
    type(mt), value_information(vi), storagenr(st), tariff(ta), subunit(su), value(val) {}
};

using namespace std;

struct MeterKeys
{
    vector<uchar> confidentiality_key;
    vector<uchar> authentication_key;

    bool hasConfidentialityKey() { return confidentiality_key.size() > 0; }
    bool hasAuthenticationKey() { return authentication_key.size() > 0; }
};

struct Telegram
{
    // The meter address as a string usually printed on the meter.
    string id;
    // If decryption failed, set this to true, to prevent further processing.
    bool decryption_failed {};

    // DLL
    int dll_len {}; // The length of the telegram, 1 byte.
    int dll_c {};   // 1 byte control code, SND_NR=0x44

    uchar dll_mfct_b[2]; //  2 bytes
    int dll_mfct {};

    vector<uchar> dll_a; // A field 6 bytes
    // The 6 a field bytes are composed of:
    uchar dll_id_b[4] {};    // 4 bytes, address in BCD = 8 decimal 00000000...99999999 digits.
    vector<uchar> dll_id; // 4 bytes, human readable order.
    uchar dll_version {}; // 1 byte
    uchar dll_type {}; // 1 byte

    // ELL
    uchar ell_ci {}; // 1 byte
    uchar ell_cc {}; // 1 byte
    uchar ell_acc {}; // 1 byte
    uchar ell_sn_b[4] {}; // 4 bytes
    int   ell_sn {}; // 4 bytes
    uchar ell_sn_session {}; // 4 bits
    int   ell_sn_time {}; // 25 bits
    uchar ell_sn_sec {}; // 3 bits
    ELLSecurityMode ell_sec_mode {}; // Based on 3 bits from above.
    uchar ell_pl_crc_b[2] {}; // 2 bytes
    uint16_t ell_pl_crc {}; // 2 bytes

    uchar ell_mfct_b[2] {}; // 2 bytes;
    int   ell_mfct {};
    bool  ell_id_found {};
    uchar ell_id_b[6] {}; // 4 bytes;
    uchar ell_version {}; // 1 byte
    uchar ell_type {};  // 1 byte

    // NWL
    int nwl_ci {}; // 1 byte

    // AFL
    uchar afl_ci {}; // 1 byte
    uchar afl_len {}; // 1 byte
    uchar afl_fc_b[2] {}; // 2 byte fragmentation control
    uint16_t afl_fc {};
    uchar afl_mcl {}; // 1 byte message control

    bool afl_ki_found {};
    uchar afl_ki_b[2] {}; // 2 byte key information
    uint16_t afl_ki {};

    bool afl_counter_found {};
    uchar afl_counter_b[4] {}; // 4 bytes
    uint32_t afl_counter {};

    bool afl_mlen_found {};
    int afl_mlen {};

    bool must_check_mac {};
    vector<uchar> afl_mac_b;

    // TPL
    vector<uchar>::iterator tpl_start;
    int tpl_ci {}; // 1 byte
    int tpl_acc {}; // 1 byte
    int tpl_sts {}; // 1 byte
    int tpl_cfg {}; // 2 bytes
    TPLSecurityMode tpl_sec_mode {}; // Based on 5 bits extracted from cfg.
    int tpl_num_encr_blocks {};
    int tpl_cfg_ext {}; // 1 byte
    int tpl_kdf_selection {}; // 1 byte
    vector<uchar> tpl_generated_key; // 16 bytes
    vector<uchar> tpl_generated_mac_key; // 16 bytes

    bool  tpl_id_found {}; // If set to true, then tpl_id_b contains valid values.
    uchar tpl_id_b[4] {}; // 4 bytes
    uchar tpl_mfct_b[2] {}; // 2 bytes
    int   tpl_mfct {};
    uchar tpl_version {}; // 1 bytes
    uchar tpl_type {}; // 1 bytes

    // The format signature is used for compact frames.
    int format_signature {};

    vector<uchar> frame; // Content of frame, potentially decrypted.
    vector<uchar> parsed;  // Parsed bytes with explanations.
    int header_size {}; // Size of headers before the APL content.
    int suffix_size {}; // Size of suffix after the APL content. Usually empty, but can be MACs!
    int mfct_0f_index = -1; // -1 if not found, else index of the 0f byte, if found, inside the difvif data after the header.
    void extractFrame(vector<uchar> *fr); // Extract to full frame.
    void extractPayload(vector<uchar> *pl); // Extract frame data containing the measurements, after the header and not the suffix.
    void extractMfctData(vector<uchar> *pl); // Extract frame data after the DIF 0x0F.

    bool handled {}; // Set to true, when a meter has accepted the telegram.

    bool parseHeader(vector<uchar> &input_frame);
    bool parse(vector<uchar> &input_frame, MeterKeys *mk);
    void parserNoWarnings() { parser_warns_ = false; }
    void print();

    // A vector of indentations and explanations, to be printed
    // below the raw data bytes to explain the telegram content.
    vector<pair<int,string>> explanations;
    void addExplanationAndIncrementPos(vector<uchar>::iterator &pos, int len, const char* fmt, ...);
    void addMoreExplanation(int pos, const char* fmt, ...);
    void explainParse(string intro, int from);

    bool isSimulated() { return is_simulated_; }
    void markAsSimulated() { is_simulated_ = true; }

    // Extracted mbus values.
    std::map<std::string,std::pair<int,DVEntry>> values;

    string autoDetectPossibleDrivers();

private:

    bool is_simulated_ {};
    bool parser_warns_ = true;
    MeterKeys *meter_keys {};

    bool parseDLL(std::vector<uchar>::iterator &pos);
    bool parseELL(std::vector<uchar>::iterator &pos);
    bool parseNWL(std::vector<uchar>::iterator &pos);
    bool parseAFL(std::vector<uchar>::iterator &pos);
    bool parseTPL(std::vector<uchar>::iterator &pos);

    void printDLL();
    void printELL();
    void printNWL();
    void printAFL();
    void printTPL();

    bool parse_TPL_72(vector<uchar>::iterator &pos);
    bool parse_TPL_78(vector<uchar>::iterator &pos);
    bool parse_TPL_79(vector<uchar>::iterator &pos);
    bool parse_TPL_7A(vector<uchar>::iterator &pos);
    bool potentiallyDecrypt(vector<uchar>::iterator &pos);
    bool parseTPLConfig(std::vector<uchar>::iterator &pos);
    static string toStringFromELLSN(int sn);
    static string toStringFromTPLConfig(int cfg);
    static string toStringFromAFLFC(int fc);
    static string toStringFromAFLMC(int mc);

    bool parseShortTPL(std::vector<uchar>::iterator &pos);
    bool parseLongTPL(std::vector<uchar>::iterator &pos);
    bool checkMAC(std::vector<uchar> &frame,
                  std::vector<uchar>::iterator from,
                  std::vector<uchar>::iterator to,
                  std::vector<uchar> &mac,
                  std::vector<uchar> &mackey);
    bool findFormatBytesFromKnownMeterSignatures(std::vector<uchar> *format_bytes);
};

struct Meter;

struct WMBus
{
    virtual WMBusDeviceType type() = 0;
    virtual std::string device() = 0;
    virtual bool ping() = 0;
    virtual uint32_t getDeviceId() = 0;
    virtual LinkModeSet getLinkModes() = 0;
    virtual LinkModeSet supportedLinkModes() = 0;
    virtual int numConcurrentLinkModes() = 0;
    virtual bool canSetLinkModes(LinkModeSet lms) = 0;
    virtual void setLinkModes(LinkModeSet lms) = 0;
    virtual void onTelegram(function<bool(vector<uchar>)> cb) = 0;
    virtual SerialDevice *serial() = 0;
    virtual void simulate() = 0;
    // Return true if underlying device is ok and device in general seems to be working.
    virtual bool isWorking() = 0;
    // This will check if the wmbus devices needs a reset and then immediately perform the reset.
    virtual void checkStatus() = 0;
    // Close any underlying ttys or software and restart/reinitialize.
    // Return true if ok.
    virtual bool reset() = 0;
    // Set a dead-mans grip timeout, if no telegram is received
    // within seconds, then invoke reset(). However do not reset
    // when no activity is expected.
    virtual void setTimeout(int seconds, std::string expected_activity) = 0;
    // Set a regular interval for resetting the wmbus device.
    // Default is once ever 24 hours.
    virtual void setResetInterval(int seconds) = 0;
    virtual ~WMBus() = 0;
};

Detected detectWMBusDeviceSetting(string devicefile,
                                  string suffix,
                                  string linkmodes,
                                  shared_ptr<SerialCommunicationManager> manager);


bool isPossibleDevice(string arg, Device *device);

shared_ptr<WMBus> openIM871A(string device, shared_ptr<SerialCommunicationManager> manager,
                             shared_ptr<SerialDevice> serial_override);
shared_ptr<WMBus> openAMB8465(string device, shared_ptr<SerialCommunicationManager> manager,
                              shared_ptr<SerialDevice> serial_override);
shared_ptr<WMBus> openRawTTY(string device, int baudrate, shared_ptr<SerialCommunicationManager> manager,
                             shared_ptr<SerialDevice> serial_override);
shared_ptr<WMBus> openRC1180(string device, shared_ptr<SerialCommunicationManager> manager,
                             shared_ptr<SerialDevice> serial_override);
shared_ptr<WMBus> openRTLWMBUS(string device, string command, shared_ptr<SerialCommunicationManager> manager, std::function<void()> on_exit,
                               shared_ptr<SerialDevice> serial_override);
shared_ptr<WMBus> openRTL433(string device, string command, shared_ptr<SerialCommunicationManager> manager, std::function<void()> on_exit,
                             shared_ptr<SerialDevice> serial_override);
shared_ptr<WMBus> openCUL(string device, shared_ptr<SerialCommunicationManager> manager,
                              shared_ptr<SerialDevice> serial_override);
shared_ptr<WMBus> openD1TC(string device, shared_ptr<SerialCommunicationManager> manager,
                           shared_ptr<SerialDevice> serial_override);
shared_ptr<WMBus> openWMB13U(string device, shared_ptr<SerialCommunicationManager> manager,
                             shared_ptr<SerialDevice> serial_override);
shared_ptr<WMBus> openSimulator(string file, shared_ptr<SerialCommunicationManager> manager,
                                shared_ptr<SerialDevice> serial_override);

string manufacturer(int m_field);
string manufacturerFlag(int m_field);
string mediaType(int a_field_device_type);
string mediaTypeJSON(int a_field_device_type);
bool isCiFieldOfType(int ci_field, CI_TYPE type);
int ciFieldLength(int ci_field);
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

AccessCheck findAndDetect(shared_ptr<SerialCommunicationManager> manager,
                          string *out_device,
                          function<AccessCheck(string,shared_ptr<SerialCommunicationManager>)> check,
                          string dongle_name,
                          string device_root);

AccessCheck checkAccessAndDetect(shared_ptr<SerialCommunicationManager> manager,
                                 function<AccessCheck(string,shared_ptr<SerialCommunicationManager>)> check,
                                 string dongle_name,
                                 string device);

enum FrameStatus { PartialFrame, FullFrame, ErrorInFrame, TextAndNotFrame };


FrameStatus checkWMBusFrame(vector<uchar> &data,
                            size_t *frame_length,
                            int *payload_len_out,
                            int *payload_offset);

AccessCheck detectIM871A(string file, Detected *detected, shared_ptr<SerialCommunicationManager> handler);
AccessCheck detectAMB8465(string file, Detected *detected, shared_ptr<SerialCommunicationManager> handler);
AccessCheck detectRawTTY(string file, int baud, Detected *detected, shared_ptr<SerialCommunicationManager> handler);
AccessCheck detectRC1180(string file, Detected *detected, shared_ptr<SerialCommunicationManager> handler);
AccessCheck detectRTLSDR(string file, Detected *detected, shared_ptr<SerialCommunicationManager> handler);
AccessCheck detectCUL(string file, Detected *detected, shared_ptr<SerialCommunicationManager> handler);
AccessCheck detectWMB13U(string file, Detected *detected, shared_ptr<SerialCommunicationManager> handler);

// Try to factory reset an AMB8465 by trying all possible serial speeds and
// restore to factory settings.
AccessCheck factoryResetAMB8465(string device, shared_ptr<SerialCommunicationManager> handler, int *was_baud);

Detected detectImstAmberCulRC(string file,
                            string suffix,
                            string linkmodes,
                            shared_ptr<SerialCommunicationManager> handler,
                            bool is_tty,
                            bool is_stdin,
                            bool is_file);

#endif
