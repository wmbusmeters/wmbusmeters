/*
 Copyright (C) 2022 Fredrik Öhrström (gpl-3.0-or-later)

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
#include"lora_iu880b.h"
#include"serial.h"
#include"threads.h"

#include<assert.h>
#include<pthread.h>
#include<errno.h>
#include<memory.h>
#include<semaphore.h>
#include<unistd.h>

using namespace std;

static void buildRequest(int endpoint_id, int msg_id, vector<uchar>& body, vector<uchar>& out);

struct IU880BDeviceInfo
{
    // 0x90 = iM880A (obsolete) 0x92 = iM880A-L (128k) 0x93 = iU880A (128k) 0x98 = iM880B 0x99 = iU880B 0xA0 = iM881A 0xA1 = iU881A
    uchar module_type {};
    uint16_t device_address {};
    uchar group_address {};
    string uid;

    string str()
    {
        string s;
        if (module_type == 0x90) s += "im880a ";
        else if (module_type == 0x92) s += "im880al ";
        else if (module_type == 0x93) s += "iu880a ";
        else if (module_type == 0x98) s += "im880b ";
        else if (module_type == 0x99) s += "iu880b ";
        else if (module_type == 0xa0) s += "im881a ";
        else if (module_type == 0xa1) s += "iu881a ";
        else s += "unknown_type("+to_string(module_type)+") ";

        s += tostrprintf("address %04x/%02x uid %s", device_address, group_address, uid.c_str());

        return s;
    }

    bool decode(vector<uchar> &bytes)
    {
        if (bytes.size() < 9) return false;
        int i = 0;
        module_type = bytes[i++];
        device_address = bytes[i] | bytes[i+1];
        i+=2;
        group_address = bytes[i++];
        i++; // skip reserved
        uid = tostrprintf("%02x%02x%02x%02x", bytes[i+3], bytes[i+2], bytes[i+1], bytes[i]);
        i+=4;
        return true;
    }
};

struct IU880BConfig
{
    // 0x90 = iM880A (obsolete) 0x92 = iM880A-L (128k) 0x93 = iU880A (128k) 0x98 = iM880B 0x99 = iU880B 0xA0 = iM881A 0xA1 = iU881A
    uchar module_type {};
    uint16_t device_address {};
    uchar group_address {};
    string uid;

    string str()
    {
        string s;
        if (module_type == 0x90) s += "im880a ";
        else if (module_type == 0x92) s += "im880al ";
        else if (module_type == 0x93) s += "iu880a ";
        else if (module_type == 0x98) s += "im880b ";
        else if (module_type == 0x99) s += "iu880b ";
        else if (module_type == 0xa0) s += "im881a ";
        else if (module_type == 0xa1) s += "iu881a ";
        else s += "unknown_type("+to_string(module_type)+") ";

        s += tostrprintf("address %04x/%02x uid %s", device_address, group_address, uid.c_str());

        return s;
    }

    bool decode(vector<uchar> &bytes)
    {
        if (bytes.size() < 9) return false;
        int i = 0;
        module_type = bytes[i++];
        device_address = bytes[i] | bytes[i+1];
        i+=2;
        group_address = bytes[i++];
        i++; // skip reserved
        uid = tostrprintf("%02x%02x%02x%02x", bytes[i+3], bytes[i+2], bytes[i+1], bytes[i]);
        i+=4;
        return true;
    }
};

struct LoRaIU880B : public virtual WMBusCommonImplementation
{
    bool ping();
    string getDeviceId();
    string getDeviceUniqueId();
    uchar getFirmwareVersion();
    LinkModeSet getLinkModes();
    void deviceReset();
    void deviceSetLinkModes(LinkModeSet lms);
    LinkModeSet supportedLinkModes()
    {
        return LORA_bit;
    }
    int numConcurrentLinkModes() {
        return 1;
    }
    bool canSetLinkModes(LinkModeSet lms)
    {
        if (lms.empty()) return false;
        if (!supportedLinkModes().supports(lms)) return false;
        // Otherwise its a single link mode.
        return 1 == countSetBits(lms.asBits());
    }
    bool sendTelegram(ContentStartsWith starts_with, vector<uchar> &content);
    void processSerialData();
    void simulate() { }

    LoRaIU880B(string alias, shared_ptr<SerialDevice> serial, shared_ptr<SerialCommunicationManager> manager);
    ~LoRaIU880B() {
    }

    static FrameStatus checkIU880BFrame(vector<uchar> &data,
                                        vector<uchar> &out,
                                        size_t *frame_length,
                                        int *endpoint_id_out,
                                        int *msg_id_out,
                                        int *status_out,
                                        int *rssi_dbm);

private:

    vector<uchar> read_buffer_;
    vector<uchar> request_;
    vector<uchar> response_;

    bool getDeviceInfo();
    bool loaded_device_info_ {};
    IU880BDeviceInfo device_info_;

    friend AccessCheck detectIU880B(Detected *detected, shared_ptr<SerialCommunicationManager> manager);
    void handleDevMgmt(int msgid, vector<uchar> &payload);
    void handleRadioLink(int msgid, vector<uchar> &payload, int rssi_dbm);
    void handleRadioLinkTest(int msgid, vector<uchar> &payload);
    void handleHWTest(int msgid, vector<uchar> &payload);

};

shared_ptr<WMBus> openIU880B(Detected detected, shared_ptr<SerialCommunicationManager> manager, shared_ptr<SerialDevice> serial_override)
{
    string bus_alias = detected.specified_device.bus_alias;
    string device_file = detected.found_file;
    assert(device_file != "");
    if (serial_override)
    {
        LoRaIU880B *imp = new LoRaIU880B(bus_alias, serial_override, manager);
        imp->markAsNoLongerSerial();
        return shared_ptr<WMBus>(imp);
    }

    auto serial = manager->createSerialDeviceTTY(device_file.c_str(), 115200, PARITY::NONE, "iu880b");
    LoRaIU880B *imp = new LoRaIU880B(bus_alias, serial, manager);
    return shared_ptr<WMBus>(imp);
}

LoRaIU880B::LoRaIU880B(string alias, shared_ptr<SerialDevice> serial, shared_ptr<SerialCommunicationManager> manager) :
    WMBusCommonImplementation(alias, WMBusDeviceType::DEVICE_IU880B, manager, serial, true)
{
    reset();
}

bool LoRaIU880B::ping()
{
    /*
    if (serial()->readonly()) return true; // Feeding from stdin or file.

    LOCK_WMBUS_EXECUTING_COMMAND(ping);

    request_.resize(4);
    request_[0] = IU880B_SERIAL_SOF;
    request_[1] = DEVMGMT_ID;
    request_[2] = DEVMGMT_MSG_PING_REQ;
    request_[3] = 0;

    verbose("(iu880b) ping\n");
    bool sent = serial()->send(request_);

    if (sent) return waitForResponse(DEVMGMT_MSG_PING_RSP);
    */
    return true;
}

string LoRaIU880B::getDeviceId()
{
    if (serial()->readonly()) return "?"; // Feeding from stdin or file.
    if (cached_device_id_ != "") return cached_device_id_;

    bool ok = getDeviceInfo();
    if (!ok) return "ER1R";

    cached_device_id_ = device_info_.uid;

    verbose("(iu880b) got device id %s\n", cached_device_id_.c_str());

    return cached_device_id_;
}

string LoRaIU880B::getDeviceUniqueId()
{
    return getDeviceId();
}

uchar LoRaIU880B::getFirmwareVersion()
{
    return 0;
}

LinkModeSet LoRaIU880B::getLinkModes()
{
    return LORA_bit;
}

void LoRaIU880B::deviceReset()
{
    // No device specific settings needed right now.
    // The common code in wmbus.cc reset()
    // will open the serial device and potentially
    // set the link modes properly.
}

void LoRaIU880B::deviceSetLinkModes(LinkModeSet lms)
{
    if (serial()->readonly()) return; // Feeding from stdin or file.

    if (!canSetLinkModes(lms))
    {
        string modes = lms.hr();
        error("(iu880b) setting link mode(s) %s is not supported for iu880b\n", modes.c_str());
    }

    LOCK_WMBUS_EXECUTING_COMMAND(set_link_modes);

    vector<uchar> init;
    init.resize(30);
    for (int i=0; i<30; ++i)
    {
        init[i] = 0xc0;
    }
    // Wake-up the dongle.
    serial()->send(init);

    vector<uchar> body = { 0x02 }; // 2 means listen to all traffic
    request_.clear();
    buildRequest(DEVMGMT_ID, DEVMGMT_MSG_SET_RADIO_MODE_REQ, body, request_);

    verbose("(iu880b) set link mode lora listen to all\n");

    bool sent = serial()->send(request_);
    if (!sent) return; // tty overridden with stdin/file

    bool ok = waitForResponse(DEVMGMT_MSG_SET_RADIO_MODE_RSP);
    if (!ok) return; // timeout
}

FrameStatus LoRaIU880B::checkIU880BFrame(vector<uchar> &data,
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

void LoRaIU880B::processSerialData()
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
        FrameStatus status = checkIU880BFrame(read_buffer_,
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
                debugPayload("(iu880b) partial frame, expecting more.", read_buffer_);
            }
            break;
        }
        if (status == ErrorInFrame)
        {
            debugPayload("(iu880b) bad frame, clearing.", read_buffer_);
            read_buffer_.clear();
            break;
        }
        if (status == FullFrame)
        {
            read_buffer_.erase(read_buffer_.begin(), read_buffer_.begin()+frame_length);

            // We now have a proper message in payload. Let us trigger actions based on it.
            // It can be wmbus receiver-dongle messages or wmbus remote meter messages received over the radio.
            switch (endpoint_id) {
            case DEVMGMT_ID: handleDevMgmt(msg_id, payload); break;
            case RADIOLINK_ID: handleRadioLink(msg_id, payload, rssi_dbm); break;
            case HWTEST_ID: handleHWTest(msg_id, payload); break;
            }
        }
    }
}

void LoRaIU880B::handleDevMgmt(int msgid, vector<uchar> &payload)
{
    switch (msgid) {
        case DEVMGMT_MSG_PING_RSP:
            verbose("(iu880b) pong\n");
            notifyResponseIsHere(DEVMGMT_MSG_PING_RSP);
            break;
        case DEVMGMT_MSG_GET_DEVICE_INFO_RSP:
            verbose("(iu880b) device info completed\n");
            response_.clear();
            response_.insert(response_.end(), payload.begin(), payload.end());
            notifyResponseIsHere(DEVMGMT_MSG_GET_DEVICE_INFO_RSP);
            break;
        case DEVMGMT_MSG_SET_RADIO_MODE_RSP:
            verbose("(iu880b) device set radio mode completed\n");
            response_.clear();
            response_.insert(response_.end(), payload.begin(), payload.end());
            notifyResponseIsHere(DEVMGMT_MSG_SET_RADIO_MODE_RSP);
            break;
        case DEVMGMT_MSG_GET_FW_INFO_RSP:
            verbose("(iu880b) device get firmware\n");
            response_.clear();
            response_.insert(response_.end(), payload.begin(), payload.end());
            notifyResponseIsHere(DEVMGMT_MSG_GET_FW_INFO_RSP);
            break;

    default:
        verbose("(iu880b) Unhandled device management message %d\n", msgid);
    }
}

void LoRaIU880B::handleRadioLink(int msgid, vector<uchar> &frame, int rssi_dbm)
{
}

void LoRaIU880B::handleRadioLinkTest(int msgid, vector<uchar> &payload)
{
}

void LoRaIU880B::handleHWTest(int msgid, vector<uchar> &payload)
{
}


bool LoRaIU880B::getDeviceInfo()
{
    if (loaded_device_info_) return true;

    LOCK_WMBUS_EXECUTING_COMMAND(get_device_info);

    vector<uchar> empty_body;
    request_.clear();
    buildRequest(DEVMGMT_ID, DEVMGMT_MSG_GET_DEVICE_INFO_REQ, empty_body, request_);

    verbose("(iu880b) get device info\n");

    vector<uchar> init;
    init.resize(30);
    for (int i=0; i<30; ++i)
    {
        init[i] = 0xc0;
    }
    // Wake-up the dongle.
    serial()->send(init);

    bool sent = serial()->send(request_);
    if (!sent) return false; // tty overridden with stdin/file

    bool ok = waitForResponse(DEVMGMT_MSG_GET_DEVICE_INFO_RSP);
    if (!ok) return false; // timeout

    // Now device info response is in response_ vector.
    device_info_.decode(response_);

    loaded_device_info_ = true;
    verbose("(iu880b) device info: %s\n", device_info_.str().c_str());

    request_.clear();
    buildRequest(DEVMGMT_ID, DEVMGMT_MSG_GET_FW_INFO_REQ, empty_body, request_);

    serial()->send(init);
    sent = serial()->send(request_);
    if (!sent) return false; // tty overridden with stdin/file

    ok = waitForResponse(DEVMGMT_MSG_GET_FW_INFO_RSP);
    if (!ok) return false; // timeout

    verbose("(iu880b) get firmware\n");

    return true;
}

bool LoRaIU880B::sendTelegram(ContentStartsWith starts_with, vector<uchar> &content)
{
    return false;
}

static void buildRequest(int endpoint_id, int msg_id, vector<uchar>& body, vector<uchar>& out)
{
    vector<uchar> request;
    request.push_back(endpoint_id);
    request.push_back(msg_id);
    request.insert(request.end(), body.begin(), body.end());

    uint16_t crc = ~crc16_CCITT(&request[0], request.size());
    request.push_back(crc & 0xff);
    request.push_back(crc >> 8);

    addSlipFraming(request, out);
}


AccessCheck detectIU880B(Detected *detected, shared_ptr<SerialCommunicationManager> manager)
{
    assert(detected->found_file != "");

    // Talk to the device and expect a very specific answer.
    auto serial = manager->createSerialDeviceTTY(detected->found_file.c_str(), 115200, PARITY::NONE, "detect iu880b");
    serial->disableCallbacks();
    bool ok = serial->open(false);
    if (!ok)
    {
        verbose("(iu880b) could not open tty %s for detection\n", detected->found_file.c_str());
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
    buildRequest(DEVMGMT_ID, DEVMGMT_MSG_GET_DEVICE_INFO_REQ, body, request);
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

    FrameStatus status = LoRaIU880B::checkIU880BFrame(response,
                                                      payload,
                                                      &frame_length,
                                                      &endpoint_id,
                                                      &msg_id,
                                                      &status_byte,
                                                      &rssi_dbm);

    if (status != FullFrame ||
        endpoint_id != DEVMGMT_ID ||
        msg_id != DEVMGMT_MSG_GET_DEVICE_INFO_RSP)
    {
        verbose("(iu880b) are you there? no.\n");
        serial->close();
        return AccessCheck::NoProperResponse;
    }

    serial->close();

    debugPayload("(iu880b) device info response", payload);

    debug("(iu880b) endpoint %02x msg %02x status %02x\n", endpoint_id, msg_id, status_byte);

    IU880BDeviceInfo di;
    di.decode(payload);

    debug("(iu880b) info: %s\n", di.str().c_str());

    detected->setAsFound(di.uid, WMBusDeviceType::DEVICE_IU880B, 115200, false, detected->specified_device.linkmodes);

    verbose("(iu880b) are you there? yes %s\n", di.uid.c_str());

    return AccessCheck::AccessOK;
}
