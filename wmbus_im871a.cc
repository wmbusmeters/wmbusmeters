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

#include"wmbus.h"
#include"wmbus_im871a.h"
#include"serial.h"

#include<assert.h>
#include<pthread.h>
#include<semaphore.h>

using namespace std;

enum FrameStatus { PartialFrame, FullFrame, ErrorInFrame };

struct WMBusIM871A : public WMBus {
    bool ping();
    uint32_t getDeviceId();
    LinkMode getLinkMode();
    void setLinkMode(LinkMode lm);    
    void onTelegram(function<void(Telegram*)> cb);

    void processMessage();
    SerialDevice *serial() { return serial_; }

    WMBusIM871A(SerialDevice *serial, SerialCommunicationManager *manager);
private:
    SerialDevice *serial_;
    SerialCommunicationManager *manager_;
    vector<uchar> read_buffer_;
    pthread_mutex_t command_lock_ = PTHREAD_MUTEX_INITIALIZER;
    sem_t command_wait_;
    int sent_command_;
    int received_command_;
    vector<uchar> received_payload_;
    vector<function<void(Telegram*)>> telegram_listeners_;
    
    void waitForResponse();
    FrameStatus checkFrame(vector<uchar> &data,
                           size_t *frame_length, int *endpoint_out, int *msgid_out, int *payload_len_out);
    void handleDevMgmt(int msgid, vector<uchar> &payload);
    void handleRadioLink(int msgid, vector<uchar> &payload);
    void handleRadioLinkTest(int msgid, vector<uchar> &payload);
    void handleHWTest(int msgid, vector<uchar> &payload);
};

WMBus *openIM871A(string device, SerialCommunicationManager *manager)
{
    SerialDevice *serial = manager->createSerialDeviceTTY(device.c_str(), 57600);
    WMBusIM871A *imp = new WMBusIM871A(serial, manager);
    return imp;
}

WMBusIM871A::WMBusIM871A(SerialDevice *serial, SerialCommunicationManager *manager) :
    serial_(serial), manager_(manager)
{
    sem_init(&command_wait_, 0, 0);
    manager_->listenTo(serial_,call(this,processMessage));
    serial_->open();
    
}

bool WMBusIM871A::ping() {
    pthread_mutex_lock(&command_lock_);
    
    vector<uchar> msg(4);
    msg[0] = WMBUS_SERIAL_SOF;
    msg[1] = DEVMGMT_ID;
    msg[2] = DEVMGMT_MSG_PING_REQ;
    msg[3] = 0;

    sent_command_ = DEVMGMT_MSG_PING_REQ;
    serial()->send(msg);

    waitForResponse();
    
    pthread_mutex_unlock(&command_lock_);    
    return true;
}

uint32_t WMBusIM871A::getDeviceId() {
    pthread_mutex_lock(&command_lock_);
    
    vector<uchar> msg(4);
    msg[0] = WMBUS_SERIAL_SOF;
    msg[1] = DEVMGMT_ID;
    msg[2] = DEVMGMT_MSG_GET_DEVICEINFO_REQ;
    msg[3] = 0;

    sent_command_ = DEVMGMT_MSG_GET_DEVICEINFO_REQ;
    serial()->send(msg);

    waitForResponse();

    uint32_t id = 0;
    if (received_command_ == DEVMGMT_MSG_GET_DEVICEINFO_RSP) {
        verbose("Module Type %02x\n", received_payload_[0]);
        verbose( "Device Mode %02x\n", received_payload_[1]);
        verbose( "Firmware version %02x\n", received_payload_[2]);
        verbose( "HCI Protocol version %02x\n", received_payload_[3]);
        id = received_payload_[4] << 24 |
            received_payload_[5] << 16 |
            received_payload_[6] << 8 |
            received_payload_[7];
        verbose( "Device id %08x\n", id);
    }
    
    pthread_mutex_unlock(&command_lock_);    
    return id;
}

LinkMode WMBusIM871A::getLinkMode() {
    pthread_mutex_lock(&command_lock_);
    
    vector<uchar> msg(4);
    msg[0] = WMBUS_SERIAL_SOF;
    msg[1] = DEVMGMT_ID;
    sent_command_ = msg[2] = DEVMGMT_MSG_GET_CONFIG_REQ;
    msg[3] = 0;

    serial()->send(msg);

    waitForResponse();
    LinkMode lm = UNKNOWN_LINKMODE;
    if (received_command_ == DEVMGMT_MSG_GET_CONFIG_RSP) {
        verbose( "Received config size %zu\n", received_payload_.size());
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
        if (has_device_mode) {
            verbose( "Device mode %02x\n", received_payload_[offset]);
            offset++;
        }
        if (has_link_mode) {
            verbose( "Link mode %02x\n", received_payload_[offset]);
            lm = (LinkMode)(int)received_payload_[offset];
            offset++;
        }
        if (has_wmbus_c_field) {
            verbose( "WMBus C-field %02x\n", received_payload_[offset]);
            offset++;
        }
        if (has_wmbus_man_id) {
            verbose( "WMBus Manufacture id %02x%02x\n", received_payload_[offset+1], received_payload_[offset+0]);
            offset+=2;
        }
        if (has_wmbus_device_id) {
            verbose( "WMBus Device id %02x%02x%02x%02x\n", received_payload_[offset+3], received_payload_[offset+2],
                    received_payload_[offset+1], received_payload_[offset+0]);
            offset+=4;
        }
        if (has_wmbus_version) {
            verbose( "WMBus Version %02x\n", received_payload_[offset]);
            offset++;
        }
        if (has_wmbus_device_type) {
            verbose( "WMBus Device Type %02x\n", received_payload_[offset]);
            offset++;
        }
        if (has_radio_channel) {
            verbose( "Radio channel %02x\n", received_payload_[offset]);
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
            verbose( "Radio power level %02x\n", received_payload_[offset]);
            offset++;
        }
        if (has_radio_data_rate) {
            verbose( "Radio data rate %02x\n", received_payload_[offset]);
            offset++;
        }
        if (has_radio_rx_window) {
            verbose( "Radio rx window %02x\n", received_payload_[offset]);
            offset++;
        }
        if (has_auto_power_saving) {
            verbose( "Auto power saving %02x\n", received_payload_[offset]);
            offset++;
        }
        if (has_auto_rssi_attachment) {
            verbose( "Auto RSSI attachment %02x\n", received_payload_[offset]);
            offset++;
        }
        if (has_auto_rx_timestamp_attachment) {
            verbose( "Auto rx timestamp attachment %02x\n", received_payload_[offset]);
            offset++;
        }
        if (has_led_control) {
            verbose( "LED control %02x\n", received_payload_[offset]);
            offset++;
        }
        if (has_rtc_control) {
            verbose( "RTC control %02x\n", received_payload_[offset]);
            offset++;
        }
    }
    
    pthread_mutex_unlock(&command_lock_);    
    return lm;
}

void WMBusIM871A::setLinkMode(LinkMode lm) {
    pthread_mutex_lock(&command_lock_);
    
    vector<uchar> msg(8);
    msg[0] = WMBUS_SERIAL_SOF;
    msg[1] = DEVMGMT_ID;
    sent_command_ = msg[2] = DEVMGMT_MSG_SET_CONFIG_REQ;
    msg[3] = 3; // Len
    msg[4] = 0; // Temporary
    msg[5] = 2; // iff1 bits: Set Radio Mode only
    msg[6] = (int)lm; // C1a
    msg[7] = 0; // iff2 bits: Set nothing

    verbose( "SET CONFIG REQ to USB im871a\n\n");
    serial()->send(msg);

    waitForResponse();
    pthread_mutex_unlock(&command_lock_);    
}

void WMBusIM871A::onTelegram(function<void(Telegram*)> cb) {
    telegram_listeners_.push_back(cb);
}

void WMBusIM871A::waitForResponse() {
    while (manager_->isRunning()) {
        int rc = sem_wait(&command_wait_);
        if (rc==0) break;
        if (rc==-1) {
            if (errno==EINTR) continue;
            break;
        }
    }
}

FrameStatus WMBusIM871A::checkFrame(vector<uchar> &data,
                                    size_t *frame_length, int *endpoint_out, int *msgid_out, int *payload_len_out)
{
    if (data.size() == 0) return PartialFrame;
    if (data[0] != 0xa5) return ErrorInFrame;
    int ctrlbits = (data[1] & 0xf0) >> 4;
    if (ctrlbits & 1) return ErrorInFrame; // Bit 1 is reserved, we do not expect it....
    bool has_timestamp = ((ctrlbits&2)==2);
    bool has_rssi = ((ctrlbits&4)==4);
    bool has_crc16 = ((ctrlbits&8)==8);    
    int endpoint = data[1] & 0x0f;
    if (endpoint != DEVMGMT_ID &&
        endpoint != RADIOLINK_ID &&
        endpoint != RADIOLINKTEST_ID &&
        endpoint != HWTEST_ID) return ErrorInFrame;
    *endpoint_out = endpoint;
    
    int msgid = data[2];
    if (endpoint == DEVMGMT_ID && (msgid<1 || msgid>0x27)) return ErrorInFrame;
    if (endpoint == RADIOLINK_ID && (msgid<1 || msgid>0x05)) return ErrorInFrame;
    if (endpoint == RADIOLINKTEST_ID && (msgid<1 || msgid>0x07)) return ErrorInFrame; // Are 5 and 6 disallowed?
    if (endpoint == HWTEST_ID && (msgid<1 || msgid>0x02)) return ErrorInFrame;
    *msgid_out = msgid;
    
    int payload_len = data[3];
    *payload_len_out = payload_len;
    
    *frame_length = 4+payload_len+(has_timestamp?4:0)+(has_rssi?1:0)+(has_crc16?2:0);
    if (data.size() < *frame_length) return PartialFrame;
    
    return FullFrame;
}

void WMBusIM871A::processMessage() {

    vector<uchar> data;
    serial_->receive(&data);
    read_buffer_.insert(read_buffer_.end(), data.begin(), data.end());

    size_t frame_length;
    int endpoint;
    int msgid;
    int payload_len;

    FrameStatus status = checkFrame(read_buffer_, &frame_length, &endpoint, &msgid, &payload_len);
    
    if (status == ErrorInFrame) {
        verbose( "Protocol error in message received from iM871A!\n");
        read_buffer_.clear();
    } else
    if (status == FullFrame) {

        vector<uchar> payload;
        if (payload_len > 0) {
            payload.insert(payload.begin(), read_buffer_.begin()+4, read_buffer_.begin()+payload_len);
        }

        read_buffer_.erase(read_buffer_.begin(), read_buffer_.begin()+frame_length);

        switch (endpoint) {
        case DEVMGMT_ID: handleDevMgmt(msgid, payload); break;
        case RADIOLINK_ID: handleRadioLink(msgid, payload); break;
        case RADIOLINKTEST_ID: handleRadioLinkTest(msgid, payload); break;
        case HWTEST_ID: handleHWTest(msgid, payload); break;
        }
    }
}

void WMBusIM871A::handleDevMgmt(int msgid, vector<uchar> &payload)
{
    switch (msgid) {
        case DEVMGMT_MSG_PING_RSP: // 0x02
            verbose("Received ping response.\n");
            received_command_ = msgid;
            sem_post(&command_wait_);
            break;
        case DEVMGMT_MSG_SET_CONFIG_RSP: // 0x04
            verbose("Received set device config response.\n");
            received_command_ = msgid;
            received_payload_.clear();
            received_payload_.insert(received_payload_.end(), payload.begin(), payload.end());
            sem_post(&command_wait_);
            break;
        case DEVMGMT_MSG_GET_CONFIG_RSP: // 0x06
            verbose("Received get device config response.\n");
            received_command_ = msgid;
            received_payload_.clear();
            received_payload_.insert(received_payload_.end(), payload.begin(), payload.end());
            sem_post(&command_wait_);
            break;
        case DEVMGMT_MSG_GET_DEVICEINFO_RSP: // 0x10
            verbose("Received device info response.\n");
            received_command_ = msgid;
            received_payload_.clear();
            received_payload_.insert(received_payload_.end(), payload.begin(), payload.end());
            sem_post(&command_wait_);
            break;
    default:
        verbose("Unhandled device management message %d\n", msgid);
    }            
}

void WMBusIM871A::handleRadioLink(int msgid, vector<uchar> &payload)
{
    switch (msgid) {
        case RADIOLINK_MSG_WMBUSMSG_IND: // 0x03
            {
                Telegram t;
                verbose("Radiolink received (%zu bytes): ", payload.size());
                for (auto c : payload) verbose("%02x ", c);
                verbose("\n");
                t.c_field = payload[0];
                t.m_field = payload[2]<<8 | payload[1];
                t.a_field.resize(6);
                t.a_field_address.resize(4);
                for (int i=0; i<6; ++i) {
                    t.a_field[i] = payload[3+i];
                    if (i<4) { t.a_field_address[i] = payload[3+3-i]; }
                }
                t.a_field_version = payload[3+4];
                t.a_field_device_type=payload[3+5];
                t.ci_field=payload[9];
                t.payload.clear();
                t.payload.insert(t.payload.end(), payload.begin()+10, payload.end());
		verbose("C field: %02x\n", t.c_field);
		verbose("M field: %04x\n", t.m_field);
                verbose("A field version: %02x\n", t.a_field_version);
		verbose("A field dev type: %02x\n", t.a_field_device_type);
		verbose("Ci field: %02x\n", t.ci_field);

                for (auto f : telegram_listeners_) {
                    if (f) f(&t);
                }
            }
            break;
    default:
        verbose("Unhandled radio link message %d\n", msgid);
    }            
}

void WMBusIM871A::handleRadioLinkTest(int msgid, vector<uchar> &payload)
{
    switch (msgid) {
    default:
        verbose("Unhandled radio link test message %d\n", msgid);
    }            
}

void WMBusIM871A::handleHWTest(int msgid, vector<uchar> &payload)
{
    switch (msgid) {
    default:
        verbose("Unhandled hw test message %d\n", msgid);
    }            
}


