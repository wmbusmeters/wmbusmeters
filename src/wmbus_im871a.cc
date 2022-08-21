/*
 Copyright (C) 2017-2021 Fredrik Öhrström (gpl-3.0-or-later)

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
#include"wmbus_common_implementation.h"
#include"wmbus_utils.h"
#include"wmbus_im871a.h"
#include"serial.h"
#include"threads.h"

#include<assert.h>
#include<pthread.h>
#include<errno.h>
#include<memory.h>
#include<semaphore.h>
#include<unistd.h>

using namespace std;

// 15 is like 14 but with some bug fixes
#define FIRMWARE_15_C_AND_T 0x15
// 14 is the first version to support both C and T at the same time.
#define FIRMWARE_14_C_AND_T 0x14
#define FIRMWARE_13_C_OR_T  0x13

struct IM871ADeviceInfo
{
    uchar module_type; // 0x33 = im871a 0x36 = im170a
    uchar device_mode; // 0 = collector(other) 1 = meter
    uchar firmware_version; // 13 hci 1.6 and 14 hci 1.7
    uchar hci_version;  // serial protocol?
    uint32_t uid;

    string str()
    {
        string s;
        s += "type=";
        if (module_type == 0x33) s+="im871a ";
        else if (module_type == 0x36) s+="im170a ";
        else s+="unknown_type("+to_string(module_type)+") ";

        s += "mode=";
        if (device_mode == 0) s+="collector ";
        else if (device_mode == 1) s+="meter ";
        else s+="unknown_mode("+to_string(device_mode)+") ";

        string ss;
        strprintf(ss, "firmware=%02x hci=%02x uid=%08x", firmware_version, hci_version, uid);
        return s+ss;
    }

    bool decode(vector<uchar> &bytes)
    {
        if (bytes.size() < 8) return false;
        int i = 0;
        module_type = bytes[i++];
        device_mode = bytes[i++];
        firmware_version = bytes[i++];
        hci_version = bytes[i++];
        uid = bytes[i+3]<<24|bytes[i+2]<<16|bytes[i+1]<<8|bytes[i]; i+=4;
        return true;
    }
};

struct Config
{
    // first variable group
    uchar device_mode;
    uchar link_mode;
    uchar c_field;
    uint16_t mfct;
    uint32_t id;
    uchar version;
    uchar media;
    uchar radio_channel;

    // second variable group
    uchar radio_power_level;
    uchar radio_data_rate;
    uchar radio_rx_window;
    uchar auto_power_saving;
    uchar auto_rssi;
    uchar auto_rx_timestamp;
    uchar led_control;
    uchar rtc_control;

    string dongleId()
    {
        string s;
        strprintf(s, "%08x", id);
        return s;
    }

    string str()
    {
        string s;

        if (device_mode == 0) s+="other ";
        else if (device_mode == 1) s+="meter ";
        else s+="unknown_mode("+to_string(device_mode)+") ";

        s += "link_mode="+toString(LinkModeIM871A(link_mode));

        string ids;
        strprintf(ids, " id=%08x media=%02x version=%02x c_field=%02x auto_rssi=%02x", id, media, version, c_field, auto_rssi);

        return s+ids;
    }

    bool decode(vector<uchar> &bytes)
    {
        if (bytes.size() < 2) return false;
        size_t i = 0;
        uchar iiflag1 = bytes[i++]; if (i >= bytes.size()) return false;
        if (iiflag1 & 0x01) { device_mode = bytes[i++]; } if (i >= bytes.size()) return false;
        if (iiflag1 & 0x02) { link_mode = bytes[i++]; } if (i >= bytes.size()) return false;
        if (iiflag1 & 0x04) { c_field = bytes[i++]; } if (i+1 >= bytes.size()) return false;
        if (iiflag1 & 0x08) { mfct = bytes[i+1]<<8|bytes[i]; i+=2; } if (i+3 >= bytes.size()) return false;
        if (iiflag1 & 0x10) { id = bytes[i+3]<<24|bytes[i+2]<<16|bytes[i+1]<<8|bytes[i]; i+=4; } if (i >= bytes.size()) return false;
        if (iiflag1 & 0x20) { version = bytes[i++]; } if (i >= bytes.size()) return false;
        if (iiflag1 & 0x40) { media = bytes[i++]; } if (i >= bytes.size()) return false;
        if (iiflag1 & 0x80) { radio_channel = bytes[i++]; } if (i >= bytes.size()) return false;

        uchar iiflag2 = bytes[i++]; if (i >= bytes.size()) return false;
        if (iiflag2 & 0x01) { radio_power_level = bytes[i++]; } if (i >= bytes.size()) return false;
        if (iiflag2 & 0x02) { radio_data_rate = bytes[i++]; } if (i >= bytes.size()) return false;
        if (iiflag2 & 0x04) { radio_rx_window = bytes[i++]; } if (i >= bytes.size()) return false;
        if (iiflag2 & 0x08) { auto_power_saving = bytes[i++]; } if (i >= bytes.size()) return false;
        if (iiflag2 & 0x10) { auto_rssi = bytes[i++]; } if (i >= bytes.size()) return false;
        if (iiflag2 & 0x20) { auto_rx_timestamp = bytes[i++]; } if (i >= bytes.size()) return false;
        if (iiflag2 & 0x40) { led_control = bytes[i++]; } if (i >= bytes.size()) return false;
        if (iiflag2 & 0x80) { rtc_control = bytes[i++]; }

        return true;
    }
};

string toString(LinkModeIM871A lm)
{
    switch (lm)
    {
#define X(name,text) case LinkModeIM871A::name: return #text;
LIST_OF_IM871A_LINK_MODES
#undef X
    }
    return "unknown";
}

struct WMBusIM871aIM170A : public virtual BusDeviceCommonImplementation
{
    bool ping();
    string getDeviceId();
    string getDeviceUniqueId();
    uchar getFirmwareVersion();
    LinkModeSet getLinkModes();
    void deviceReset();
    bool deviceSetLinkModes(LinkModeSet lms);
    LinkModeSet supportedLinkModes()
    {
        if (type() == BusDeviceType::DEVICE_IM871A)
        {
            return
                C1_bit |
                C2_bit |
                S1_bit |
                S1m_bit |
                T1_bit |
                T2_bit;
        }
        else
        {
            return N1a_bit |
                N1b_bit |
                N1c_bit |
                N1d_bit |
                N1e_bit |
                N1f_bit;
        }
    }
    int numConcurrentLinkModes() {
        return 2;
    }
    bool canSetLinkModes(LinkModeSet lms)
    {
        if (lms.empty()) return false;
        if (!supportedLinkModes().supports(lms)) return false;
        // Ok, the supplied link modes are compatible.
        if (type() == DEVICE_IM170A)
        {
            // Simple test.
            return 1 == countSetBits(lms.asBits());
        }
        // For im871a 14 and later firmware.
        if (getFirmwareVersion() > FIRMWARE_13_C_OR_T)
        {
            if (2 == countSetBits(lms.asBits()) &&
                lms.has(LinkMode::C1) &&
                lms.has(LinkMode::T1))
            {
                return true;
            }
        }
        // Otherwise its a single link mode.
        return 1 == countSetBits(lms.asBits());
    }
    bool sendTelegram(LinkMode lm, TelegramFormat format, vector<uchar> &content);

    void processSerialData();
    void simulate() { }

    WMBusIM871aIM170A(BusDeviceType type, string alias, shared_ptr<SerialDevice> serial, shared_ptr<SerialCommunicationManager> manager);
    ~WMBusIM871aIM170A() {
    }

    static FrameStatus checkIM871AFrame(vector<uchar> &data,
                                        size_t *frame_length, int *endpoint_out, int *msgid_out,
                                        int *payload_len_out, int *payload_offset,
                                        int *rssi_dbm);

private:

    IM871ADeviceInfo device_info_ {};
    Config     device_config_ {};

    vector<uchar> read_buffer_;
    vector<uchar> request_;
    vector<uchar> response_;

    bool getDeviceInfo();
    bool loaded_device_info_ {};

    bool getConfig();

    friend AccessCheck detectIM871AIM170A(Detected *detected, shared_ptr<SerialCommunicationManager> manager);
    void handleDevMgmt(int msgid, vector<uchar> &payload);
    void handleRadioLink(int msgid, vector<uchar> &payload, int rssi_dbm);
    void handleRadioLinkTest(int msgid, vector<uchar> &payload);
    void handleHWTest(int msgid, vector<uchar> &payload);
};


int toDBM(int rssi)
{
    // Very course approximation of this graph:
    // Figure 7-3: RSSI vs. Input Power (Silicon Labs Si1002 datasheet [3])
    // Stronger rssi:s than 0 dbm will be reported as 0 dbm.
    // rssi = >230 -> 0 dbm
    // rssi = 205 -> -20 dbm
    // rssi = 45  -> -100 dbm
#define SLOPE (80.0/(205.0-45.0))
    if (rssi >= 230) return 0;
    int dbm = -100+SLOPE*(rssi-45);
    return dbm;
}

shared_ptr<BusDevice> openIM871AIM170A(BusDeviceType type, Detected detected, shared_ptr<SerialCommunicationManager> manager, shared_ptr<SerialDevice> serial_override)
{
    string bus_alias = detected.specified_device.bus_alias;
    string device_file = detected.found_file;
    assert(device_file != "");
    if (serial_override)
    {
        WMBusIM871aIM170A *imp = new WMBusIM871aIM170A(type, bus_alias, serial_override, manager);
        imp->markAsNoLongerSerial();
        return shared_ptr<BusDevice>(imp);
    }

    auto serial = manager->createSerialDeviceTTY(device_file.c_str(), 57600, PARITY::NONE, "im871a");
    WMBusIM871aIM170A *imp = new WMBusIM871aIM170A(type, bus_alias, serial, manager);
    return shared_ptr<BusDevice>(imp);
}

shared_ptr<BusDevice> openIM871A(Detected detected, shared_ptr<SerialCommunicationManager> manager, shared_ptr<SerialDevice> serial_override)
{
    return openIM871AIM170A(BusDeviceType::DEVICE_IM871A, detected, manager, serial_override);
}

shared_ptr<BusDevice> openIM170A(Detected detected, shared_ptr<SerialCommunicationManager> manager, shared_ptr<SerialDevice> serial_override)
{
    return openIM871AIM170A(BusDeviceType::DEVICE_IM170A, detected, manager, serial_override);
}

WMBusIM871aIM170A::WMBusIM871aIM170A(BusDeviceType type, string alias, shared_ptr<SerialDevice> serial, shared_ptr<SerialCommunicationManager> manager) :
    BusDeviceCommonImplementation(alias, type, manager, serial, true)
{
    reset();
}

bool WMBusIM871aIM170A::ping()
{
    if (serial()->readonly()) return true; // Feeding from stdin or file.

    LOCK_WMBUS_EXECUTING_COMMAND(ping);

    request_.resize(4);
    request_[0] = IM871A_SERIAL_SOF;
    request_[1] = DEVMGMT_ID;
    request_[2] = DEVMGMT_MSG_PING_REQ;
    request_[3] = 0;

    verbose("(im871a) ping\n");
    bool sent = serial()->send(request_);

    if (sent) return waitForResponse(DEVMGMT_MSG_PING_RSP);

    return true;
}

string WMBusIM871aIM170A::getDeviceId()
{
    if (serial()->readonly()) return "?"; // Feeding from stdin or file.
    if (cached_device_id_ != "") return cached_device_id_;

    bool ok = getConfig();
    if (!ok) return "ERR";

    cached_device_id_ = tostrprintf("%08x", device_config_.id);

    verbose("(im871a) got device id %s\n", cached_device_id_.c_str());

    return cached_device_id_;
}

string WMBusIM871aIM170A::getDeviceUniqueId()
{
    if (serial()->readonly()) return "?"; // Feeding from stdin or file.
    if (cached_device_unique_id_ != "") return cached_device_unique_id_;

    bool ok = getDeviceInfo();
    if (!ok) return "ERR";

    cached_device_unique_id_ = tostrprintf("%08x", device_info_.uid);

    verbose("(im871a) got device unique id %s\n", cached_device_unique_id_.c_str());

    return cached_device_unique_id_;
}

uchar WMBusIM871aIM170A::getFirmwareVersion()
{
    if (serial()->readonly()) return 0x15; // Feeding from stdin or file.

    bool ok = getDeviceInfo();
    if (!ok) return 255;

    return device_info_.firmware_version;
}

LinkModeSet WMBusIM871aIM170A::getLinkModes()
{
    if (serial()->readonly()) { return Any_bit; }  // Feeding from stdin or file.

    LOCK_WMBUS_EXECUTING_COMMAND(get_link_modes);

    request_.resize(4);
    request_[0] = IM871A_SERIAL_SOF;
    request_[1] = DEVMGMT_ID;
    request_[2] = DEVMGMT_MSG_GET_CONFIG_REQ;
    request_[3] = 0;

    verbose("(im871a) get config\n");
    bool sent = serial()->send(request_);

    if (!sent)
    {
        // If we are using a serial override that will not respond,
        // then just return a value.

        // Use the remembered link modes set before.
        return protectedGetLinkModes();
    }

    bool ok = waitForResponse(DEVMGMT_MSG_GET_CONFIG_RSP);

    if (!ok)
    {
        LinkModeSet lms;
        return lms;
    }

    LinkMode lm = LinkMode::UNKNOWN;

    int iff1 = response_[0];
    bool has_device_mode = (iff1&1)==1;
    bool has_link_mode = (iff1&2)==2;
    bool has_wmbus_c_field = (iff1&4)==4;
    bool has_wmbus_man_id = (iff1&8)==8;
    bool has_wmbus_device_id = (iff1&16)==16;
    bool has_wmbus_version = (iff1&32)==32;
    bool has_wmbus_device_type = (iff1&64)==64;
    bool has_radio_channel = (iff1&128)==128;

    int offset = 1;
    if (has_device_mode)
    {
        verbose("(im871a) config: device mode %02x\n", response_[offset]);
        offset++;
    }
    if (has_link_mode)
    {
        verbose("(im871a) config: link mode %02x\n", response_[offset]);
        if (response_[offset] == (int)LinkModeIM871A::C1a) {
            lm = LinkMode::C1;
        }
        if (response_[offset] == (int)LinkModeIM871A::S1) {
            lm = LinkMode::S1;
        }
        if (response_[offset] == (int)LinkModeIM871A::S1m) {
            lm = LinkMode::S1m;
        }
        if (response_[offset] == (int)LinkModeIM871A::T1) {
            lm = LinkMode::T1;
        }
        if (response_[offset] == (int)LinkModeIM871A::CT_N1A) {
            lm = LinkMode::N1a;
        }
        if (response_[offset] == (int)LinkModeIM871A::N1B) {
            lm = LinkMode::N1b;
        }
        if (response_[offset] == (int)LinkModeIM871A::N1C) {
            lm = LinkMode::N1c;
        }
        if (response_[offset] == (int)LinkModeIM871A::N1D) {
            lm = LinkMode::N1d;
        }
        if (response_[offset] == (int)LinkModeIM871A::N1E) {
            lm = LinkMode::N1e;
        }
        if (response_[offset] == (int)LinkModeIM871A::N1F) {
            lm = LinkMode::N1f;
        }
        offset++;
    }
    if (has_wmbus_c_field) {
        verbose("(im871a) config: wmbus c-field %02x\n", response_[offset]);
        offset++;
    }
    if (has_wmbus_man_id) {
        int flagid = 256*response_[offset+1] +response_[offset+0];
        string flag = manufacturerFlag(flagid);
        verbose("(im871a) config: wmbus mfg id %02x%02x (%s)\n", response_[offset+1], response_[offset+0],
                flag.c_str());
        offset+=2;
    }
    if (has_wmbus_device_id) {
        verbose("(im871a) config: wmbus device id %02x%02x%02x%02x\n", response_[offset+3], response_[offset+2],
                response_[offset+1], response_[offset+0]);
        offset+=4;
    }
    if (has_wmbus_version) {
        verbose("(im871a) config: wmbus version %02x\n", response_[offset]);
        offset++;
    }
    if (has_wmbus_device_type) {
        verbose("(im871a) config: wmbus device type %02x\n", response_[offset]);
        offset++;
    }
    if (has_radio_channel) {
        verbose("(im871a) config: radio channel %02x\n", response_[offset]);
        offset++;
    }
    int iff2 = response_[offset];
    offset++;
    bool has_radio_power_level = (iff2&1)==1;
    bool has_radio_data_rate = (iff2&2)==2;
    bool has_radio_rx_window = (iff2&4)==4;
    bool has_auto_power_saving = (iff2&8)==8;
    bool has_auto_rssi_attachment = (iff2&16)==16;
    bool has_auto_rx_timestamp_attachment = (iff2&32)==32;
    bool has_led_control = (iff2&64)==64;
    bool has_rtc_control = (iff2&128)==128;
    if (has_radio_power_level) {
        verbose("(im871a) config: radio power level %02x\n", response_[offset]);
        offset++;
    }
    if (has_radio_data_rate) {
        verbose("(im871a) config: radio data rate %02x\n", response_[offset]);
        offset++;
    }
    if (has_radio_rx_window) {
        verbose("(im871a) config: radio rx window %02x\n", response_[offset]);
        offset++;
    }
    if (has_auto_power_saving) {
        verbose("(im871a) config: auto power saving %02x\n", response_[offset]);
        offset++;
    }
    if (has_auto_rssi_attachment) {
        verbose("(im871a) config: auto RSSI attachment %02x\n", response_[offset]);
        offset++;
    }
    if (has_auto_rx_timestamp_attachment) {
        verbose("(im871a) config: auto rx timestamp attachment %02x\n", response_[offset]);
        offset++;
    }
    if (has_led_control) {
        verbose("(im871a) config: led control %02x\n", response_[offset]);
        offset++;
    }
    if (has_rtc_control) {
        verbose("(im871a) config: rtc control %02x\n", response_[offset]);
        offset++;
    }


    LinkModeSet lms;
    lms.addLinkMode(lm);
    return lms;
}

void WMBusIM871aIM170A::deviceReset()
{
    // No device specific settings needed right now.
    // The common code in wmbus.cc reset()
    // will open the serial device and potentially
    // set the link modes properly.
}

bool WMBusIM871aIM170A::deviceSetLinkModes(LinkModeSet lms)
{
    bool rc = false;

    if (serial()->readonly()) return true; // Feeding from stdin or file.

    if (!canSetLinkModes(lms))
    {
        string modes = lms.hr();
        error("(im871a) setting link mode(s) %s is not supported for im871a\n", modes.c_str());
    }

    LOCK_WMBUS_EXECUTING_COMMAND(set_link_modes);

    request_.resize(10);
    request_[0] = IM871A_SERIAL_SOF;
    request_[1] = DEVMGMT_ID;
    request_[2] = DEVMGMT_MSG_SET_CONFIG_REQ;
    request_[3] = 6; // Len
    request_[4] = 0; // Temporary
    request_[5] = 2; // iff1 bits: Set Radio Mode
    if (lms.has(LinkMode::C1) && lms.has(LinkMode::T1)) {
        assert(getFirmwareVersion() > FIRMWARE_13_C_OR_T);
        request_[6] = (int)LinkModeIM871A::CT_N1A;
    } else  if (lms.has(LinkMode::C1)) {
        request_[6] = (int)LinkModeIM871A::C1a;
    } else  if (lms.has(LinkMode::C2)) {
        request_[6] = (int)LinkModeIM871A::C2b;
    } else if (lms.has(LinkMode::S1)) {
        request_[6] = (int)LinkModeIM871A::S1;
    } else if (lms.has(LinkMode::S1m)) {
        request_[6] = (int)LinkModeIM871A::S1m;
    } else if (lms.has(LinkMode::T1)) {
        request_[6] = (int)LinkModeIM871A::T1;
    } else if (lms.has(LinkMode::T2)) {
        request_[6] = (int)LinkModeIM871A::T2;
    } else if (lms.has(LinkMode::N1a)) {
        request_[6] = (int)LinkModeIM871A::CT_N1A;
    } else if (lms.has(LinkMode::N1b)) {
        request_[6] = (int)LinkModeIM871A::N1B;
    } else if (lms.has(LinkMode::N1c)) {
        request_[6] = (int)LinkModeIM871A::N1C;
    } else if (lms.has(LinkMode::N1d)) {
        request_[6] = (int)LinkModeIM871A::N1D;
    } else if (lms.has(LinkMode::N1e)) {
        request_[6] = (int)LinkModeIM871A::N1E;
    } else if (lms.has(LinkMode::N1f)) {
        request_[6] = (int)LinkModeIM871A::N1F;
    } else {
        request_[6] = (int)LinkModeIM871A::C1a; // Defaults to C1a
    }

    request_[7] = 0x10 | 0x20; // iff2 bits: Set rssi 0x10, timestamp 0x20
    request_[8] = 1;  // Enable rssi
    request_[9] = 0;  // Disable timestamp

    verbose("(im871a) set config to set link mode %02x\n", request_[6]);
    bool sent = serial()->send(request_);

    if (sent)
    {
        bool ok = waitForResponse(DEVMGMT_MSG_SET_CONFIG_RSP);
        if (ok)
        {
            rc = true;
        }
        else
        {
            warning("Warning! Did not get confirmation on set link mode for im871a\n");
        }
    }

    return rc;
}

FrameStatus WMBusIM871aIM170A::checkIM871AFrame(vector<uchar> &data,
                                          size_t *frame_length, int *endpoint_out, int *msgid_out,
                                          int *payload_len_out, int *payload_offset,
                                          int *rssi_dbm)
{
    if (data.size() == 0) return PartialFrame;

    debugPayload("(im871a) checkIM871AFrame", data);
    if (data[0] != 0xa5)
    {
        debugPayload("(im871a) frame does not start with a5", data);
        bool found_a5 = false;
        for (size_t i = 0; i < data.size(); ++i)
        {
            if (data[i] == 0xa5)
            {
                debug("(im871a) found a5 at pos %d\n", i);
                data.erase(data.begin(), data.begin()+i);
                found_a5 = true;;
                break;
            }
        }
        if (!found_a5)
        {
            debug("(im871a) no a5 found at all, drop frame packet.\n");
            return ErrorInFrame;
        }
    }
    if (data.size() < 4)
    {
        debug("(im871a) frame is less than 4 bytes, listen for more bytes.\n");
        return PartialFrame;
    }

    int ctrlbits = (data[1] & 0xf0) >> 4;
    if (ctrlbits & 1) {
        debug("(im871a) error in frame, bit 1 shoud not be set in data[1]\n");
        return ErrorInFrame; // Bit 1 is reserved, we do not expect it....
    }
    bool has_timestamp = ((ctrlbits&2)==2);
    bool has_rssi = ((ctrlbits&4)==4);
    bool has_crc16 = ((ctrlbits&8)==8);
    debug("(im871a) has_timestamp=%d has_rssi=%d has_crc16=%d\n", has_timestamp, has_rssi, has_crc16);

    int endpoint = data[1] & 0x0f;

    debug("(im871a) endpoint %d\n", endpoint);
    if (endpoint != DEVMGMT_ID &&  // 0x01
        endpoint != RADIOLINK_ID && // 0x02
        endpoint != RADIOLINKTEST_ID && // 0x03
        endpoint != HWTEST_ID) // 0x04
    {
        debug("(im871a) Not a valid endpoint %d\n", endpoint);
        return ErrorInFrame;
    }
    *endpoint_out = endpoint;

    int msgid = data[2];
    debug("(im871a) msgid %d\n", msgid);
    if (endpoint == DEVMGMT_ID && (msgid<1 || msgid>0x27))
    {
        debug("(im871a) DEVMGMT_ID ERROR unexpected msgid %d\n", msgid);
        return ErrorInFrame;
    }
    if (endpoint == RADIOLINK_ID && (msgid<1 || msgid>0x05))
    {
        debug("(im871a) RADIOLINK_ID_ID ERROR unexpected msgid %d\n", msgid);
        return ErrorInFrame;
    }
    if (endpoint == RADIOLINKTEST_ID && (msgid<1 || msgid>0x07))
    {
        debug("(im871a) RADIOLINKTEST_ID ERROR unexpected msgid %d\n", msgid);
        return ErrorInFrame;
    }
    if (endpoint == HWTEST_ID && (msgid<1 || msgid>0x02))
    {
        debug("(im871a) HWTEST_ID ERROR unexpected msgid %d\n", msgid);
        return ErrorInFrame;
    }

    *msgid_out = msgid;

    int payload_len = data[3];
    *payload_len_out = payload_len;
    *payload_offset = 4;

    *frame_length = *payload_offset+payload_len+(has_timestamp?4:0)+(has_rssi?1:0)+(has_crc16?2:0);
    if (data.size() < *frame_length) {
        debug("(im871a) not enough bytes yet, partial frame %d %d.\n", data.size(), *frame_length);
        return PartialFrame;
    }

    int i = *payload_offset + payload_len;
    if (has_timestamp) {
        uint32_t a = data[i];
        uint32_t b = data[i+1];
        uint32_t c = data[i+2];
        uint32_t d = data[i+3];

        uint32_t ts = a+b*256+c*256*256+d*256*256*256;
        debug("(im871a) timestamp %08x\n", ts);
        i += 4;
    }
    if (has_rssi) {
        int rssi = data[i];
        *rssi_dbm = toDBM(rssi);
        debug("(im871a) rssi %d (%d dBm)\n", rssi, *rssi_dbm);
        i++;
    }
    if (has_crc16) {
        uint32_t a = data[i];
        uint32_t b = data[i+1];
        uint32_t crc16 = a+b*256;
        i+=2;
        uint16_t gotcrc = ~crc16_CCITT(&data[1], i-1-2);
        bool crcok = crc16_CCITT_check(&data[1], i-1);
        debug("(im871a) got crc16 %04x expected %04x\n", crc16, gotcrc);
        if (!crcok) {
            warning("(im871a) warning: got wrong crc %04x expected %04x\n", gotcrc, crc16);
        }
    }

    debug("(im871a) received full frame\n");
    return FullFrame;
}

void WMBusIM871aIM170A::processSerialData()
{
    vector<uchar> data;

    // Receive and accumulated serial data until a full frame has been received.
    serial()->receive(&data);

    LOCK_WMBUS_RECEIVING_BUFFER(processSerialData);

    read_buffer_.insert(read_buffer_.end(), data.begin(), data.end());

    size_t frame_length;
    int endpoint;
    int msgid;
    int payload_len, payload_offset;
    int rssi_dbm = 0;

    for (;;)
    {
        FrameStatus status = checkIM871AFrame(read_buffer_, &frame_length, &endpoint, &msgid, &payload_len, &payload_offset, &rssi_dbm);

        if (status == PartialFrame)
        {
            if (read_buffer_.size() > 0)
            {
                debugPayload("(im871a) partial frame, expecting more.", read_buffer_);
            }
            break;
        }
        if (status == ErrorInFrame)
        {
            debugPayload("(im871a) bad frame, clearing.", read_buffer_);
            read_buffer_.clear();
            break;
        }
        if (status == FullFrame)
        {
            vector<uchar> payload;
            if (payload_len > 0)
            {
                if (endpoint == RADIOLINK_ID &&
                    msgid == RADIOLINK_MSG_WMBUSMSG_IND)
                {
                    uchar l = payload_len;
                    payload.insert(payload.begin(), &l, &l+1); // Re-insert the len byte.
                }
                // Insert the payload.
                payload.insert(payload.end(),
                               read_buffer_.begin()+payload_offset,
                               read_buffer_.begin()+payload_offset+payload_len);
            }
            read_buffer_.erase(read_buffer_.begin(), read_buffer_.begin()+frame_length);

            // We now have a proper message in payload. Let us trigger actions based on it.
            // It can be wmbus receiver-dongle messages or wmbus remote meter messages received over the radio.
            switch (endpoint) {
            case DEVMGMT_ID: handleDevMgmt(msgid, payload); break;
            case RADIOLINK_ID: handleRadioLink(msgid, payload, rssi_dbm); break;
            case RADIOLINKTEST_ID: handleRadioLinkTest(msgid, payload); break;
            case HWTEST_ID: handleHWTest(msgid, payload); break;
            }
        }
    }
}

void WMBusIM871aIM170A::handleDevMgmt(int msgid, vector<uchar> &payload)
{
    switch (msgid) {
        case DEVMGMT_MSG_PING_RSP: // 0x02
            verbose("(im871a) pong\n");
            notifyResponseIsHere(DEVMGMT_MSG_PING_RSP);
            break;
        case DEVMGMT_MSG_SET_CONFIG_RSP: // 0x04
            verbose("(im871a) set config completed\n");
            response_.clear();
            response_.insert(response_.end(), payload.begin(), payload.end());
            notifyResponseIsHere(DEVMGMT_MSG_SET_CONFIG_RSP);
            break;
        case DEVMGMT_MSG_GET_CONFIG_RSP: // 0x06
            verbose("(im871a) get config completed\n");
            response_.clear();
            response_.insert(response_.end(), payload.begin(), payload.end());
            notifyResponseIsHere(DEVMGMT_MSG_GET_CONFIG_RSP);
            break;
        case DEVMGMT_MSG_GET_DEVICEINFO_RSP: // 0x10
            verbose("(im871a) device info completed\n");
            response_.clear();
            response_.insert(response_.end(), payload.begin(), payload.end());
            notifyResponseIsHere(DEVMGMT_MSG_GET_DEVICEINFO_RSP);
            break;
    default:
        verbose("(im871a) Unhandled device management message %d\n", msgid);
    }
}

void WMBusIM871aIM170A::handleRadioLink(int msgid, vector<uchar> &frame, int rssi_dbm)
{
    switch (msgid) {
    case RADIOLINK_MSG_WMBUSMSG_IND: // 0x03
    {
        // Invoke common telegram reception code in BusDeviceCommonImplementation.
        AboutTelegram about("im871a["+cached_device_id_+"]", rssi_dbm, FrameType::WMBUS);
        handleTelegram(about, frame);
    }
    break;
    case RADIOLINK_MSG_DATA_RSP: // 0x05
        verbose("(im871a) send telegram completed\n");
        response_.clear();
        notifyResponseIsHere(RADIOLINK_MSG_DATA_RSP);
    break;
    case RADIOLINK_MSG_WMBUSMSG_RSP: // 0x02
        verbose("(im871a) send telegram completed\n");
        response_.clear();
        notifyResponseIsHere(RADIOLINK_MSG_WMBUSMSG_RSP);
    break;
    default:
        verbose("(im871a) Unhandled radio link message %d\n", msgid);
    }
}

void WMBusIM871aIM170A::handleRadioLinkTest(int msgid, vector<uchar> &payload)
{
    switch (msgid) {
    default:
        verbose("(im871a) Unhandled radio link test message %d\n", msgid);
    }
}

void WMBusIM871aIM170A::handleHWTest(int msgid, vector<uchar> &payload)
{
    switch (msgid) {
    default:
        verbose("(im871a) Unhandled hw test message %d\n", msgid);
    }
}

bool extract_response(vector<uchar> &data, vector<uchar> &response, int expected_endpoint, int expected_msgid)
{
    size_t frame_length;
    int endpoint, msgid, payload_len, payload_offset, rssi_dbm;
    FrameStatus status = WMBusIM871aIM170A::checkIM871AFrame(data,
                                                       &frame_length, &endpoint, &msgid,
                                                       &payload_len, &payload_offset, &rssi_dbm);
    if (status != FullFrame ||
        endpoint != expected_endpoint ||
        msgid != expected_msgid)
    {
        return false;
    }

    response.clear();
    response.insert(data.end(), data.begin()+payload_offset, data.begin()+payload_offset+payload_len);
    return true;
}

bool WMBusIM871aIM170A::getDeviceInfo()
{
    if (loaded_device_info_) return true;

    LOCK_WMBUS_EXECUTING_COMMAND(get_device_info);

    request_.resize(4);
    request_[0] = IM871A_SERIAL_SOF;
    request_[1] = DEVMGMT_ID;
    request_[2] = DEVMGMT_MSG_GET_DEVICEINFO_REQ;
    request_[3] = 0;

    verbose("(im871a) get device info\n");

    bool sent = serial()->send(request_);
    if (!sent) return false; // tty overridden with stdin/file

    bool ok = waitForResponse(DEVMGMT_MSG_GET_DEVICEINFO_RSP);
    if (!ok) return false; // timeout

    // Now device info response is in response_ vector.
    device_info_.decode(response_);

    loaded_device_info_ = true;
    verbose("(im871a) device info: %s\n", device_info_.str().c_str());

    return true;
}

bool WMBusIM871aIM170A::getConfig()
{
    if (serial()->readonly()) return true;

    LOCK_WMBUS_EXECUTING_COMMAND(getConfig);

    request_.resize(4);
    request_[0] = IM871A_SERIAL_SOF;
    request_[1] = DEVMGMT_ID;
    request_[2] = DEVMGMT_MSG_GET_CONFIG_REQ;
    request_[3] = 0;

    verbose("(im871a) get config\n");

    bool sent = serial()->send(request_);
    if (!sent) return false;

    bool ok = waitForResponse(DEVMGMT_MSG_GET_CONFIG_RSP);
    if (!ok) return false;

    return device_config_.decode(response_);
}

bool WMBusIM871aIM170A::sendTelegram(LinkMode lm, TelegramFormat format, vector<uchar> &content)
{
    if (serial()->readonly()) return true;
    if (content.size() > 250) return false;

    LOCK_WMBUS_EXECUTING_COMMAND(sendTelegram);

    request_.resize(4);
    request_[0] = IM871A_SERIAL_SOF;
    request_[1] = RADIOLINK_ID;
    int resp = 0;
    if (format == TelegramFormat::WMBUS_C_FIELD)
    {
        request_[2] = RADIOLINK_MSG_WMBUSMSG_REQ;
        resp = RADIOLINK_MSG_WMBUSMSG_RSP;
    }
    else if (format == TelegramFormat::WMBUS_CI_FIELD)
    {
        request_[2] = RADIOLINK_MSG_DATA_REQ;
        resp = RADIOLINK_MSG_DATA_RSP;
    }
    else
    {
        warning("(im871a) cannot use telegram format %s for sending\n", toString(format));
        return false;
    }

    request_[3] = content.size();

    for (size_t i=0; i<content.size(); ++i)
    {
        request_.push_back(content[i]);
    }

    verbose("(im871a) send telegram waiting for %d\n", resp);

    bool sent = serial()->send(request_);
    if (!sent) return false;

    bool ok = waitForResponse(resp);
    if (!ok) return false; // timeout

    return true;
}

AccessCheck detectIM871AIM170A(Detected *detected, shared_ptr<SerialCommunicationManager> manager)
{
    assert(detected->found_file != "");

    // Talk to the device and expect a very specific answer.
    auto serial = manager->createSerialDeviceTTY(detected->found_file.c_str(), 57600, PARITY::NONE, "detect im871a");
    serial->disableCallbacks();
    bool ok = serial->open(false);
    if (!ok)
    {
        verbose("(im871a) could not open tty %s for detection\n", detected->found_file.c_str());
        return AccessCheck::NoSuchDevice;
    }

    vector<uchar> response;
    // First clear out any data in the queue.
    serial->receive(&response);
    response.clear();

    vector<uchar> request;
    request.resize(4);
    request[0] = IM871A_SERIAL_SOF;
    request[1] = DEVMGMT_ID;
    request[2] = DEVMGMT_MSG_GET_DEVICEINFO_REQ;
    request[3] = 0;

    serial->send(request);
    // Wait for 100ms so that the USB stick have time to prepare a response.
    usleep(1000*100);
    serial->receive(&response);

    size_t frame_length;
    int endpoint, msgid, payload_len, payload_offset, rssi_dbm;
    FrameStatus status = WMBusIM871aIM170A::checkIM871AFrame(response,
                                                       &frame_length, &endpoint, &msgid,
                                                       &payload_len, &payload_offset, &rssi_dbm);
    if (status != FullFrame ||
        endpoint != 1 ||
        msgid != DEVMGMT_MSG_GET_DEVICEINFO_RSP)
    {
        verbose("(im871a/im170a) are you there? no.\n");
        serial->close();
        return AccessCheck::NoProperResponse;
    }

    vector<uchar> payload;
    payload.insert(payload.end(), response.begin()+payload_offset, response.begin()+payload_offset+payload_len);

    debugPayload("(device info bytes)", payload);

    IM871ADeviceInfo di;
    di.decode(payload);

    debug("(im871a/im170a) info: %s\n", di.str().c_str());

    BusDeviceType type;

    string types;
    if (di.module_type == 0x33)
    {
        type = BusDeviceType::DEVICE_IM871A;
        types = "im871a";
    }
    else
    {
        type = BusDeviceType::DEVICE_IM170A;
        types = "im170a";
    }

    request.resize(4);
    request[0] = IM871A_SERIAL_SOF;
    request[1] = DEVMGMT_ID;
    request[2] = DEVMGMT_MSG_GET_CONFIG_REQ;
    request[3] = 0;

    serial->send(request);
    // Wait for 100ms so that the USB stick have time to prepare a response.
    usleep(1000*100);
    serial->receive(&response);

    status = WMBusIM871aIM170A::checkIM871AFrame(response,
                                                 &frame_length, &endpoint, &msgid,
                                                 &payload_len, &payload_offset, &rssi_dbm);
    if (status != FullFrame ||
        endpoint != 1 ||
        msgid != DEVMGMT_MSG_GET_CONFIG_RSP)
    {
        verbose("(im871a/im170a) are you there? no.\n");
        serial->close();
        return AccessCheck::NoProperResponse;
    }

    serial->close();

    payload.clear();
    payload.insert(payload.end(), response.begin()+payload_offset, response.begin()+payload_offset+payload_len);

    debugPayload("(device config bytes)", payload);

    Config co;
    co.decode(payload);

    debug("(im871a/im170a) config: %s\n", co.str().c_str());

    detected->setAsFound(co.dongleId(), type, 57600, false, detected->specified_device.linkmodes);

    verbose("(im871a/im170a) are you there? yes %s %s firmware: %02x\n", co.dongleId().c_str(), types.c_str(), di.firmware_version);

    return AccessCheck::AccessOK;
}
