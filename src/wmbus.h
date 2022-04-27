/*
 Copyright (C) 2017-2022 Fredrik Öhrström (gpl-3.0-or-later)

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

#include"dvparser.h"
#include"manufacturers.h"
#include"serial.h"
#include"util.h"

#include<inttypes.h>
#include<map>
#include<set>

// Check and remove the data link layer CRCs from a wmbus telegram.
// If the CRCs do not pass the test, return false.
void removeAnyDLLCRCs(std::vector<uchar> &payload);
bool trimCRCsFrameFormatA(std::vector<uchar> &payload);
bool trimCRCsFrameFormatB(std::vector<uchar> &payload);

#define LIST_OF_MBUS_DEVICES \
    X(UNKNOWN,unknown,false,false,detectUNKNOWN)     \
    X(MBUS,mbus,true,false,detectMBUS)               \
    X(AUTO,auto,false,false,detectAUTO)              \
    X(AMB8465,amb8465,true,false,detectAMB8465)      \
    X(CUL,cul,true,false,detectCUL)                  \
    X(IM871A,im871a,true,false,detectIM871AIM170A)   \
    X(IM170A,im170a,true,false,detectSKIP)           \
    X(RAWTTY,rawtty,true,false,detectRAWTTY)         \
    X(HEXTTY,hextty,true,false,detectSKIP)           \
    X(RC1180,rc1180,true,false,detectRC1180)         \
    X(RTL433,rtl433,false,true,detectRTL433)         \
    X(RTLWMBUS,rtlwmbus,false,true,detectRTLWMBUS)   \
    X(IU880B,iu880b,true,false,detectIU880B)         \
    X(SIMULATION,simulation,false,false,detectSIMULATION)

enum WMBusDeviceType {
#define X(name,text,tty,rtlsdr,detector) DEVICE_ ## name,
LIST_OF_MBUS_DEVICES
#undef X
};

enum class ContentStartsWith { C_FIELD, CI_FIELD, SHORT_FRAME, LONG_FRAME };
const char *toString(ContentStartsWith sw);

bool usesTTY(WMBusDeviceType t);
bool usesRTLSDR(WMBusDeviceType t);
const char *toString(WMBusDeviceType t);
const char *toLowerCaseString(WMBusDeviceType t);
WMBusDeviceType toWMBusDeviceType(string &t);

void setIgnoreDuplicateTelegrams(bool idt);

// In link mode S1, is used when both the transmitter and receiver are stationary.
// It can be transmitted relatively seldom.

// In link mode T1, the meter transmits a telegram every few seconds or minutes.
// Suitable for drive-by/walk-by collection of meter values.

// Link mode C1 is like T1 but uses less energy when transmitting due to
// a different radio encoding. Also significant is:
// S1/T1 usually uses the A format for the data link layer, more CRCs.
// C1 usually uses the B format for the data link layer, less CRCs = less overhead.

// The im871a can for example receive C1a, but it is unclear if there are any meters that use it.

#define LIST_OF_LINK_MODES \
    X(Any,any,--anylinkmode,0xffff)             \
    X(C1,c1,--c1,0x1)                           \
    X(S1,s1,--s1,0x2)                           \
    X(S1m,s1m,--s1m,0x4)                        \
    X(T1,t1,--t1,0x8)                           \
    X(N1a,n1a,--n1a,0x10)                       \
    X(N1b,n1b,--n1b,0x20)                       \
    X(N1c,n1c,--n1c,0x40)                       \
    X(N1d,n1d,--n1d,0x80)                       \
    X(N1e,n1e,--n1e,0x100)                      \
    X(N1f,n1f,--n1f,0x200)                      \
    X(MBUS,mbus,--mbus,0x400)                   \
    X(LORA,lora,--lora,0x800)                   \
    X(UNKNOWN,unknown,----,0x0)

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
    LinkModeSet &addLinkMode(LinkMode lm);
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
    // Check if any link mode has been set.
    bool empty() { return set_ == 0; }
    // Clear the set to empty.
    void clear() { set_ = 0; }
    // Mark set as all linkmodes!
    void setAll() { set_ = (int)LinkMode::Any; }
    // For bit counting etc.
    int asBits() { return set_; }

    // Return a human readable string.
    std::string hr();

    LinkModeSet() { }
    LinkModeSet(int s) : set_(s) {}

private:

    int set_ {};
};

LinkModeSet parseLinkModes(string modes);
bool isValidLinkModes(string modes);

// A wmbus specified device is supplied on the command line or in the config file.
// It has this format "alias=file:type(id):fq:bps:linkmods:CMD(command)"
struct SpecifiedDevice
{
    std::string bus_alias; // A bus alias, necessary for C2/T2 meters and mbus.
    int index; // 0,1,2,3 the order on the command line / config file.
    std::string file; // simulation_meter.txt, stdin, file.raw, /dev/ttyUSB0
    std::string hex; // a hex string supplied on the command line becomes a hex simulation.
    bool is_tty{}, is_stdin{}, is_file{}, is_simulation{}, is_hex_simulation{};
    WMBusDeviceType type; // im871a, rtlwmbus
    std::string id; // 12345678 for wmbus dongles or 0,1 for rtlwmbus indexes.
    std::string extras; // Extra device specific settings.
    std::string fq; // 868.95M
    std::string bps; // 9600
    LinkModeSet linkmodes; // c1,t1,s1
    std::string command; // command line of background process that streams data into wmbusmeters

    bool handled {}; // Set to true when this device has been detected/handled.
    time_t last_alarm {}; // Last time an alarm was sent for this device not being found.

    void clear();
    string str();
    bool parse(string &s);
    static bool isLikelyDevice(string &s);
};

struct Detected
{
    SpecifiedDevice specified_device {}; // Device as specified from the command line / config file.

    string found_file; // The device file to use.
    string found_hex;  // An immediate hex string is supplied.
    string found_device_id; // An "unique" identifier, typically the id used by the dongle as its own wmbus id, if it transmits.
    WMBusDeviceType found_type {};  // IM871A, AMB8465 etc.
    int found_bps {}; // Serial speed of tty.
    bool found_tty_override {};

    void setSpecifiedDevice(SpecifiedDevice sd)
    {
        specified_device = sd;
    }

    void setAsFound(string id, WMBusDeviceType t, int b, bool to, LinkModeSet clm)
    {
        found_device_id = id;
        found_type = t;
        found_bps = b;
        found_tty_override = to;
    }

    std::string str()
    {
        return found_file+":"+string(toString(found_type))+"["+found_device_id+"]"+":"+to_string(found_bps)+"/"+to_string(found_tty_override);
    }
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

using namespace std;

struct MeterKeys
{
    vector<uchar> confidentiality_key;
    vector<uchar> authentication_key;

    bool hasConfidentialityKey() { return confidentiality_key.size() > 0; }
    bool hasAuthenticationKey() { return authentication_key.size() > 0; }
};

enum class FrameType
{
    WMBUS,
    MBUS,
    HAN
};

const char *toString(FrameType ft);

struct AboutTelegram
{
    // wmbus device used to receive this telegram.
    string device;
    // The device's opinion of the rssi, best effort conversion into the dbm scale.
    // -100 dbm = 0.1 pico Watt to -20 dbm = 10 micro W
    // Measurements smaller than -100 and larger than -10 are unlikely.
    int rssi_dbm {};
    // WMBus or MBus
    FrameType type {};

    AboutTelegram(string dv, int rs, FrameType t) : device(dv), rssi_dbm(rs), type(t) {}
    AboutTelegram() {}
};

// Mark understood bytes as either PROTOCOL, ie dif vif, acc and other header bytes.
// Or CONTENT, ie the value fields found inside the transport layer.
enum class KindOfData
{
    PROTOCOL, CONTENT
};

// Content can be not understood at all NONE, partially understood PARTIAL when typically bitsets have
// been partially decoded, or FULL when the volume or energy field is by itself complete.
// Encrypted if it yet decrypted. Compressed and no format signature is known.
enum class Understanding
{
    NONE, ENCRYPTED, COMPRESSED, PARTIAL, FULL
};

struct Explanation
{
    int pos {};
    int len {};
    string info;
    KindOfData kind {};
    Understanding understanding {};

    Explanation(int p, int l, const string &i, KindOfData k, Understanding u) :
        pos(p), len(l), info(i), kind(k), understanding(u) {}
};

struct Telegram
{
private:
    Telegram(Telegram&t) { }

public:
    Telegram() = default;

    AboutTelegram about;

    // If a warning is printed mark this.
    bool triggered_warning {};

    // The different ids found, the first is th dll_id, ell_id, nwl_id, and the last is the tpl_id.
    vector<string> ids;
    // Ids separated by commas
    string idsc;

    // If decryption failed, set this to true, to prevent further processing.
    bool decryption_failed {};

    // DLL
    int dll_len {}; // The length of the telegram, 1 byte.
    int dll_c {};   // 1 byte control code, SND_NR=0x44

    uchar dll_mfct_b[2]; //  2 bytes
    int dll_mfct {};

    uchar mbus_primary_address; // Single byte address 0-250 for mbus devices.
    uchar mbus_ci; // MBus control information field.

    vector<uchar> dll_a; // A field 6 bytes
    // The 6 a field bytes are composed of 4 id bytes, version and type.
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
    int tpl_sts_offset {}; // Remember where the sts field is in the telegram, so
                           // that we can add more vendor specific decodings to it.
    int tpl_cfg {}; // 2 bytes
    TPLSecurityMode tpl_sec_mode {}; // Based on 5 bits extracted from cfg.
    int tpl_num_encr_blocks {};
    int tpl_cfg_ext {}; // 1 byte
    int tpl_kdf_selection {}; // 1 byte
    vector<uchar> tpl_generated_key; // 16 bytes
    vector<uchar> tpl_generated_mac_key; // 16 bytes

    bool  tpl_id_found {}; // If set to true, then tpl_id_b contains valid values.
    vector<uchar> tpl_a; // A field 6 bytes
    // The 6 a field bytes are composed of 4 id bytes, version and type.
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
    bool parse(vector<uchar> &input_frame, MeterKeys *mk, bool warn);

    bool parseMBUSHeader(vector<uchar> &input_frame);
    bool parseMBUS(vector<uchar> &input_frame, MeterKeys *mk, bool warn);

    bool parseWMBUSHeader(vector<uchar> &input_frame);
    bool parseWMBUS(vector<uchar> &input_frame, MeterKeys *mk, bool warn);

    bool parseHANHeader(vector<uchar> &input_frame);
    bool parseHAN(vector<uchar> &input_frame, MeterKeys *mk, bool warn);

    void print();

    // A vector of indentations and explanations, to be printed
    // below the raw data bytes to explain the telegram content.
    vector<Explanation> explanations;
    void addExplanationAndIncrementPos(vector<uchar>::iterator &pos, int len, KindOfData k, Understanding u, const char* fmt, ...);
    void addMoreExplanation(int pos, const char* fmt, ...);
    void addMoreExplanation(int pos, string json);

    // Add an explanation of data inside manufacturer specific data.
    void addSpecialExplanation(int offset, int len, KindOfData k, Understanding u, const char* fmt, ...);
    void explainParse(string intro, int from);
    string analyzeParse(OutputFormat o, int *content_length, int *understood_content_length);

    bool parserWarns() { return parser_warns_; }
    bool isSimulated() { return is_simulated_; }
    bool beingAnalyzed() { return being_analyzed_; }
    void markAsSimulated() { is_simulated_ = true; }
    void markAsBeingAnalyzed() { being_analyzed_ = true; }

    // The actual content of the (w)mbus telegram. The DifVif entries.
    // Mapped from their key for quick access to their offset and content.
    std::map<std::string,std::pair<int,DVEntry>> dv_entries;

    string autoDetectPossibleDrivers();

    // part of original telegram bytes, only filled if pre-processing modifies it
    vector<uchar> original;

private:

    bool is_simulated_ {};
    bool being_analyzed_ {};
    bool parser_warns_ = true;
    MeterKeys *meter_keys {};

    // Fixes quirks from non-compliant meters to make telegram compatible with the standard
    void preProcess();

    bool parseMBusDLLandTPL(std::vector<uchar>::iterator &pos);

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
    bool alreadyDecryptedCBC(vector<uchar>::iterator &pos);
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

struct SendBusContent
{
    string bus;
    string content;
    ContentStartsWith starts_with;

    static bool isLikely(const string &s);
    bool parse(const string &s);
};

struct Meter;

struct WMBus
{
    // Each bus can be given an alias name to be
    // referred to from meters.
    virtual std::string busAlias() = 0;

    // I wmbus device identifier consists of:
    // device:type[id] for example:
    // /dev/ttyUSB1:im871a[12345678]

    virtual std::string device() = 0;
    virtual WMBusDeviceType type() = 0;
    // The device id is the changeable id of the dongle.
    // For im871a,amb8465 it is the transmit address.
    // For rtlsdr it is the id set using rtl_eeprom.
    // Not all dongles have this.
    virtual string getDeviceId() = 0;
    // The im871a and amb8465 dongles does have a unique, immutable id as well.
    // Not all dongles have this.
    virtual string getDeviceUniqueId() = 0;
    // Human readable explanation of this device, eg: /dev/ttysUB0:im871a[12345678]:t1
    virtual std::string hr() = 0;
    virtual bool isSerial() = 0;

    virtual LinkModeSet getLinkModes() = 0;
    virtual bool ping() = 0;
    virtual LinkModeSet supportedLinkModes() = 0;
    virtual int numConcurrentLinkModes() = 0;
    virtual bool canSetLinkModes(LinkModeSet lms) = 0;
    virtual void setLinkModes(LinkModeSet lms) = 0;
    virtual void onTelegram(function<bool(AboutTelegram&,vector<uchar>)> cb) = 0;
    virtual bool sendTelegram(ContentStartsWith starts_with, vector<uchar> &content) = 0;
    virtual SerialDevice *serial() = 0;
    // Return true of the serial has been overridden, usually with stdin or a file.
    virtual bool serialOverride() = 0;
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
    // Close this device.
    virtual void close() = 0;
    // Remember how this device was detected.
    virtual void setDetected(Detected detected) = 0;
    virtual Detected *getDetected() = 0;

    virtual ~WMBus() = 0;
};

Detected detectWMBusDeviceWithFileOrHex(SpecifiedDevice &specified_device,
                                        LinkModeSet default_linkmodes,
                                        shared_ptr<SerialCommunicationManager> manager);
Detected detectWMBusDeviceWithCommand(SpecifiedDevice &specified_device,
                                      LinkModeSet default_linkmodes,
                                      shared_ptr<SerialCommunicationManager> handler);


shared_ptr<WMBus> openIM871A(Detected detected,
                             shared_ptr<SerialCommunicationManager> manager,
                             shared_ptr<SerialDevice> serial_override);
shared_ptr<WMBus> openIM170A(Detected detected,
                             shared_ptr<SerialCommunicationManager> manager,
                             shared_ptr<SerialDevice> serial_override);
shared_ptr<WMBus> openIU880B(Detected detected,
                             shared_ptr<SerialCommunicationManager> manager,
                             shared_ptr<SerialDevice> serial_override);
shared_ptr<WMBus> openAMB8465(Detected detected,
                              shared_ptr<SerialCommunicationManager> manager,
                              shared_ptr<SerialDevice> serial_override);
shared_ptr<WMBus> openRawTTY(Detected detected,
                             shared_ptr<SerialCommunicationManager> manager,
                             shared_ptr<SerialDevice> serial_override);
shared_ptr<WMBus> openHexTTY(Detected detected,
                             shared_ptr<SerialCommunicationManager> manager,
                             shared_ptr<SerialDevice> serial_override);
shared_ptr<WMBus> openMBUS(Detected detected,
                           shared_ptr<SerialCommunicationManager> manager,
                           shared_ptr<SerialDevice> serial_override);
shared_ptr<WMBus> openRC1180(Detected detected,
                             shared_ptr<SerialCommunicationManager> manager,
                             shared_ptr<SerialDevice> serial_override);
shared_ptr<WMBus> openRTLWMBUS(Detected detected,
                               string bin_dir,
                               bool daemon,
                               shared_ptr<SerialCommunicationManager> manager,
                               shared_ptr<SerialDevice> serial_override);
shared_ptr<WMBus> openRTL433(Detected detected,
                             string bin_dir,
                             bool daemon,
                             shared_ptr<SerialCommunicationManager> manager,
                             shared_ptr<SerialDevice> serial_override);
shared_ptr<WMBus> openCUL(Detected detected,
                          shared_ptr<SerialCommunicationManager> manager,
                          shared_ptr<SerialDevice> serial_override);
shared_ptr<WMBus> openSimulator(Detected detected,
                                shared_ptr<SerialCommunicationManager> manager,
                                shared_ptr<SerialDevice> serial_override);

string manufacturer(int m_field);
string manufacturerFlag(int m_field);
bool flagToManufacturer(const char *s, uint16_t *out_mfct);
string mediaType(int a_field_device_type, int m_field);
string mediaTypeJSON(int a_field_device_type, int m_field);
bool isCiFieldOfType(int ci_field, CI_TYPE type);
int ciFieldLength(int ci_field);
string ciType(int ci_field);
string cType(int c_field);
bool isValidWMBusCField(int c_field);
bool isValidMBusCField(int c_field);
string ccType(int cc_field);
string difType(int dif);
double vifScale(int vif);
string vifKey(int vif); // E.g. temperature energy power mass_flow volume_flow
string vifUnit(int vif); // E.g. m3 c kwh kw MJ MJh
string vifType(int vif); // Long description
string vifeType(int dif, int vif, int vife); // Long description
string formatData(int dif, int vif, int vife, string data);

// Decode the status byte in the TPL with a map that gives the
// translations for the 3 vendor specific high bits.
string decodeTPLStatusByte(uchar sts, std::map<int,std::string> *vendor_lookup);

int difLenBytes(int dif);
MeasurementType difMeasurementType(int dif);

string linkModeName(LinkMode link_mode);
string measurementTypeName(MeasurementType mt);

enum FrameStatus { PartialFrame, FullFrame, ErrorInFrame, TextAndNotFrame };


FrameStatus checkWMBusFrame(vector<uchar> &data,
                            size_t *frame_length,
                            int *payload_len_out,
                            int *payload_offset,
                            bool only_test);

FrameStatus checkMBusFrame(vector<uchar> &data,
                           size_t *frame_length,
                           int *payload_len_out,
                           int *payload_offset,
                           bool only_test);

AccessCheck reDetectDevice(Detected *detected, shared_ptr<SerialCommunicationManager> handler);

AccessCheck detectAUTO(Detected *detected, shared_ptr<SerialCommunicationManager> handler);
AccessCheck detectAMB8465(Detected *detected, shared_ptr<SerialCommunicationManager> handler);
AccessCheck detectCUL(Detected *detected, shared_ptr<SerialCommunicationManager> handler);
AccessCheck detectD1TC(Detected *detected, shared_ptr<SerialCommunicationManager> manager);
AccessCheck detectIM871AIM170A(Detected *detected, shared_ptr<SerialCommunicationManager> handler);
AccessCheck detectIU880B(Detected *detected, shared_ptr<SerialCommunicationManager> handler);
AccessCheck detectRAWTTY(Detected *detected, shared_ptr<SerialCommunicationManager> handler);
AccessCheck detectMBUS(Detected *detected, shared_ptr<SerialCommunicationManager> handler);
AccessCheck detectRC1180(Detected *detected, shared_ptr<SerialCommunicationManager> handler);
AccessCheck detectRTL433(Detected *detected, shared_ptr<SerialCommunicationManager> handler);
AccessCheck detectRTLWMBUS(Detected *detected, shared_ptr<SerialCommunicationManager> handler);
AccessCheck detectSKIP(Detected *detected, shared_ptr<SerialCommunicationManager> handler);

// Try to factory reset an AMB8465 by trying all possible serial speeds and
// restore to factory settings.
AccessCheck factoryResetAMB8465(string tty, shared_ptr<SerialCommunicationManager> handler, int *was_baud);

Detected detectWMBusDeviceOnTTY(string tty,
                                set<WMBusDeviceType> probe_for,
                                LinkModeSet desired_linkmodes,
                                shared_ptr<SerialCommunicationManager> handler);

// Remember meters id/mfct/ver/type combos that we should only warn once for.
bool warned_for_telegram_before(Telegram *t, vector<uchar> &dll_a);

////////////////// MBUS

const char *mbusCField(uchar c_field);
const char *mbusCiField(uchar ci_field);

int genericifyMedia(int media);
bool isCloseEnough(int media1, int media2);

#endif
