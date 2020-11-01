/*
 Copyright (C) 2018-2020 Fredrik Öhrström

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
#include"wmbus_amb8465.h"
#include"serial.h"
#include"threads.h"

#include<assert.h>
#include<pthread.h>
#include<errno.h>
#include<unistd.h>
#include<sys/time.h>
#include<time.h>

using namespace std;

struct ConfigAMB8465
{
    uchar uart_ctl0 {};
    uchar uart_ctl1 {};
    uchar received_frames_as_cmd {};
    uchar c_field {};
    uint16_t mfct {};
    uint32_t id {};
    uchar version {};
    uchar media {};

    uchar auto_rssi {};

    string dongleId()
    {
        return tostrprintf("%08x", id);
    }

    string str()
    {
        string ids = tostrprintf("id=%08x media=%02x version=%02x c_field=%02x auto_rssi=%02x", id, media, version, c_field, auto_rssi);
        return ids;
    }

    bool decode(vector<uchar> &bytes)
    {
        size_t o = 5;
        if (bytes.size() < o) return false;

        uart_ctl0 = bytes[0+o];
        uart_ctl1 = bytes[0+o];

        received_frames_as_cmd = bytes[5+o];
        c_field = bytes[49+o];
        id = bytes[51+o]<<8|bytes[50+o];
        mfct = bytes[55+o]<<24|bytes[54+o]<<16|bytes[53+o]<<8|bytes[52+o];
        version = bytes[56+o];
        media = bytes[57+o];

        auto_rssi = bytes[69+o];

        return true;
    }
};

struct WMBusAmber : public virtual WMBusCommonImplementation
{
    bool ping();
    string getDeviceId();
    string getDeviceUniqueId();
    LinkModeSet getLinkModes();
    void deviceSetLinkModes(LinkModeSet lms);
    void deviceReset();
    LinkModeSet supportedLinkModes()
    {
        return
            C1_bit |
            S1_bit |
            S1m_bit |
            T1_bit;
    }
    int numConcurrentLinkModes() { return 1; }
    bool canSetLinkModes(LinkModeSet desired_modes)
    {
        if (desired_modes.empty()) return false;
        // Simple check first, are they all supported?
        if (!supportedLinkModes().supports(desired_modes)) return false;
        // So far so good, is the desired combination supported?
        // If only a single bit is desired, then it is supported.
        if (1 == countSetBits(desired_modes.asBits())) return true;
        // More than 2 listening modes at the same time will always fail.
        if (2 != countSetBits(desired_modes.asBits())) return false;
        // C1 and T1 can be listened to at the same time!
        if (desired_modes.has(LinkMode::C1) && desired_modes.has(LinkMode::T1)) return true;
        // Likewise for S1 and S1-m
        if (desired_modes.has(LinkMode::S1) || desired_modes.has(LinkMode::S1m)) return true;
        // Any other combination is forbidden.
        return false;
    }
    void processSerialData();
    bool getConfiguration();
    void simulate() { }

    WMBusAmber(shared_ptr<SerialDevice> serial, shared_ptr<SerialCommunicationManager> manager);
    ~WMBusAmber() {
        manager_->onDisappear(this->serial(), NULL);
    }

private:
    vector<uchar> read_buffer_;
    vector<uchar> request_;
    vector<uchar> response_;

    LinkModeSet link_modes_ {};
    bool rssi_expected_ {};
    struct timeval timestamp_last_rx_ {};

    ConfigAMB8465 device_config_;

    FrameStatus checkAMB8465Frame(vector<uchar> &data,
                                  size_t *frame_length,
                                  int *msgid_out,
                                  int *payload_len_out,
                                  int *payload_offset,
                                  int *rssi_dbm);
    void handleMessage(int msgid, vector<uchar> &frame, int rssi_dbm);
};

shared_ptr<WMBus> openAMB8465(string device, shared_ptr<SerialCommunicationManager> manager, shared_ptr<SerialDevice> serial_override)
{
    assert(device != "");

    if (serial_override)
    {
        WMBusAmber *imp = new WMBusAmber(serial_override, manager);
        imp->markAsNoLongerSerial();
        return shared_ptr<WMBus>(imp);
    }

    auto serial = manager->createSerialDeviceTTY(device.c_str(), 9600, "amb8465");
    WMBusAmber *imp = new WMBusAmber(serial, manager);
    return shared_ptr<WMBus>(imp);
}

WMBusAmber::WMBusAmber(shared_ptr<SerialDevice> serial, shared_ptr<SerialCommunicationManager> manager) :
    WMBusCommonImplementation(DEVICE_AMB8465, manager, serial, true)
{
    rssi_expected_ = true;
    reset();
}

void WMBusAmber::deviceReset()
{
    timerclear(&timestamp_last_rx_);
}

uchar xorChecksum(vector<uchar> &msg, size_t len)
{
    assert(msg.size() >= len);
    uchar c = 0;
    for (size_t i=0; i<len; ++i) {
        c ^= msg[i];
    }
    return c;
}

bool WMBusAmber::ping()
{
    if (serial()->readonly()) return true; // Feeding from stdin or file.

    return true;
}

string WMBusAmber::getDeviceId()
{
    if (serial()->readonly()) { return "?"; }  // Feeding from stdin or file.
    if (cached_device_id_ != "") return cached_device_id_;

    bool ok = getConfiguration();
    if (!ok) return "ERR";

    cached_device_id_ = device_config_.dongleId();

    return cached_device_id_;
}

string WMBusAmber::getDeviceUniqueId()
{
    if (serial()->readonly()) { return "?"; }  // Feeding from stdin or file.
    if (cached_device_unique_id_ != "") return cached_device_unique_id_;

    LOCK_WMBUS_EXECUTING_COMMAND(get_device_unique_id);

    request_.resize(4);
    request_[0] = AMBER_SERIAL_SOF;
    request_[1] = CMD_SERIALNO_REQ;
    request_[2] = 0; // No payload
    request_[3] = xorChecksum(request_, 3);

    verbose("(amb8465) get device unique id\n");
    bool sent = serial()->send(request_);
    if (!sent) return "?";

    bool ok = waitForResponse(CMD_SERIALNO_REQ | 0x80);
    if (!ok) return "?";

    if (response_.size() < 5) return "ERR";

    uint32_t idv =
        response_[1] << 24 |
        response_[2] << 16 |
        response_[3] << 8 |
        response_[4];

    verbose("(amb8465) unique device id %08x\n", idv);

    cached_device_unique_id_ = tostrprintf("%08x", idv);
    return cached_device_unique_id_;
}

LinkModeSet WMBusAmber::getLinkModes()
{
    if (serial()->readonly()) { return Any_bit; }  // Feeding from stdin or file.

    // It is not possible to read the volatile mode set using setLinkModeSet below.
    // (It is possible to read the non-volatile settings, but this software
    // does not change those.) So we remember the state for the device.
    return link_modes_;
}

bool WMBusAmber::getConfiguration()
{
    if (serial()->readonly()) { return true; }  // Feeding from stdin or file.

    LOCK_WMBUS_EXECUTING_COMMAND(getConfiguration);

    request_.resize(6);
    request_[0] = AMBER_SERIAL_SOF;
    request_[1] = CMD_GET_REQ;
    request_[2] = 0x02;
    request_[3] = 0x00;
    request_[4] = 0x80;
    request_[5] = xorChecksum(request_, 5);

    assert(request_[5] == 0x77);

    verbose("(amb8465) get config\n");
    bool sent = serial()->send(request_);
    if (!sent) return false;

    bool ok = waitForResponse(CMD_GET_REQ | 0x80);
    if (!ok) return false;

    return device_config_.decode(response_);
}

void WMBusAmber::deviceSetLinkModes(LinkModeSet lms)
{
    if (serial()->readonly()) return; // Feeding from stdin or file.

    if (!canSetLinkModes(lms))
    {
        string modes = lms.hr();
        error("(amb8465) setting link mode(s) %s is not supported for amb8465\n", modes.c_str());
    }

    LOCK_WMBUS_EXECUTING_COMMAND(devicesSetLinkModes);

    request_.resize(8);
    request_[0] = AMBER_SERIAL_SOF;
    request_[1] = CMD_SET_MODE_REQ;
    request_[2] = 1; // Len
    if (lms.has(LinkMode::C1) && lms.has(LinkMode::T1))
    {
        // Listening to both C1 and T1!
        request_[3] = 0x09;
    }
    else if (lms.has(LinkMode::C1))
    {
        // Listening to only C1.
        request_[3] = 0x0E;
    }
    else if (lms.has(LinkMode::T1))
    {
        // Listening to only T1.
        request_[3] = 0x08;
    }
    else if (lms.has(LinkMode::S1) || lms.has(LinkMode::S1m))
    {
        // Listening only to S1 and S1-m
        request_[3] = 0x03;
    }
    request_[4] = xorChecksum(request_, 4);

    verbose("(amb8465) set link mode %02x\n", request_[3]);
    bool sent = serial()->send(request_);

    if (sent)
    {
        bool ok = waitForResponse(CMD_SET_MODE_REQ | 0x80);
        if (!ok)
        {
            warning("Warning! Did not get confirmation on set link mode for amb8465\n");
        }
    }

    link_modes_ = lms;
}

FrameStatus WMBusAmber::checkAMB8465Frame(vector<uchar> &data,
                                          size_t *frame_length,
                                          int *msgid_out,
                                          int *payload_len_out,
                                          int *payload_offset,
                                          int *rssi_dbm)
{
    if (data.size() < 2) return PartialFrame;
    debugPayload("(amb8465) checkAMB8465Frame", data);
    int payload_len = 0;
    if (data[0] == 0xff)
    {
        if (data.size() < 3)
        {
            debug("(amb8465) not enough bytes yet for command.\n");
            return PartialFrame;
        }

        // Only response from CMD_DATA_IND has rssi
        int rssi_len = (rssi_expected_ && data[1] == (0x80|CMD_DATA_IND)) ? 1 : 0;

        // A command response begins with 0xff
        *msgid_out = data[1];
        payload_len = data[2];
        *payload_len_out = payload_len;
        *payload_offset = 3;
        // FF CMD len payload [RSSI] CS
        *frame_length = 4 + payload_len + rssi_len;
        if (data.size() < *frame_length)
        {
            debug("(amb8465) not enough bytes yet, partial command response %d %d.\n", data.size(), *frame_length);
            return PartialFrame;
        }

        debug("(amb8465) received full command frame\n");

        uchar cs = xorChecksum(data, *frame_length-1);
        if (data[*frame_length-1] != cs) {
            verbose("(amb8465) checksum error %02x (should %02x)\n", data[*frame_length-1], cs);
        }

        if (rssi_len)
        {
            int rssi = (int)data[*frame_length-2];
            *rssi_dbm = (rssi >= 128) ? (rssi - 256) / 2 - 74 : rssi / 2 - 74;
            verbose("(amb8465) rssi %d (%d dBm)\n", rssi, *rssi_dbm);
        }

      return FullFrame;
    }
    // If it is not a 0xff we assume it is a message beginning with a length.
    // There might be a different mode where the data is wrapped in 0xff. But for the moment
    // this is what I see.
    size_t offset = 0;

    // The data[0] must be at least 10 bytes. C MM AAAA V T Ci
    // And C must be 0x44.
    while ((payload_len = data[offset]) < 10 || data[offset+1] != 0x44)
    {
        offset++;
        if (offset + 2 >= data.size()) {
            // No sensible telegram in the buffer. Flush it!
            // But not the last char, because the next char could be a 0x44
            verbose("(amb8465) no sensible telegram found, clearing buffer.\n");
            uchar last = data[data.size()-1];
            data.clear();
            data.insert(data.end(), &last, &last+1); // Re-insert the last byte.
            return PartialFrame;
        }
    }
    *msgid_out = 0; // 0 is used to signal
    *payload_len_out = payload_len;
    *payload_offset = offset+1;
    *frame_length = payload_len+offset+1;
    if (data.size() < *frame_length)
    {
        debug("(amb8465) not enough bytes yet, partial frame %d %d.\n", data.size(), *frame_length);
        return PartialFrame;
    }

    if (offset > 0)
    {
        verbose("(amb8465) out of sync, skipping %d bytes.\n", offset);
    }
    debug("(amb8465) received full frame\n");

    if (rssi_expected_)
    {
        int rssi = data[*frame_length-1];
        *rssi_dbm = (rssi >= 128) ? (rssi - 256) / 2 - 74 : rssi / 2 - 74;
        verbose("(amb8465) rssi %d (%d dBm)\n", rssi, *rssi_dbm);
    }

    return FullFrame;
}

void WMBusAmber::processSerialData()
{
    vector<uchar> data;

    // Receive and accumulated serial data until a full frame has been received.
    serial()->receive(&data);

    struct timeval timestamp;

    // Check long delay beetween rx chunks
    gettimeofday(&timestamp, NULL);

    if (read_buffer_.size() > 0 && timerisset(&timestamp_last_rx_)) {
        struct timeval chunk_time;
        timersub(&timestamp, &timestamp_last_rx_, &chunk_time);

        if (chunk_time.tv_sec >= 2) {
            verbose("(amb8465) rx long delay (%lds), drop incomplete telegram\n", chunk_time.tv_sec);
            read_buffer_.clear();
            protocolErrorDetected();
        }
        else
        {
            unsigned long chunk_time_ms = 1000 * chunk_time.tv_sec + chunk_time.tv_usec / 1000;
            debug("(amb8465) chunk time %ld msec\n", chunk_time_ms);
        }
    }

    read_buffer_.insert(read_buffer_.end(), data.begin(), data.end());

    size_t frame_length;
    int msgid;
    int payload_len, payload_offset;
    int rssi_dbm;

    for (;;)
    {
        FrameStatus status = checkAMB8465Frame(read_buffer_, &frame_length, &msgid, &payload_len, &payload_offset, &rssi_dbm);

        if (status == PartialFrame)
        {
            if (read_buffer_.size() > 0) {
                // Save timestamp of this chunk
                timestamp_last_rx_ = timestamp;
            }
            else
            {
                // Clean and empty
                timerclear(&timestamp_last_rx_);
            }
            break;
        }
        if (status == ErrorInFrame)
        {
            verbose("(amb8465) protocol error in message received!\n");
            string msg = bin2hex(read_buffer_);
            debug("(amb8465) protocol error \"%s\"\n", msg.c_str());
            read_buffer_.clear();
            protocolErrorDetected();
            break;
        }
        if (status == FullFrame)
        {
            vector<uchar> payload;
            if (payload_len > 0)
            {
                uchar l = payload_len;
                payload.insert(payload.end(), &l, &l+1); // Re-insert the len byte.
                payload.insert(payload.end(), read_buffer_.begin()+payload_offset, read_buffer_.begin()+payload_offset+payload_len);
            }

            read_buffer_.erase(read_buffer_.begin(), read_buffer_.begin()+frame_length);

            handleMessage(msgid, payload, rssi_dbm);
        }
    }
}

void WMBusAmber::handleMessage(int msgid, vector<uchar> &frame, int rssi_dbm)
{
    switch (msgid) {
    case (0):
    {
        AboutTelegram about("amb8465["+cached_device_id_+"]", rssi_dbm);
        handleTelegram(about, frame);
        break;
    }
    case (0x80|CMD_SET_MODE_REQ):
    {
        verbose("(amb8465) set link mode completed\n");
        response_.clear();
        response_.insert(response_.end(), frame.begin(), frame.end());
        debugPayload("(amb8465) set link mode response", response_);
        notifyResponseIsHere(0x80|CMD_SET_MODE_REQ);
        break;
    }
    case (0x80|CMD_GET_REQ):
    {
        verbose("(amb8465) get config completed\n");
        response_.clear();
        response_.insert(response_.end(), frame.begin(), frame.end());
        debugPayload("(amb8465) get config response", response_);
        notifyResponseIsHere(0x80|CMD_GET_REQ);
        break;
    }
    case (0x80|CMD_SERIALNO_REQ):
    {
        verbose("(amb8465) get device id completed\n");
        response_.clear();
        response_.insert(response_.end(), frame.begin(), frame.end());
        debugPayload("(amb8465) get device id response", response_);
        notifyResponseIsHere(0x80|CMD_SERIALNO_REQ);
        break;
    }
    default:
        verbose("(amb8465) unhandled device message %d\n", msgid);
        response_.clear();
        response_.insert(response_.end(), frame.begin(), frame.end());
        debugPayload("(amb8465) unknown response", response_);
    }
}

AccessCheck detectAMB8465(Detected *detected, shared_ptr<SerialCommunicationManager> manager)
{
    // Talk to the device and expect a very specific answer.
    auto serial = manager->createSerialDeviceTTY(detected->found_file.c_str(), 9600, "detect amb8465");
    serial->disableCallbacks();
    AccessCheck rc = serial->open(false);
    if (rc != AccessCheck::AccessOK) return AccessCheck::NotThere;

    vector<uchar> response;
    // First clear out any data in the queue.
    serial->receive(&response);
    response.clear();

    vector<uchar> request;
    request.resize(6);
    request[0] = AMBER_SERIAL_SOF;
    request[1] = CMD_GET_REQ;
    request[2] = 0x02;
    request[3] = 0x00;
    request[4] = 0x80;
    request[5] = xorChecksum(request, 5);

    assert(request[5] == 0x77);

    bool sent = serial->send(request);
    if (!sent) {
        serial->close();
        return AccessCheck::NotThere;
    }

    serial->send(request);
    // Wait for 100ms so that the USB stick have time to prepare a response.
    usleep(1000*100);

    vector<uchar> data;
    int count = 0;
    while (response.size() < 0x7E && count < 2)
    {
        size_t n = serial->receive(&data);
        if (n == 0)
        {
            usleep(1000*100);
            count++;
            continue;
        }
        response.insert(response.end(), data.begin(), data.end());
    }

    serial->close();

    // FF8A7A00780080710200000000FFFFFA00FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF003200021400FFFFFFFFFF010004000000FFFFFF01440000000000000000FFFF0B040100FFFFFFFFFF00030000FFFFFFFFFFFFFF0000FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF17

    if (response.size() < 8 ||
        response[0] != 0xff ||
        response[1] != (0x80 | request[1]) ||
        response[2] != 0x7A)
    {
        verbose("(amb8465) are you there? no.\n");
        serial->close();
        return AccessCheck::NotThere;
    }

    serial->close();

    ConfigAMB8465 config;
    config.decode(response);

    detected->setAsFound(config.dongleId(), WMBusDeviceType::DEVICE_AMB8465, 9600, false, false,
        detected->specified_device.linkmodes);

    verbose("(amb8465) detect %s\n", config.str().c_str());
    verbose("(amb8465) are you there? yes %s\n", config.dongleId().c_str());

    return AccessCheck::AccessOK;
}

static AccessCheck tryFactoryResetAMB8465(string device, shared_ptr<SerialCommunicationManager> manager, int baud)
{
    // Talk to the device and expect a very specific answer.
    auto serial = manager->createSerialDeviceTTY(device.c_str(), baud, "reset amb8465");
    AccessCheck rc = serial->open(false);
    if (rc != AccessCheck::AccessOK) {
        verbose("(amb8465) could not open device %s using baud %d\n", device.c_str(), baud);
        return AccessCheck::NotThere;
    }

    vector<uchar> data;
    // First clear out any data in the queue.
    serial->receive(&data);
    data.clear();

    vector<uchar> request_;
    request_.resize(4);
    request_[0] = AMBER_SERIAL_SOF;
    request_[1] = CMD_FACTORYRESET_REQ;
    request_[2] = 0; // No payload
    request_[3] = xorChecksum(request_, 3);

    assert(request_[3] == 0xee);

    verbose("(amb8465) try factory reset %s using baud %d\n", device.c_str(), baud);
    serial->send(request_);
    // Wait for 100ms so that the USB stick have time to prepare a response.
    usleep(1000*100);
    serial->receive(&data);
    int limit = 0;
    while (data.size() > 8 && data[0] != 0xff)
    {
        // Eat bytes until a 0xff appears to get in sync with the proper response.
        // Extraneous bytes might be due to a partially read telegram.
        data.erase(data.begin());
        vector<uchar> more;
        serial->receive(&more);
        if (more.size() > 0) {
            data.insert(data.end(), more.begin(), more.end());
        }
        if (limit++ > 100) break; // Do not wait too long.
    }

    serial->close();

    debugPayload("(amb8465) reset response", data);

    if (data.size() < 8 ||
        data[0] != 0xff ||
        data[1] != 0x90 ||
        data[2] != 0x01 ||
        data[3] != 0x00 || // Status should be 0.
        data[4] != xorChecksum(data, 4))
    {
        verbose("(amb8465) no response to factory reset %s using baud %d\n", device.c_str(), baud);
        return AccessCheck::NotThere;
    }
    verbose("(amb8465) received proper factory reset response %s using baud %d\n", device.c_str(), baud);
    return AccessCheck::AccessOK;
}

int bauds[] = { 1200, 2400, 4800, 9600, 19200, 38400, 56000, 115200, 0 };

AccessCheck factoryResetAMB8465(string device, shared_ptr<SerialCommunicationManager> manager, int *was_baud)
{
    AccessCheck rc = AccessCheck::NotThere;

    for (int i=0; bauds[i] != 0; ++i)
    {
        rc = tryFactoryResetAMB8465(device, manager, bauds[i]);
        if (rc == AccessCheck::AccessOK)
        {
            *was_baud = bauds[i];
            return AccessCheck::AccessOK;
        }
    }
    *was_baud = 0;
    return AccessCheck::NotThere;
}
