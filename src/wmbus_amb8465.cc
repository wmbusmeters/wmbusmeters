/*
 Copyright (C) 2018-2022 Fredrik Öhrström (gpl-3.0-or-later)

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

uchar xorChecksum(vector<uchar> &msg, size_t offset, size_t len);

struct ConfigAMB8465AMB3665
{
    BusDeviceType module_type {};
    uchar uart_ctl0 {};
    uchar uart_ctl1 {};
    uchar uart_cmd_out_enable {};

    uchar b1_add_disable {};

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
        string ids = tostrprintf("uart_cmd_out_enable=%02x id=%08x media=%02x version=%02x c_field=%02x "
                                 "auto_rssi=%02x b1_add_disable=%02x",
                                 uart_cmd_out_enable,
                                 id, media, version, c_field, auto_rssi, b1_add_disable);
        return ids;
    }

    bool decodeNoFrame(vector<uchar> &bytes, size_t o)
    {
        if (bytes.size() < o+69) return false;

        uart_ctl0 = bytes[0+o];
        uart_ctl1 = bytes[0+o];

        uart_cmd_out_enable = bytes[5+o];
        b1_add_disable = bytes[48+o];
        c_field = bytes[49+o];
        id = bytes[51+o]<<8|bytes[50+o];
        mfct = bytes[55+o]<<24|bytes[54+o]<<16|bytes[53+o]<<8|bytes[52+o];
        version = bytes[56+o];
        media = bytes[57+o];

        auto_rssi = bytes[69+o];
        return true;
    }

    bool decode8465(vector<uchar> &bytes, size_t offset)
    {
        // The first 5 bytes are:
        // 0xFF
        // 0x8A
        // <num_bytes+2[0x7a]>
        // <memory_start[0x00]>
        // <num_bytes[0x78]>
        // then follows the parameter bytes
        // 0x78 parameter bytes
        // <check_sum byte>
        // Total length 0x7e
        if (bytes.size() < offset+5) return false;
        if (bytes[offset+0] != 0xff ||
            bytes[offset+1] != 0x8a ||
            bytes[offset+2] != 0x7a ||
            bytes[offset+3] != 0x00 ||
            bytes[offset+4] != 0x78)
        {
            debug("(amb8465) not the right header decoding ConfigAMB8465!\n");
            return false;
        }
        if (bytes.size() < offset+0x7e)
        {
            debug("(amb8465) not enough data for decoding ConfigAMB8465!\n");
            return false;
        }
        size_t o = offset+5;

        decodeNoFrame(bytes, o);

        uchar received_crc = bytes[offset + 0x7e - 1];
        uchar calculated_crc = xorChecksum(bytes, offset, 0x7e - 1);
        if (received_crc != calculated_crc)
        {
            debug("(amb8465) bad crc in response! Expected %02x but got %02x\n", calculated_crc, received_crc);
            return false;
        }

        string tmp = str();
        debug("(amb8465) proprely decoded ConfigAMB8465 response. Content: %s\n", tmp.c_str());

        return true;
    }

    bool decode3665(vector<uchar> &bytes, size_t offset)
    {
        // The first 5 bytes are:
        // 0xFF
        // 0x8A
        // <num_bytes+2[0x7a]
        // <memory_start[0x00]
        // <num_bytes[0x78]
        // then follows the parameter bytes
        // 0x78 parameter bytes
        // <check_sum byte
        // Total length 0x86
        if (bytes.size() < offset+5) return false;
        if (bytes[offset+0] != 0xff ||
            bytes[offset+1] != 0x8a ||
            bytes[offset+2] != 0x82 ||
            bytes[offset+3] != 0x00 ||
            bytes[offset+4] != 0x80)
        {
            debug("(amb3665) not the right header decoding ConfigAMB3665!\n");
            return false;
        }
        if (bytes.size() < offset+0x86)
        {
            debug("(amb3665) not enough data for decoding ConfigAMB3665!\n");
            return false;
        }
        size_t o = offset+5;

        decodeNoFrame(bytes, o);

        uchar received_crc = bytes[offset + 0x86 - 1];
        uchar calculated_crc = xorChecksum(bytes, offset, 0x7e - 1);
        if (received_crc != calculated_crc)
        {
            debug("(amb3665) bad crc in response! Expected %02x but got %02x\n", calculated_crc, received_crc);
            return false;
        }

        string tmp = str();
        debug("(amb3665) proprely decoded ConfigAMB3665 response. Content: %s\n", tmp.c_str());

        return true;
    }

};

/*
Which receive mode can hear which transmit mode?

868 MHz

Transmit         Receive
------------------------------------------------------------------------
S1 0x01   -->  S2 (0x03)
S1-m 0x02 -->  S2 (0x03)
S2 0x03   -->  S2 (0x03)

T1-Meter 0x05 (to_collector) --> T2-Other (0x08) or T2/C2-Other (0x09)
T1-Other 0x06 (to_meter)     --> T2-Meter (0x07)
T2-Meter 0x07 (to_collector) --> T2-Other (0x08) or T2/C2-Other (0x09)
T2-Other 0x08 (to_meter)     --> T2-Meter (0x07)

T2/C2-Other 0x09 (to_collector) transmit uses last received mode T2 or C2. -->

R2-Meter 0x0A (to_collector) --> R2-Other (0x0B)
R2-Other 0x0B (to_meter)     --> R2-Meter (0x0A)

C1-Meter 0x0C (to_collector) --> C2-Other (0x0E) or T2/C2-Other (0x09)
C2-Meter 0x0D (to_collector) --> C2-Other (0x0E) or T2/C2-Other (0x09)
C2-Other 0x0E (to_meter)     --> C2-Meter (0x0D)

169 MHz

Transmit         Receive
------------------------------------------------------------------------
N1a 0x01 --> N2a (0x02)
N2a 0x02 --> N2a (0x02)
N1b 0x03 --> N2b (0x04)
N2b 0x04 --> N2b (0x04)
N1c 0x05 --> N2c (0x06)
N2c 0x06 --> N2c (0x06)
N1d 0x07 --> N2d (0x08)
N2d 0x08 --> N2d (0x08)
N1e 0x09 --> N2e (0x0A)
N2e 0x0A --> N2e (0x0A)
N1f 0x0B --> N2f (0x0C)
N2f 0x0C --> N2f (0x0C)
N2g 0x0D --> N2f (0x0D)

*/

uchar setupAmberBusDeviceToReceiveTelegrams(LinkModeSet lms)
{
    if (lms.has(LinkMode::C1) && lms.has(LinkMode::T1))
    {
        // Listening to meter transmissions on C1 and T1.
        // Using receive mode C2/T2-Other (0x09).
        return (int)LinkModeAMB::C2T2Other;
    }
    if (lms.has(LinkMode::S1) || lms.has(LinkMode::S1m) || lms.has(LinkMode::S2))
    {
        // Listening to S1, S1-m.
        // Using collector receive (and bi-directional) mode S2 (0x03).
        return (int)LinkModeAMB::S2;
    }
    if (lms.has(LinkMode::T1))
    {
        // Listening to meter transmissions T1 only.
        // Using collector receive mode T2-Other (0x08)
        return (int)LinkModeAMB::T2Other;
    }
    if (lms.has(LinkMode::T2))
    {
        // Listening to collector transmissions T1 only.
        // Using meter receive mode T2-Meter (0x07)
        return (int)LinkModeAMB::T2Meter;
    }
    if (lms.has(LinkMode::C1))
    {
        // Listening to meter transmissions on C1 only.
        // Using collector receive mode C2-Other (0x0e)
        return (int)LinkModeAMB::C2Other;
    }
    if (lms.has(LinkMode::C2))
    {
        // Listening to collector transmissions on C1 only.
        // Using meter receive mode C2-Meter (0x0d)
        return (int)LinkModeAMB::C2Meter;
    }
    if (lms.has(LinkMode::N1a))
    {
        // Listening to meter transmission N1a.
        // Using collector receive mode N2a (0x02).
        return (int)LinkModeAMB::N2a;
    }
    if (lms.has(LinkMode::N1b))
    {
        // Listening to meter transmission N1b.
        // Using collector receive mode N2b (0x04).
        return (int)LinkModeAMB::N2b;
    }
    if (lms.has(LinkMode::N1c))
    {
        // Listening to meter transmission N1c.
        // Using collector receive mode N2c (0x06).
        return (int)LinkModeAMB::N2c;
    }
    if (lms.has(LinkMode::N1d))
    {
        // Listening to meter transmission N1d.
        // Using collector receive mode N2d (0x08).
        return (int)LinkModeAMB::N2d;
    }
    if (lms.has(LinkMode::N1e))
    {
        // Listening to meter transmission N1e.
        // Using collector receive mode N2e (0x0a).
        return (int)LinkModeAMB::N2e;
    }
    if (lms.has(LinkMode::N1f))
    {
        // Listening to meter transmission N1f.
        // Using collector receive mode N2f (0x0c).
        return (int)LinkModeAMB::N2f;
    }

    assert(false);

    // Error
    return 0xff;
}

uchar setupAmberBusDeviceToSendTelegram(LinkMode lm)
{
    if (lm == LinkMode::S1)
    {
        // Send S1 telegram using mode S1 (0x01).
        return (int)LinkModeAMB::S1;
    }
    if (lm == LinkMode::S1m)
    {
        // Send S1 telegram using mode S1m (0x02).
        return (int)LinkModeAMB::S1m;
    }
    if (lm == LinkMode::S2)
    {
        // Bi-directional communication with meter using mode S2 (0x03).
        return (int)LinkModeAMB::S2;
    }
    if (lm == LinkMode::T1)
    {
        // Send T1 telegram using mode T1-Meter (0x05).
        return (int)LinkModeAMB::T1Meter;
    }
    if (lm == LinkMode::T2)
    {
        // Send T2 telegram to meter using mode T2-Other (0x06).
        return (int)LinkModeAMB::T2Other;
    }
    if (lm == LinkMode::C1)
    {
        // Send C1 telegram using mode C1-Meter (0x0c).
        return (int)LinkModeAMB::C1Meter;
    }
    if (lm == LinkMode::C2)
    {
        // Send C2 telegram to meter using mode C2-Other (0x0e).
        return (int)LinkModeAMB::C2Other;
    }

    if (lm == LinkMode::N1a) return (int)LinkModeAMB::N1a;
    if (lm == LinkMode::N1b) return (int)LinkModeAMB::N1b;
    if (lm == LinkMode::N1c) return (int)LinkModeAMB::N1c;
    if (lm == LinkMode::N1d) return (int)LinkModeAMB::N1d;
    if (lm == LinkMode::N1e) return (int)LinkModeAMB::N1e;
    if (lm == LinkMode::N1f) return (int)LinkModeAMB::N1f;

    assert(false);

    // Error
    return 0xff;
}

struct WMBusAmber : public virtual BusDeviceCommonImplementation
{
    bool ping();
    string getDeviceId();
    string getDeviceUniqueId();
    LinkModeSet getLinkModes();
    bool deviceSetLinkModes(LinkModeSet lms);
    void deviceReset();
    LinkModeSet supportedLinkModes()
    {
        if (type() == BusDeviceType::DEVICE_AMB8465)
        {
            return
                C1_bit |
                C2_bit |
                S1_bit |
                S1m_bit |
                S2_bit |
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
    bool sendTelegram(LinkMode lm, TelegramFormat format, vector<uchar> &content);

    WMBusAmber(string alias, shared_ptr<SerialDevice> serial, shared_ptr<SerialCommunicationManager> manager, BusDeviceType dt);
    ~WMBusAmber() {
        manager_->onDisappear(this->serial(), NULL);
    }

private:
    vector<uchar> read_buffer_; // Must be protected by LOCK_WMBUS_RECEIVING_BUFFER(where)
    vector<uchar> request_;
    vector<uchar> response_;

    LinkModeSet link_modes_ {};
    uchar last_set_link_mode_ { 0x01 };
    bool rssi_expected_ {};
    struct timeval timestamp_last_rx_ {};

    ConfigAMB8465AMB3665 device_config_;

    FrameStatus checkAMB8465Frame(vector<uchar> &data,
                                  size_t *frame_length,
                                  int *msgid_out,
                                  int *payload_len_out,
                                  int *payload_offset,
                                  int *rssi_dbm);
    void handleMessage(int msgid, vector<uchar> &frame, int rssi_dbm);
};

shared_ptr<BusDevice> openAmber(Detected detected,
                                shared_ptr<SerialCommunicationManager> manager,
                                shared_ptr<SerialDevice> serial_override,
                                BusDeviceType dt)
{
    string bus_alias  = detected.specified_device.bus_alias;
    string device = detected.found_file;
    assert(device != "");

    if (serial_override)
    {
        WMBusAmber *imp = new WMBusAmber(bus_alias, serial_override, manager, dt);
        imp->markAsNoLongerSerial();
        return shared_ptr<BusDevice>(imp);
    }

    auto serial = manager->createSerialDeviceTTY(device.c_str(), 9600, PARITY::NONE, "amb8465");
    WMBusAmber *imp = new WMBusAmber(bus_alias, serial, manager, dt);
    return shared_ptr<BusDevice>(imp);
}

shared_ptr<BusDevice> openAMB8465(Detected detected, shared_ptr<SerialCommunicationManager> manager, shared_ptr<SerialDevice> serial_override)
{
    return openAmber(detected, manager, serial_override, DEVICE_AMB8465);
}

shared_ptr<BusDevice> openAMB3665(Detected detected, shared_ptr<SerialCommunicationManager> manager, shared_ptr<SerialDevice> serial_override)
{
    return openAmber(detected, manager, serial_override, DEVICE_AMB3665);
}

WMBusAmber::WMBusAmber(string alias,
                       shared_ptr<SerialDevice> serial,
                       shared_ptr<SerialCommunicationManager> manager,
                       BusDeviceType dt) :
    BusDeviceCommonImplementation(alias, dt, manager, serial, true)
{
    rssi_expected_ = true;
    reset();
}

void WMBusAmber::deviceReset()
{
    timerclear(&timestamp_last_rx_);
}

uchar xorChecksum(vector<uchar> &msg, size_t offset, size_t len)
{
    assert(msg.size() >= len+offset);
    uchar c = 0;
    for (size_t i=offset; i<len+offset; ++i) {
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
    request_[3] = xorChecksum(request_, 0, 3);

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
    request_[5] = xorChecksum(request_, 0, 5);

    assert(request_[5] == 0x77);

    verbose("(amb8465) get config\n");
    bool sent = serial()->send(request_);
    if (!sent) return false;

    bool ok = waitForResponse(CMD_GET_REQ | 0x80);
    if (!ok) return false;

    return device_config_.decodeNoFrame(response_, 3);
}

bool WMBusAmber::deviceSetLinkModes(LinkModeSet lms)
{
    bool rc = false;

    if (serial()->readonly()) return true; // Feeding from stdin or file.

    if (!canSetLinkModes(lms))
    {
        string modes = lms.hr();
        error("(amb8465) setting link mode(s) %s is not supported for amb8465 \n", modes.c_str());
    }

    {
        // Empty the read buffer we do not want any partial data lying around
        // because we expect a response to arrive.
        LOCK_WMBUS_RECEIVING_BUFFER(deviceSetLinkMode_ClearBuffer);
        read_buffer_.clear();
    }

    {
        LOCK_WMBUS_EXECUTING_COMMAND(devicesSetLinkModes);

        request_.resize(5);
        request_[0] = AMBER_SERIAL_SOF;
        request_[1] = CMD_SET_MODE_REQ;
        request_[2] = 1; // Len
        request_[3] = setupAmberBusDeviceToReceiveTelegrams(lms);
        request_[4] = xorChecksum(request_, 0, 4);

        verbose("(amb8465) set link mode %02x\n", request_[3]);
        bool sent = serial()->send(request_);

        if (sent)
        {
            bool ok = waitForResponse(CMD_SET_MODE_REQ | 0x80);
            if (ok)
            {
                rc = true;
            }
            else
            {
                warning("Warning! Did not get confirmation on set link mode for amb8465\n");
                rc = false;
            }
        }

        link_modes_ = lms;
        last_set_link_mode_ = request_[3];
    }

    getDeviceUniqueId();

    return rc;
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
        int rssi_len = (rssi_expected_ && data[1] == (CMD_DATA_IND)) ? 1 : 0;

        // A command response begins with 0xff
        *msgid_out = data[1];
        payload_len = data[2];
        *payload_len_out = payload_len;
        *payload_offset = 3;
        // FF CMD len payload [RSSI] CS (Note, RSSI already included in the payload_len if present.)
        *frame_length = 4 + payload_len;
        if (data.size() < *frame_length)
        {
            debug("(amb8465) not enough bytes yet, partial command response %d %d.\n", data.size(), *frame_length);
            return PartialFrame;
        }

        debug("(amb8465) received full command frame\n");

        uchar cs = xorChecksum(data, 0, *frame_length-1);
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
    // And C must be a valid wmbus c field.
    while ((payload_len = data[offset]) < 10 || !isValidWMBusCField(data[offset+1]))
    {
        offset++;
        if (offset + 2 >= data.size())
        {
            // No sensible telegram in the buffer. Flush it!
            // But not the last char, because the next char could be a valid c field.
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

    LOCK_WMBUS_RECEIVING_BUFFER(processSerialData);

    if (read_buffer_.size() > 0 && timerisset(&timestamp_last_rx_))
    {
        struct timeval chunk_time;
        timersub(&timestamp, &timestamp_last_rx_, &chunk_time);

        if (chunk_time.tv_sec >= 2)
        {
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
    case (0): // Transparent telegram mode (no 0xff header)
    case(CMD_DATA_IND): // Command telegram mode (0xff03 prefix)
    {
        AboutTelegram about("amb8465["+cached_device_id_+"]", rssi_dbm, LinkMode::UNKNOWN, FrameType::WMBUS);
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
    case (0x80|CMD_DATA_REQ):
    {
        verbose("(amb8465) send telegram completed\n");
        response_.clear();
        response_.insert(response_.end(), frame.begin(), frame.end());
        debugPayload("(amb8465) send telegram response", response_);
        notifyResponseIsHere(0x80|CMD_DATA_REQ);
        break;
    }
    default:
        verbose("(amb8465) unhandled device message %d\n", msgid);
        response_.clear();
        response_.insert(response_.end(), frame.begin(), frame.end());
        debugPayload("(amb8465) unknown response", response_);
    }
}

bool WMBusAmber::sendTelegram(LinkMode lm, TelegramFormat format, vector<uchar> &content)
{
    if (serial()->readonly()) return true; // Feeding from stdin or file.
    if (content.size() > 250) return false;

    LOCK_WMBUS_EXECUTING_COMMAND(sendTelegram);

    bool rc = false;

    if (serial()->readonly()) return true; // Feeding from stdin or file.

    uchar link_mode = setupAmberBusDeviceToSendTelegram(lm);

    if (link_mode == 0xff)
    {
        error("(amb8465) setting link mode %s for sending is not supported for amb8465 \n", toString(lm));
    }

    {
        // Empty the read buffer we do not want any partial data lying around
        // because we expect a response to arrive.
        LOCK_WMBUS_RECEIVING_BUFFER(deviceSetLinkMode_ClearBuffer);
        read_buffer_.clear();
    }

    if (link_mode != last_set_link_mode_)
    {
        // Switch to the send link mode.
        request_.resize(5);
        request_[0] = AMBER_SERIAL_SOF;
        request_[1] = CMD_SET_MODE_REQ;
        request_[2] = 1; // Len
        request_[3] = link_mode;
        request_[4] = xorChecksum(request_, 0, 4);

        verbose("(amb8465) set link mode %02x for sending\n", request_[3]);
        bool sent = serial()->send(request_);

        if (sent)
        {
            bool ok = waitForResponse(CMD_SET_MODE_REQ | 0x80);
            if (ok)
            {
                rc = true;
            }
            else
            {
                warning("Warning! Did not get confirmation on set link mode for amb8465 for sending\n");
                rc = false;
            }
        }
    }

    request_.resize(content.size()+4);
    request_[0] = AMBER_SERIAL_SOF;
    request_[1] = CMD_DATA_REQ;
    request_[2] = content.size();
    for (size_t i = 0; i < content.size(); ++i)
    {
        request_[3+i] = content[i];
    }
    request_[content.size()+3] = xorChecksum(request_, 0, request_.size());

    verbose("(amb8465) send %zu bytes of data\n", request_.size());
    bool sent = serial()->send(request_);

    if (sent)
    {
        bool ok = waitForResponse(CMD_DATA_REQ | 0x80);
        if (ok)
        {
            rc = true;
        }
        else
        {
            warning("Warning! Did not get confirmation on send data for amb8465\n");
            rc = false;
        }
    }

    if (link_mode != last_set_link_mode_)
    {
        // Restore the link mode.
        request_.resize(5);
        request_[0] = AMBER_SERIAL_SOF;
        request_[1] = CMD_SET_MODE_REQ;
        request_[2] = 1; // Len
        request_[3] = last_set_link_mode_;
        request_[4] = xorChecksum(request_, 0, 4);

        verbose("(amb8465) set link mode %02x for restore after sending\n", request_[3]);
        bool sent = serial()->send(request_);

        if (sent)
        {
            bool ok = waitForResponse(CMD_SET_MODE_REQ | 0x80);
            if (ok)
            {
                rc = true;
            }
            else
            {
                warning("Warning! Did not get confirmation on set link mode for amb8465 for restore after sending\n");
                rc = false;
            }
        }
    }

    return rc;
}

AccessCheck detectAMB8465AMB3665(Detected *detected, shared_ptr<SerialCommunicationManager> manager)
{
    assert(detected->found_file != "");

    // Talk to the device and expect a very specific answer.
    auto serial = manager->createSerialDeviceTTY(detected->found_file.c_str(), 9600, PARITY::NONE, "detect amb8465/amb3665");
    serial->disableCallbacks();
    bool ok = serial->open(false);
    if (!ok)
    {
        verbose("(amb8465/3665) could not open tty %s for detection\n", detected->found_file.c_str());
        return AccessCheck::NoSuchDevice;
    }

    vector<uchar> response;
    int count = 1;
    // First clear out any data in the queue, this might require multiple reads.
    for (;;)
    {
        size_t n = serial->receive(&response);
        count++;
        if (n == 0) break;
        if (count > 10)
        {
            break;
        }
        usleep(1000*100);
        continue;
    }

    if (response.size() > 0)
    {
        if (count <= 10)
        {
            debug("(amb8465/3665) cleared %zu bytes from serial buffer\n", response.size());
        }
        else
        {
            debug("(amb8465/3665) way too much data received %zu when trying to detect! cannot clear serial buffer!\n",
                  response.size());
        }

        response.clear();
    }

    // Query all of the non-volatile parameter memory.
    vector<uchar> request;
    request.resize(6);
    request[0] = AMBER_SERIAL_SOF;
    request[1] = CMD_GET_REQ;
    request[2] = 0x02;
    request[3] = 0x00; // Start at byte 0
    request[4] = 0x80; // End at byte 127
    request[5] = xorChecksum(request, 0, 5);

    assert(request[5] == 0x77);

    bool sent = false;
    count = 0;
    do
    {
        debug("(amb8465/3665) sending %zu bytes attempt %d\n", request.size(), count);
        sent = serial->send(request);
        debug("(amb8465/3665) sent %zu bytes %s\n", request.size(), sent?"OK":"Failed");
        if (!sent)
        {
            // We failed to send! Why? We have successfully opened the tty....
            // Perhaps the dongle needs to wake up. Lets try again in 100 ms.
            usleep(1000*100);
            count ++;
            if (count >= 4)
            {
                // Tried and failed 3 times.
                debug("(amb8465/3665) failed to sent query! Giving up!\n");
                verbose("(amb8465/3665) are you there? no, nothing is there.\n");
                serial->close();
                return AccessCheck::NoProperResponse;
            }
        }
    } while (sent == false && count < 4);

    // Wait for 100ms so that the USB stick have time to prepare a response.
    usleep(1000*100);

    ConfigAMB8465AMB3665 config;
    vector<uchar> data;
    bool ok_8465 {};
    bool ok_3665 {};

    count = 1;
    for (;;)
    {
        if (count > 3)
        {
            verbose("(amb8465/3665) are you there? no.\n");
            serial->close();
            return AccessCheck::NoProperResponse;
        }
        debug("(amb8465/3665) reading response... %d\n", count);

        size_t n = serial->receive(&data);
        count++;
        if (n == 0)
        {
            usleep(1000*100);
            continue;
        }
        response.insert(response.end(), data.begin(), data.end());
        size_t offset_8465;
        size_t offset_3665;
        bool got_8465 = findBytes(response, 0xff, 0x8A, 0x7A, &offset_8465);
        bool got_3665 = findBytes(response, 0xff, 0x8A, 0x82, &offset_3665);

        if (!got_8465 && !got_3665)
        {
            // No response found yet, lets wait for more bytes.
            usleep(1000*100);
            continue;
        }

        // We have the start of the response, but do we have enough bytes?
        if (got_8465)
        {
            ok_8465 = config.decode8465(response, offset_8465);
            // Yes!
            if (ok_8465) {
                debug("(amb8465) found response at offset %zu\n", offset_8465);
                break;
            }
        }
        if (got_3665)
        {
            // We have the start of the response, but do we have enough bytes?
            ok_3665 = config.decode3665(response, offset_3665);
            // Yes!
            if (ok_3665) {
                debug("(amb3665) found response at offset %zu\n", offset_3665);
                break;
            }
        }
        // No complete response found yet, lets wait for more bytes.
        usleep(1000*100);
    }

    serial->close();

    if (ok_8465)
    {
        // FF8A7A00780080710200000000FFFFFA00FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF003200021400FFFFFFFFFF010004000000FFFFFF01440000000000000000FFFF0B040100FFFFFFFFFF00030000FFFFFFFFFFFFFF0000FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF17

        detected->setAsFound(config.dongleId(), BusDeviceType::DEVICE_AMB8465, 9600, false,
                             detected->specified_device.linkmodes);

        verbose("(amb8465) detect %s\n", config.str().c_str());
        verbose("(amb8465) are you there? yes %s\n", config.dongleId().c_str());
    }

    if (ok_3665)
    {
        // FF8A8200800080710200000000FFFFFA00FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF0C3200021400FFFFFFFFFF010004000000FFFFFF01440000000000000000FFFF0B060100FFFFFFFFFF00020000FFFFFFFFFFFFFF0000FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF18

        detected->setAsFound(config.dongleId(), BusDeviceType::DEVICE_AMB3665, 9600, false,
                             detected->specified_device.linkmodes);

        verbose("(amb3665) detect %s\n", config.str().c_str());
        verbose("(amb3665) are you there? yes %s\n", config.dongleId().c_str());
    }

    return AccessCheck::AccessOK;
}

static AccessCheck tryFactoryResetAMB8465(string device, shared_ptr<SerialCommunicationManager> manager, int baud)
{
    // Talk to the device and expect a very specific answer.
    auto serial = manager->createSerialDeviceTTY(device.c_str(), baud, PARITY::NONE, "reset amb8465");
    bool ok = serial->open(false);
    if (!ok)
    {
        verbose("(amb8465) could not open device %s using baud %d for reset\n", device.c_str(), baud);
        return AccessCheck::NoSuchDevice;
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
    request_[3] = xorChecksum(request_, 0, 3);

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
        data[4] != xorChecksum(data, 0, 4))
    {
        verbose("(amb8465) no response to factory reset %s using baud %d\n", device.c_str(), baud);
        return AccessCheck::NoProperResponse;
    }
    verbose("(amb8465) received proper factory reset response %s using baud %d\n", device.c_str(), baud);
    return AccessCheck::AccessOK;
}

int bauds[] = { 1200, 2400, 4800, 9600, 19200, 38400, 56000, 115200, 0 };

AccessCheck factoryResetAMB8465(string device, shared_ptr<SerialCommunicationManager> manager, int *was_baud)
{
    AccessCheck rc = AccessCheck::NoSuchDevice;

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
    return AccessCheck::NoSuchDevice;
}
