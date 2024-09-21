/*
 Copyright (C) 2017-2024 Fredrik Öhrström (gpl-3.0-or-later)

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
#include"wmbus_iu891a.h"
#include"serial.h"
#include"threads.h"

#include<assert.h>
#include<pthread.h>
#include<errno.h>
#include<memory.h>
#include<semaphore.h>
#include<unistd.h>

using namespace std;

struct DeviceInfo_IU891A
{
    uchar module_type; // 109=0x6d=iM891A-XL 110=0x6e=iU891A-XL 163=0xa3=iM881A-XL
    uint32_t uid;
    string uids;
    string product_type;
    string product_id;

    string str()
    {
        string s;
/*        s += "status=";
          string st = tostrprintf( "%02x", status);*/
        //s += st;
        s += " type=";
        string mt = tostrprintf("%02x", module_type);
        if (module_type == 0x6d) s+="im891a ";
        else if (module_type == 0x6e) s+="iu891a ";
        else if (module_type == 0xa3) s+="im881 ";
        else s+="unknown_type("+mt+") ";

        string ss;
        strprintf(&ss, "uid=%08x", uid);
        return s+ss;
    }

    bool decode(vector<uchar> &bytes)
    {
        if (bytes.size() < 8) return false;
        int i = 0;
        module_type = bytes[i++];
        uid = bytes[i+3]<<24|bytes[i+2]<<16|bytes[i+1]<<8|bytes[i]; i+=4;
        strprintf(&uids, "%08x", uid);
        return true;
    }
};

struct WMBusAddressInfo_IU891A
{
    uint16_t mfct {};
    uint32_t id {};
    uint8_t version {};
    uint8_t type {};

    string dongleId()
    {
        string s;
        strprintf(&s, "%08x", id);
        return s;
    }

    bool decode(vector<uchar> &bytes)
    {
        if (bytes.size() < 8) return false;
        int i = 0;
        mfct = bytes[i] << 8 | bytes[i+1];
        i += 2;
        id = bytes[i]<<24|bytes[i+1]<<16|bytes[i+1]<<8|bytes[i+3];
        i+=4;
        version = bytes[i++];
        type = bytes[i++];

        return true;
    }

};

struct Config_IU891A
{
    LinkModeSet link_modes;
    uint16_t option_bits {};
    uint16_t ui_option_bits {};
    uint16_t led_flash_timing {};
    uint32_t recalibrate_in_ms {};

    string str()
    {
        string s;

        if (option_bits & 0x01) s += "RCV_FILTER ";
        else s += "RCV_ALL ";

        if (option_bits & 0x02) s += "RCV_IND ";
        if (option_bits & 0x04) s += "SND_IND ";
        if (option_bits & 0x08) s += "RECALIB ";

        if (ui_option_bits & 0x01) s += "ASSERT_PIN24_ON_TELEGRAM_ARRIVAL ";
        if (ui_option_bits & 0x02) s += "PIN24_POLARITY_REVERSED ";
        if (ui_option_bits & 0x04) s += "ASSERT_PIN25_ON_TELEGRAM_SENT ";
        if (ui_option_bits & 0x08) s += "PIN25_POLARITY_REVERSED ";

        s += tostrprintf("led flash: %d ms ", led_flash_timing);
        s += tostrprintf("recalibrate: %d ms ", recalibrate_in_ms);

        s.pop_back();
        return s;
    }

    void encode(vector<uchar> &bytes, uchar lm)
    {
        bytes.clear();
        bytes.resize(11);
        size_t i = 0;
        bytes[i++] = lm;
        bytes[i++] = option_bits & 0xff;
        bytes[i++] = (option_bits >> 8) & 0xff;
        bytes[i++] = ui_option_bits & 0xff;
        bytes[i++] = (ui_option_bits >> 8) & 0xff;
        bytes[i++] = led_flash_timing & 0xff;
        bytes[i++] = (led_flash_timing >> 8) & 0xff;
        bytes[i++] = recalibrate_in_ms & 0xff;
        bytes[i++] = (recalibrate_in_ms >> 8) & 0xff;
        bytes[i++] = (recalibrate_in_ms >> 16) & 0xff;
        bytes[i++] = (recalibrate_in_ms >> 24) & 0xff;
    }

    bool decode(vector<uchar> &bytes)
    {
        if (bytes.size() < 11) return false;
        size_t i = 0;
        uchar c = bytes[i++];
        link_modes.clear();
        if (c == LINK_MODE_OFF)
        {
        }
        else if (c == LINK_MODE_S)
        {
            link_modes.addLinkMode(LinkMode::S1);
        }
        else if (c == LINK_MODE_T)
        {
            link_modes.addLinkMode(LinkMode::T1);
        }
        else if (c == LINK_MODE_CT)
        {
            link_modes.addLinkMode(LinkMode::C1).addLinkMode(LinkMode::T1);
        }
        else if (c == LINK_MODE_C)
        {
            link_modes.addLinkMode(LinkMode::C1);
        }
        option_bits = bytes[i+1]<<8 | bytes[i+0];
        i += 2;
        ui_option_bits = bytes[i+1]<<8 | bytes[i+0];
        i += 2;
        led_flash_timing = bytes[i+1]<<8 | bytes[i+0];
        i += 2;
        recalibrate_in_ms =
            bytes[i+3]<<24 | bytes[i+2] <<16 |
            bytes[i+1]<<8 | bytes[i+0];

        return true;
    }
};

const char *toString(ErrorCodeIU891ADevMgmt ec)
{
    switch (ec)
    {
#define X(num,text,info) case ErrorCodeIU891ADevMgmt::text: return #info;
LIST_OF_IU891A_DEVMGMT_ERROR_CODES
#undef X
    default: return "Unknown";
    }
}

ErrorCodeIU891ADevMgmt toErrorCodeIU891ADevMgmt(uchar c)
{
    switch (c)
    {
#define X(num,text,info) case num: return ErrorCodeIU891ADevMgmt::text;
LIST_OF_IU891A_DEVMGMT_ERROR_CODES
#undef X
    default: return ErrorCodeIU891ADevMgmt::UNKNOWN;
    }
}

const char *toString(ErrorCodeIU891AWMBUSGW ec)
{
    switch (ec)
    {
#define X(num,text,info) case ErrorCodeIU891AWMBUSGW::text: return #info;
LIST_OF_IU891A_WMBUSGW_ERROR_CODES
#undef X
    default: return "Unknown";
    }
}

ErrorCodeIU891AWMBUSGW toErrorCodeIU891AWMBUSGW(uchar c)
{
    switch (c)
    {
#define X(num,text,info) case num: return ErrorCodeIU891AWMBUSGW::text;
LIST_OF_IU891A_WMBUSGW_ERROR_CODES
#undef X
    default: return ErrorCodeIU891AWMBUSGW::UNKNOWN;
    }
}

struct WMBusIU891A : public virtual BusDeviceCommonImplementation
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
        return
            C1_bit |
            C2_bit |
            S1_bit |
            S1m_bit |
            T1_bit |
            T2_bit;
    }

    int numConcurrentLinkModes()
    {
        return 2;
    }

    bool canSetLinkModes(LinkModeSet lms)
    {
        if (lms.empty()) return false;
        if (!supportedLinkModes().supports(lms)) return false;
        // Ok, the supplied link modes are compatible.
        // For iu891a
        if (2 == countSetBits(lms.asBits()) &&
            lms.has(LinkMode::C1) &&
            lms.has(LinkMode::T1))
        {
            return true;
        }
        // Otherwise its a single link mode.
        return 1 == countSetBits(lms.asBits());
    }
    bool sendTelegram(LinkMode lm, TelegramFormat format, vector<uchar> &content);

    void processSerialData();
    void simulate() { }

    WMBusIU891A(BusDeviceType type, string alias, shared_ptr<SerialDevice> serial, shared_ptr<SerialCommunicationManager> manager);
    ~WMBusIU891A() {
    }

    static FrameStatus checkIU891AFrame(vector<uchar> &data,
                                        vector<uchar> &out,
                                        size_t *frame_length,
                                        int *endpoint_id_out,
                                        int *msg_id_out,
                                        int *status_out,
                                        int *rssi_dbm);

    static void extractFrame(vector<uchar> &payload, int *rssi_dbm, vector<uchar> *frame);

private:

    DeviceInfo_IU891A device_info_ {};
    WMBusAddressInfo_IU891A device_wmbus_address_ {};
    Config_IU891A device_config_ {};

    vector<uchar> read_buffer_;
    vector<uchar> request_;
    vector<uchar> response_;

    bool getDeviceInfo();
    bool loaded_device_info_ {};

    bool getConfig();

    friend AccessCheck detectIU891A(Detected *detected, shared_ptr<SerialCommunicationManager> manager);
    void handleDevMgmt(int msgid, vector<uchar> &payload);
    void handleWMbusGateway(int msgid, vector<uchar> &payload);

};

shared_ptr<BusDevice> openIU891A(BusDeviceType type, Detected detected, shared_ptr<SerialCommunicationManager> manager, shared_ptr<SerialDevice> serial_override)
{
    string bus_alias = detected.specified_device.bus_alias;
    string device_file = detected.found_file;
    assert(device_file != "");
    if (serial_override)
    {
        WMBusIU891A *imp = new WMBusIU891A(type, bus_alias, serial_override, manager);
        imp->markAsNoLongerSerial();
        return shared_ptr<BusDevice>(imp);
    }

    auto serial = manager->createSerialDeviceTTY(device_file.c_str(), 115200, PARITY::NONE, "iu891a");
    WMBusIU891A *imp = new WMBusIU891A(type, bus_alias, serial, manager);
    return shared_ptr<BusDevice>(imp);
}

shared_ptr<BusDevice> openIU891A(Detected detected, shared_ptr<SerialCommunicationManager> manager, shared_ptr<SerialDevice> serial_override)
{
    return openIU891A(BusDeviceType::DEVICE_IU891A, detected, manager, serial_override);
}

shared_ptr<BusDevice> open(Detected detected, shared_ptr<SerialCommunicationManager> manager, shared_ptr<SerialDevice> serial_override)
{
    return openIU891A(BusDeviceType::DEVICE_IU891A, detected, manager, serial_override);
}

WMBusIU891A::WMBusIU891A(BusDeviceType type, string alias, shared_ptr<SerialDevice> serial, shared_ptr<SerialCommunicationManager> manager) :
    BusDeviceCommonImplementation(alias, type, manager, serial, true)
{
    reset();
}

static void buildRequest(int endpoint_id, int msg_id, vector<uchar>& body, vector<uchar>& out)
{
    vector<uchar> request;
    request.push_back(endpoint_id);
    request.push_back(msg_id);
    request.insert(request.end(), body.begin(), body.end());

    uint16_t crc = ~crc16_CCITT(&request[0], request.size()); // Safe to use &[0] since request size > 0
    request.push_back(crc & 0xff);
    request.push_back(crc >> 8);

    addSlipFraming(request, out);
}

bool WMBusIU891A::ping()
{
    if (serial()->readonly()) return true; // Feeding from stdin or file.
    return true;
}

string WMBusIU891A::getDeviceId()
{
    if (serial()->readonly()) return "?"; // Feeding from stdin or file.
    if (cached_device_id_ != "") return cached_device_id_;

    bool ok = getDeviceInfo();
    if (!ok) return "ERR";

    ok = getConfig();
    if (!ok) return "ERR";

    cached_device_id_ = device_wmbus_address_.dongleId();

    verbose("(iu891a) got device id %s\n", cached_device_id_.c_str());

    return cached_device_id_;
}

string WMBusIU891A::getDeviceUniqueId()
{
    if (serial()->readonly()) return "?"; // Feeding from stdin or file.
    if (cached_device_unique_id_ != "") return cached_device_unique_id_;

    bool ok = getDeviceInfo();
    if (!ok) return "ERR";

    cached_device_unique_id_ = tostrprintf("%08x", device_info_.uid);

    verbose("(im871a) got device unique id %s\n", cached_device_unique_id_.c_str());

    return cached_device_unique_id_;
}

uchar WMBusIU891A::getFirmwareVersion()
{
    if (serial()->readonly()) return 0x15; // Feeding from stdin or file.

//    bool ok = getDeviceInfo();
    //   if (!ok) return 255;

    return 0; // device_info_.firmware_version;
}

LinkModeSet WMBusIU891A::getLinkModes()
{
    if (serial()->readonly()) { return Any_bit; }  // Feeding from stdin or file.

    bool ok = getConfig();
    if (!ok) return LinkModeSet();

    return device_config_.link_modes;
}

void WMBusIU891A::deviceReset()
{
    // No device specific settings needed right now.
    // The common code in wmbus.cc reset()
    // will open the serial device and potentially
    // set the link modes properly.
}

uchar setupIMSTBusDeviceToReceiveTelegrams(LinkModeSet lms)
{
    if (lms.has(LinkMode::C1) && lms.has(LinkMode::T1))
    {
        return LINK_MODE_CT;
    }
    else if (lms.has(LinkMode::C1))
    {
        return LINK_MODE_C;
    }
    else  if (lms.has(LinkMode::C2))
    {
        return LINK_MODE_C;
    }
    else if (lms.has(LinkMode::S1))
    {
        return LINK_MODE_S;
    }
    else if (lms.has(LinkMode::S1m))
    {
        return LINK_MODE_S;
    }
    else if (lms.has(LinkMode::T1))
    {
        return LINK_MODE_T;
    }
    else if (lms.has(LinkMode::T2))
    {
        return LINK_MODE_T;
    }
    else
    {
        return LINK_MODE_C; // Defaults to C
    }

    assert(false);

    // Error
    return 0xff;
}


bool WMBusIU891A::deviceSetLinkModes(LinkModeSet lms)
{
    if (serial()->readonly()) return true; // Feeding from stdin or file.

    if (!canSetLinkModes(lms))
    {
        string modes = lms.hr();
        error("(iu871a) setting link mode(s) %s is not supported for iu891a\n", modes.c_str());
    }

    LOCK_WMBUS_EXECUTING_COMMAND(set_link_modes);

    vector<uchar> body;
    vector<uchar> request;

    device_config_.option_bits &= 0xfffe; // forward all received telegrams to wmbusmeters.
    device_config_.option_bits |= 0x0006; // get notified when received and sent.
    device_config_.encode(body, setupIMSTBusDeviceToReceiveTelegrams(lms));

    buildRequest(SAP_WMBUSGW_ID, WMBUSGW_SET_ACTIVE_CONFIGURATION_REQ, body, request);

    verbose("(iu891a) set config\n");
    bool sent = serial()->send(request);
    if (!sent) return false;

    bool ok = waitForResponse(WMBUSGW_SET_ACTIVE_CONFIGURATION_RSP);
    if (!ok) return false; // timeout

    verbose("(iu871a) set config to set link mode %02x\n", body[0]);

    return true;
}

void WMBusIU891A::extractFrame(vector<uchar> &payload, int *rssi_dbm, vector<uchar> *frame)
{
    if (payload.size() < 10) return;
    *rssi_dbm = (int8_t)payload[7];
    frame->clear();
    frame->insert(frame->begin(), payload.begin()+8, payload.end());
}

FrameStatus WMBusIU891A::checkIU891AFrame(vector<uchar> &data,
                                          vector<uchar> &out,
                                          size_t *frame_length_out,
                                          int *endpoint_id_out,
                                          int *msg_id_out,
                                          int *status_byte_out,
                                          int *rssi_dbm)
{
    vector<uchar> msg;

    removeSlipFraming(data, frame_length_out, msg);

    if (msg.size() < 5) return PartialFrame;

    *endpoint_id_out = msg[0];
    *msg_id_out = msg[1];
    *status_byte_out = msg[2];

    uint16_t crc = ~crc16_CCITT(&msg[0], msg.size()-2);
    uchar crc_lo = crc & 0xff;
    uchar crc_hi = crc >> 8;

    if (msg[msg.size()-2] != crc_lo || msg[msg.size()-1] != crc_hi)
    {
        debug("(iu880b) bad crc got %02x%02x expected %02x%02x\n",
            msg[msg.size()-1], msg[msg.size()-2], crc_hi, crc_lo);

        return ErrorInFrame;
    }

    int payload_offset = 3;
    int payload_len = msg.size()-2;
    out.clear();
    out.insert(out.end(), msg.begin()+payload_offset, msg.begin()+payload_len);

    return FullFrame;
}

void WMBusIU891A::processSerialData()
{
    vector<uchar> data;

    // Receive and accumulated serial data until a full frame has been received.
    serial()->receive(&data);

    read_buffer_.insert(read_buffer_.end(), data.begin(), data.end());

    size_t frame_length;
    int endpoint_id;
    int msg_id;
    int status_byte;
    int rssi_dbm = 0;

    vector<uchar> payload;

    for (;;)
    {
        FrameStatus status = checkIU891AFrame(read_buffer_,
                                              payload,
                                              &frame_length,
                                              &endpoint_id,
                                              &msg_id,
                                              &status_byte,
                                              &rssi_dbm);

        if (status == PartialFrame)
        {
            if (read_buffer_.size() > 0)
            {
                debugPayload("(iu891a) partial frame, expecting more.", read_buffer_);
            }
            break;
        }
        if (status == ErrorInFrame)
        {
            debugPayload("(iu891a) bad frame, clearing.", read_buffer_);
            read_buffer_.clear();
            break;
        }
        if (status == FullFrame)
        {
            read_buffer_.erase(read_buffer_.begin(), read_buffer_.begin()+frame_length);

            // We now have a proper message in payload. Let us trigger actions based on it.
            // It can be wmbus receiver-dongle messages or wmbus remote meter messages received over the radio.
            switch (endpoint_id) {
            case SAP_DEVMGMT_ID: handleDevMgmt(msg_id, payload); break;
            case SAP_WMBUSGW_ID: handleWMbusGateway(msg_id, payload); break;
            }
        }
    }
}

bool WMBusIU891A::getDeviceInfo()
{
    if (loaded_device_info_) return true;

    LOCK_WMBUS_EXECUTING_COMMAND(get_device_info);

    vector<uchar> body;
    vector<uchar> request;

    buildRequest(SAP_DEVMGMT_ID, DEVMGMT_MSG_GET_DEVICE_INFO_REQ, body, request);

    verbose("(iu891a) get device info\n");
    bool sent = serial()->send(request);
    if (!sent) return false; // tty overridden with stdin/file

    bool ok = waitForResponse(DEVMGMT_MSG_GET_DEVICE_INFO_RSP);
    if (!ok) return false; // timeout

    // Now device info response is in response_ vector.
    device_info_.decode(response_);

    verbose("(iu891a) device info: %s\n", device_info_.str().c_str());

    body.clear();
    request.clear();
    buildRequest(SAP_WMBUSGW_ID, WMBUSGW_GET_WMBUS_ADDRESS_REQ, body, request);

    verbose("(iu891a) get wmbus address\n");
    sent = serial()->send(request);
    if (!sent) return false; // tty overridden with stdin/file

    ok = waitForResponse(WMBUSGW_GET_WMBUS_ADDRESS_RSP);
    if (!ok) return false; // timeout

    // Now device info response is in response_ vector.
    device_wmbus_address_.decode(response_);

    loaded_device_info_ = true;
    verbose("(iu891a) device info: %s %s\n", device_wmbus_address_.dongleId().c_str(), device_info_.str().c_str());

    return true;
}

bool WMBusIU891A::getConfig()
{
    if (serial()->readonly()) return true;

    LOCK_WMBUS_EXECUTING_COMMAND(get_config);

    vector<uchar> body;
    vector<uchar> request;

    buildRequest(SAP_WMBUSGW_ID, WMBUSGW_GET_ACTIVE_CONFIGURATION_REQ, body, request);

    verbose("(iu891a) get config\n");
    bool sent = serial()->send(request);
    if (!sent) return false; // tty overridden with stdin/file

    bool ok = waitForResponse(WMBUSGW_GET_ACTIVE_CONFIGURATION_RSP);
    if (!ok) return false; // timeout

    // Now device info response is in response_ vector.
    device_config_.decode(response_);

    verbose("(iu891a) config: %s link modes: %s\n",
            device_config_.str().c_str(),
            device_config_.link_modes.hr().c_str());

    return true;
}

bool WMBusIU891A::sendTelegram(LinkMode lm, TelegramFormat format, vector<uchar> &content)
{
    if (serial()->readonly()) return true;
    if (content.size() > 250) return false;

    return false;
}

AccessCheck detectIU891A(Detected *detected, shared_ptr<SerialCommunicationManager> manager)
{
    assert(detected->found_file != "");

    // Talk to the device and expect a very specific answer.
    auto serial = manager->createSerialDeviceTTY(detected->found_file.c_str(), 115200, PARITY::NONE, "detect iu891a");
    serial->disableCallbacks();
    bool ok = serial->open(false);
    if (!ok)
    {
        verbose("(iu891a) could not open tty %s for detection\n", detected->found_file.c_str());
        return AccessCheck::NoSuchDevice;
    }

    vector<uchar> response;
    // First clear out any data in the queue.
    serial->receive(&response);
    response.clear();

    vector<uchar> init;
    init.resize(30);
    for (int i=0; i<30; ++i)
    {
        init[i] = 0xc0;
    }
    // Wake-up the dongle.
    serial->send(init);

    vector<uchar> body;
    vector<uchar> request;
    buildRequest(SAP_DEVMGMT_ID, DEVMGMT_MSG_GET_DEVICE_INFO_REQ, body, request);
    serial->send(request);

    // Wait for 100ms so that the USB stick have time to prepare a response.
    usleep(100*1000);
    serial->receive(&response);

    int endpoint_id = 0;
    int msg_id = 0;
    int status_byte = 0;
    size_t frame_length = 0;
    int rssi_dbm = 0;
    vector<uchar> payload;

    FrameStatus status = WMBusIU891A::checkIU891AFrame(response,
                                                       payload,
                                                       &frame_length,
                                                       &endpoint_id,
                                                       &msg_id,
                                                       &status_byte,
                                                       &rssi_dbm);

    if (status != FullFrame ||
        endpoint_id != SAP_DEVMGMT_ID ||
        msg_id != DEVMGMT_MSG_GET_DEVICE_INFO_RSP)
    {
        verbose("(iu891a) are you there? no.\n");
        serial->close();
        return AccessCheck::NoProperResponse;
    }

    debugPayload("(iu891a) device info response", payload);

    debug("(iu891a) endpoint %02x msg %02x status %02x\n", endpoint_id, msg_id, status_byte);

    DeviceInfo_IU891A di;
    di.decode(payload);

    debug("(iu891a) info: %s\n", di.str().c_str());

    body.clear();
    request.clear();
    buildRequest(SAP_WMBUSGW_ID, WMBUSGW_GET_WMBUS_ADDRESS_REQ, body, request);
    serial->send(request);

    // Wait for 100ms so that the USB stick have time to prepare a response.
    usleep(100*1000);
    serial->receive(&response);

    status = WMBusIU891A::checkIU891AFrame(response,
                                           payload,
                                           &frame_length,
                                           &endpoint_id,
                                           &msg_id,
                                           &status_byte,
                                           &rssi_dbm);

    if (status != FullFrame ||
        endpoint_id != SAP_WMBUSGW_ID ||
        msg_id != WMBUSGW_GET_WMBUS_ADDRESS_RSP)
    {
        verbose("(iu891a) are you there? I thought so, but no.\n");
        serial->close();
        return AccessCheck::NoProperResponse;
    }

    debugPayload("(iu891a) wmbus address response", payload);

    WMBusAddressInfo_IU891A wa;
    wa.decode(payload);

    serial->close();

    detected->setAsFound(wa.dongleId(), BusDeviceType::DEVICE_IU891A, 115200, false, detected->specified_device.linkmodes);

    verbose("(iu891a) are you there? yes %s\n", wa.dongleId().c_str());

    return AccessCheck::AccessOK;
}

void WMBusIU891A::handleDevMgmt(int msgid, vector<uchar> &payload)
{
    switch (msgid) {
        case DEVMGMT_MSG_PING_RSP:
            debug("(iu891a) rsp pong\n");
            break;
        case DEVMGMT_MSG_GET_DEVICE_INFO_RSP:
            debug("(iu891a) rsp got device info\n");
            break;
        case DEVMGMT_MSG_GET_FW_INFO_RSP:
            debug("(iu891a) rsp got firmware\n");
            break;
        default:
            warning("(iu891a) Unhandled device management message %d\n", msgid);
            return;
    }
    response_.clear();
    response_.insert(response_.end(), payload.begin(), payload.end());
    notifyResponseIsHere(msgid);
}


void WMBusIU891A::handleWMbusGateway(int msgid, vector<uchar> &payload)
{
    switch (msgid) {
        case WMBUSGW_GET_WMBUS_ADDRESS_RSP:
            debug("(iu891a) rsp got wmbus address\n");
            break;
        case WMBUSGW_GET_ACTIVE_CONFIGURATION_RSP:
            debug("(iu891a) rsp got active config\n");
            break;
        case WMBUSGW_SET_ACTIVE_CONFIGURATION_RSP:
            debug("(iu891a) rsp set active config\n");
            break;
        case WMBUSGW_RX_MESSAGE_IND:
        {
            // Invoke common telegram reception code in BusDeviceCommonImplementation.
            vector<uchar> frame;
            int rssi_dbm;

            extractFrame(payload, &rssi_dbm, &frame);
            AboutTelegram about("iu891a["+cached_device_id_+"]", rssi_dbm, FrameType::WMBUS);
            handleTelegram(about, frame);
            return;
        }
            break;
        default:
            warning("(iu891a) Unhandled wmbus gateway message %d\n", msgid);
            return;
    }
    response_.clear();
    response_.insert(response_.end(), payload.begin(), payload.end());
    notifyResponseIsHere(msgid);
}
