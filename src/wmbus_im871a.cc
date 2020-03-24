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

#include"wmbus.h"
#include"wmbus_utils.h"
#include"wmbus_im871a.h"
#include"serial.h"

#include<assert.h>
#include<pthread.h>
#include<sys/errno.h>
#include<semaphore.h>
#include<unistd.h>

using namespace std;

struct WMBusIM871A : public virtual WMBusCommonImplementation
{
    bool ping();
    uint32_t getDeviceId();
    LinkModeSet getLinkModes();
    void setLinkModes(LinkModeSet lms);
    LinkModeSet supportedLinkModes() {
        return
            C1_bit |
            S1_bit |
            S1m_bit |
            T1_bit |
            N1a_bit |
            N1b_bit |
            N1c_bit |
            N1d_bit |
            N1e_bit |
            N1f_bit;
    }
    int numConcurrentLinkModes() { return 1; }
    bool canSetLinkModes(LinkModeSet lms)
    {
        if (0 == countSetBits(lms.bits())) return false;
        if (!supportedLinkModes().supports(lms)) return false;
        // Ok, the supplied link modes are compatible,
        // but im871a can only listen to one at a time.
        return 1 == countSetBits(lms.bits());
    }
    void processSerialData();
    SerialDevice *serial() { return serial_.get(); }
    void simulate() { }

    WMBusIM871A(unique_ptr<SerialDevice> serial, SerialCommunicationManager *manager);
    ~WMBusIM871A() { }

private:
    unique_ptr<SerialDevice> serial_;
    SerialCommunicationManager *manager_;
    vector<uchar> read_buffer_;
    pthread_mutex_t command_lock_ = PTHREAD_MUTEX_INITIALIZER;
    sem_t command_wait_;
    int sent_command_ {};
    int received_command_ {};
    vector<uchar> received_payload_;
    LinkModeSet link_modes_ {};

    void waitForResponse();
    static FrameStatus checkIM871AFrame(vector<uchar> &data,
                                        size_t *frame_length, int *endpoint_out, int *msgid_out,
                                        int *payload_len_out, int *payload_offset);
    friend bool detectIM871A(string device, SerialCommunicationManager *manager);
    void handleDevMgmt(int msgid, vector<uchar> &payload);
    void handleRadioLink(int msgid, vector<uchar> &payload);
    void handleRadioLinkTest(int msgid, vector<uchar> &payload);
    void handleHWTest(int msgid, vector<uchar> &payload);
};

unique_ptr<WMBus> openIM871A(string device, SerialCommunicationManager *manager, unique_ptr<SerialDevice> serial_override)
{
    if (serial_override)
    {
        WMBusIM871A *imp = new WMBusIM871A(std::move(serial_override), manager);
        return unique_ptr<WMBus>(imp);
    }

    auto serial = manager->createSerialDeviceTTY(device.c_str(), 57600);
    WMBusIM871A *imp = new WMBusIM871A(std::move(serial), manager);
    return unique_ptr<WMBus>(imp);
}

WMBusIM871A::WMBusIM871A(unique_ptr<SerialDevice> serial, SerialCommunicationManager *manager) :
    WMBusCommonImplementation(DEVICE_IM871A), serial_(std::move(serial)), manager_(manager)
{
    sem_init(&command_wait_, 0, 0);
    manager_->listenTo(serial_.get(),call(this,processSerialData));
    serial_->open(true);
}

bool WMBusIM871A::ping()
{
    if (serial_->readonly()) return true; // Feeding from stdin or file.

    pthread_mutex_lock(&command_lock_);

    vector<uchar> msg(4);
    msg[0] = IM871A_SERIAL_SOF;
    msg[1] = DEVMGMT_ID;
    msg[2] = DEVMGMT_MSG_PING_REQ;
    msg[3] = 0;

    sent_command_ = DEVMGMT_MSG_PING_REQ;
    verbose("(im871a) ping\n");
    bool sent = serial()->send(msg);

    if (sent) waitForResponse();

    pthread_mutex_unlock(&command_lock_);
    return true;
}

uint32_t WMBusIM871A::getDeviceId()
{
    if (serial_->readonly()) return 0; // Feeding from stdin or file.

    pthread_mutex_lock(&command_lock_);

    vector<uchar> msg(4);
    msg[0] = IM871A_SERIAL_SOF;
    msg[1] = DEVMGMT_ID;
    msg[2] = DEVMGMT_MSG_GET_DEVICEINFO_REQ;
    msg[3] = 0;

    sent_command_ = DEVMGMT_MSG_GET_DEVICEINFO_REQ;
    verbose("(im871a) get device info\n");
    bool sent = serial()->send(msg);

    uint32_t id = 0;

    if (sent)
    {
        waitForResponse();

        if (received_command_ == DEVMGMT_MSG_GET_DEVICEINFO_RSP) {
            verbose("(im871a) device info: module Type %02x\n", received_payload_[0]);
            verbose("(im871a) device info: device Mode %02x\n", received_payload_[1]);
            verbose("(im871a) device info: firmware version %02x\n", received_payload_[2]);
            verbose("(im871a) device info: hci protocol version %02x\n", received_payload_[3]);
            id = received_payload_[4] << 24 |
                received_payload_[5] << 16 |
                received_payload_[6] << 8 |
                received_payload_[7];
            verbose("(im871a) devince info: id %08x\n", id);
        }
    }
    else
    {
        id = 0;
    }

    pthread_mutex_unlock(&command_lock_);
    return id;
}

LinkModeSet WMBusIM871A::getLinkModes()
{
    if (serial_->readonly()) { return Any_bit; }  // Feeding from stdin or file.

    pthread_mutex_lock(&command_lock_);

    vector<uchar> msg(4);
    msg[0] = IM871A_SERIAL_SOF;
    msg[1] = DEVMGMT_ID;
    sent_command_ = msg[2] = DEVMGMT_MSG_GET_CONFIG_REQ;
    msg[3] = 0;

    verbose("(im871a) get config\n");
    bool sent = serial()->send(msg);

    if (!sent)
    {
        pthread_mutex_unlock(&command_lock_);
        // Use the remembered link modes set before.
        return link_modes_;
    }

    waitForResponse();
    LinkMode lm = LinkMode::UNKNOWN;
    if (received_command_ == DEVMGMT_MSG_GET_CONFIG_RSP)
    {
        int iff1 = received_payload_[0];
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
            verbose("(im871a) config: device mode %02x\n", received_payload_[offset]);
            offset++;
        }
        if (has_link_mode)
        {
            verbose("(im871a) config: link mode %02x\n", received_payload_[offset]);
            if (received_payload_[offset] == (int)LinkModeIM871A::C1a) {
                lm = LinkMode::C1;
            }
            if (received_payload_[offset] == (int)LinkModeIM871A::S1) {
                lm = LinkMode::S1;
            }
            if (received_payload_[offset] == (int)LinkModeIM871A::S1m) {
                lm = LinkMode::S1m;
            }
            if (received_payload_[offset] == (int)LinkModeIM871A::T1) {
                lm = LinkMode::T1;
            }
            if (received_payload_[offset] == (int)LinkModeIM871A::N1A) {
                lm = LinkMode::N1a;
            }
            if (received_payload_[offset] == (int)LinkModeIM871A::N1B) {
                lm = LinkMode::N1b;
            }
            if (received_payload_[offset] == (int)LinkModeIM871A::N1C) {
                lm = LinkMode::N1c;
            }
            if (received_payload_[offset] == (int)LinkModeIM871A::N1D) {
                lm = LinkMode::N1d;
            }
            if (received_payload_[offset] == (int)LinkModeIM871A::N1E) {
                lm = LinkMode::N1e;
            }
            if (received_payload_[offset] == (int)LinkModeIM871A::N1F) {
                lm = LinkMode::N1f;
            }
            offset++;
        }
        if (has_wmbus_c_field) {
            verbose("(im871a) config: wmbus c-field %02x\n", received_payload_[offset]);
            offset++;
        }
        if (has_wmbus_man_id) {
            int flagid = 256*received_payload_[offset+1] +received_payload_[offset+0];
            string flag = manufacturerFlag(flagid);
            verbose("(im871a) config: wmbus mfg id %02x%02x (%s)\n", received_payload_[offset+1], received_payload_[offset+0],
                    flag.c_str());
            offset+=2;
        }
        if (has_wmbus_device_id) {
            verbose("(im871a) config: wmbus device id %02x%02x%02x%02x\n", received_payload_[offset+3], received_payload_[offset+2],
                    received_payload_[offset+1], received_payload_[offset+0]);
            offset+=4;
        }
        if (has_wmbus_version) {
            verbose("(im871a) config: wmbus version %02x\n", received_payload_[offset]);
            offset++;
        }
        if (has_wmbus_device_type) {
            verbose("(im871a) config: wmbus device type %02x\n", received_payload_[offset]);
            offset++;
        }
        if (has_radio_channel) {
            verbose("(im871a) config: radio channel %02x\n", received_payload_[offset]);
            offset++;
        }
        int iff2 = received_payload_[offset];
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
            verbose("(im871a) config: radio power level %02x\n", received_payload_[offset]);
            offset++;
        }
        if (has_radio_data_rate) {
            verbose("(im871a) config: radio data rate %02x\n", received_payload_[offset]);
            offset++;
        }
        if (has_radio_rx_window) {
            verbose("(im871a) config: radio rx window %02x\n", received_payload_[offset]);
            offset++;
        }
        if (has_auto_power_saving) {
            verbose("(im871a) config: auto power saving %02x\n", received_payload_[offset]);
            offset++;
        }
        if (has_auto_rssi_attachment) {
            verbose("(im871a) config: auto RSSI attachment %02x\n", received_payload_[offset]);
            offset++;
        }
        if (has_auto_rx_timestamp_attachment) {
            verbose("(im871a) config: auto rx timestamp attachment %02x\n", received_payload_[offset]);
            offset++;
        }
        if (has_led_control) {
            verbose("(im871a) config: led control %02x\n", received_payload_[offset]);
            offset++;
        }
        if (has_rtc_control) {
            verbose("(im871a) config: rtc control %02x\n", received_payload_[offset]);
            offset++;
        }
    }

    pthread_mutex_unlock(&command_lock_);
    LinkModeSet lms;
    lms.addLinkMode(lm);
    return lms;
}

void WMBusIM871A::setLinkModes(LinkModeSet lms)
{
    if (serial_->readonly()) return; // Feeding from stdin or file.

    if (!canSetLinkModes(lms))
    {
        string modes = lms.hr();
        error("(im871a) setting link mode(s) %s is not supported for im871a\n", modes.c_str());
    }

    pthread_mutex_lock(&command_lock_);

    vector<uchar> msg(10);
    msg[0] = IM871A_SERIAL_SOF;
    msg[1] = DEVMGMT_ID;
    sent_command_ = msg[2] = DEVMGMT_MSG_SET_CONFIG_REQ;
    msg[3] = 6; // Len
    msg[4] = 0; // Temporary
    msg[5] = 2; // iff1 bits: Set Radio Mode
    if (lms.has(LinkMode::C1)) {
        msg[6] = (int)LinkModeIM871A::C1a;
    } else if (lms.has(LinkMode::S1)) {
        msg[6] = (int)LinkModeIM871A::S1;
    } else if (lms.has(LinkMode::S1m)) {
        msg[6] = (int)LinkModeIM871A::S1m;
    } else if (lms.has(LinkMode::T1)) {
        msg[6] = (int)LinkModeIM871A::T1;
    } else if (lms.has(LinkMode::N1a)) {
        msg[6] = (int)LinkModeIM871A::N1A;
    } else if (lms.has(LinkMode::N1b)) {
        msg[6] = (int)LinkModeIM871A::N1B;
    } else if (lms.has(LinkMode::N1c)) {
        msg[6] = (int)LinkModeIM871A::N1C;
    } else if (lms.has(LinkMode::N1d)) {
        msg[6] = (int)LinkModeIM871A::N1D;
    } else if (lms.has(LinkMode::N1e)) {
        msg[6] = (int)LinkModeIM871A::N1E;
    } else if (lms.has(LinkMode::N1f)) {
        msg[6] = (int)LinkModeIM871A::N1F;
    } else {
        msg[6] = (int)LinkModeIM871A::C1a; // Defaults to C1a
    }

    msg[7] = 16+32; // iff2 bits: Set rssi+timestamp
    msg[8] = 1;  // Enable rssi
    msg[9] = 1;  // Enable timestamp

    verbose("(im871a) set link mode %02x\n", msg[6]);
    bool sent = serial()->send(msg);

    if (sent) waitForResponse();

    // Remember the link modes, necessary when using stdin or file.
    link_modes_ = lms;
    pthread_mutex_unlock(&command_lock_);
}

void WMBusIM871A::waitForResponse()
{
    while (manager_->isRunning())
    {
        int rc = sem_wait(&command_wait_);
        if (rc==0) break;
        if (rc==-1) {
            if (errno==EINTR) continue;
            break;
        }
    }
}

FrameStatus WMBusIM871A::checkIM871AFrame(vector<uchar> &data,
                                          size_t *frame_length, int *endpoint_out, int *msgid_out,
                                          int *payload_len_out, int *payload_offset)
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
        uint32_t rssi = data[i];
        verbose("(im871a) rssi %02x\n", rssi);
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

void WMBusIM871A::processSerialData()
{
    vector<uchar> data;

    // Receive and accumulated serial data until a full frame has been received.
    serial_->receive(&data);

    read_buffer_.insert(read_buffer_.end(), data.begin(), data.end());

    size_t frame_length;
    int endpoint;
    int msgid;
    int payload_len, payload_offset;

    for (;;)
    {
        FrameStatus status = checkIM871AFrame(read_buffer_, &frame_length, &endpoint, &msgid, &payload_len, &payload_offset);

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
            case RADIOLINK_ID: handleRadioLink(msgid, payload); break;
            case RADIOLINKTEST_ID: handleRadioLinkTest(msgid, payload); break;
            case HWTEST_ID: handleHWTest(msgid, payload); break;
            }
        }
    }
}

void WMBusIM871A::handleDevMgmt(int msgid, vector<uchar> &payload)
{
    switch (msgid) {
        case DEVMGMT_MSG_PING_RSP: // 0x02
            verbose("(im871a) pong\n");
            received_command_ = msgid;
            sem_post(&command_wait_);
            break;
        case DEVMGMT_MSG_SET_CONFIG_RSP: // 0x04
            verbose("(im871a) set config completed\n");
            received_command_ = msgid;
            received_payload_.clear();
            received_payload_.insert(received_payload_.end(), payload.begin(), payload.end());
            sem_post(&command_wait_);
            break;
        case DEVMGMT_MSG_GET_CONFIG_RSP: // 0x06
            verbose("(im871a) get config completed\n");
            received_command_ = msgid;
            received_payload_.clear();
            received_payload_.insert(received_payload_.end(), payload.begin(), payload.end());
            sem_post(&command_wait_);
            break;
        case DEVMGMT_MSG_GET_DEVICEINFO_RSP: // 0x10
            verbose("(im871a) device info completed\n");
            received_command_ = msgid;
            received_payload_.clear();
            received_payload_.insert(received_payload_.end(), payload.begin(), payload.end());
            sem_post(&command_wait_);
            break;
    default:
        verbose("(im871a) Unhandled device management message %d\n", msgid);
    }
}

void WMBusIM871A::handleRadioLink(int msgid, vector<uchar> &frame)
{
    switch (msgid) {
    case RADIOLINK_MSG_WMBUSMSG_IND: // 0x03
        handleTelegram(frame);
        break;
    default:
        verbose("(im871a) Unhandled radio link message %d\n", msgid);
    }
}

void WMBusIM871A::handleRadioLinkTest(int msgid, vector<uchar> &payload)
{
    switch (msgid) {
    default:
        verbose("(im871a) Unhandled radio link test message %d\n", msgid);
    }
}

void WMBusIM871A::handleHWTest(int msgid, vector<uchar> &payload)
{
    switch (msgid) {
    default:
        verbose("(im871a) Unhandled hw test message %d\n", msgid);
    }
}

bool detectIM871A(string device, SerialCommunicationManager *manager)
{
    // Talk to the device and expect a very specific answer.
    auto serial = manager->createSerialDeviceTTY(device.c_str(), 57600);
    bool ok = serial->open(false);
    if (!ok) return false;

    vector<uchar> data;
    // First clear out any data in the queue.
    serial->receive(&data);
    data.clear();

    vector<uchar> msg(4);
    msg[0] = IM871A_SERIAL_SOF;
    msg[1] = DEVMGMT_ID;
    msg[2] = DEVMGMT_MSG_PING_REQ;
    msg[3] = 0;

    verbose("(im871a) are you there?\n");
    serial->send(msg);
    // Wait for 100ms so that the USB stick have time to prepare a response.
    usleep(1000*100);
    serial->receive(&data);
    serial->close();

    string sent = bin2hex(msg);
    string recv = bin2hex(data);

    size_t frame_length;
    int endpoint, msgid, payload_len, payload_offset;
    FrameStatus status = WMBusIM871A::checkIM871AFrame(data,
                                                       &frame_length, &endpoint, &msgid,
                                                       &payload_len, &payload_offset);
    if (status != FullFrame ||
        endpoint != 1 ||
        msgid != 2)
    {
        return false;
    }
    return true;
}
