// Copyright (c) 2018 Fredrik Öhrström
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
#include"wmbus_amb8465.h"
#include"serial.h"

#include<assert.h>
#include<pthread.h>
#include<semaphore.h>
#include <unistd.h>

using namespace std;

enum FrameStatus { PartialFrame, FullFrame, ErrorInFrame };

struct WMBusAmber : public WMBus {
    bool ping();
    uint32_t getDeviceId();
    LinkMode getLinkMode();
    void setLinkMode(LinkMode lm);
    void onTelegram(function<void(Telegram*)> cb);

    void processSerialData();
    void getConfiguration();
    SerialDevice *serial() { return serial_; }
    void simulate() { }

    WMBusAmber(SerialDevice *serial, SerialCommunicationManager *manager);
private:
    SerialDevice *serial_;
    SerialCommunicationManager *manager_;
    vector<uchar> read_buffer_;
    pthread_mutex_t command_lock_ = PTHREAD_MUTEX_INITIALIZER;
    sem_t command_wait_;
    int sent_command_;
    int received_command_;
    LinkMode link_mode_ = UNKNOWN_LINKMODE;
    vector<uchar> received_payload_;
    vector<function<void(Telegram*)>> telegram_listeners_;

    void waitForResponse();
    FrameStatus checkAMB8465Frame(vector<uchar> &data,
                           size_t *frame_length, int *msgid_out, int *payload_len_out, int *payload_offset);
    void handleMessage(int msgid, vector<uchar> &frame);
};

WMBus *openAMB8465(string device, SerialCommunicationManager *manager)
{
    SerialDevice *serial = manager->createSerialDeviceTTY(device.c_str(), 9600);
    WMBusAmber *imp = new WMBusAmber(serial, manager);
    return imp;
}

WMBusAmber::WMBusAmber(SerialDevice *serial, SerialCommunicationManager *manager) :
    serial_(serial), manager_(manager)
{
    sem_init(&command_wait_, 0, 0);
    manager_->listenTo(serial_,call(this,processSerialData));
    serial_->open(true);

}

uchar xorChecksum(vector<uchar> msg, int len) {
    uchar c = 0;
    for (int i=0; i<len; ++i) {
        c ^= msg[i];
    }
    return c;
}

bool WMBusAmber::ping() {
    pthread_mutex_lock(&command_lock_);

    /*
    vector<uchar> msg(4);
    msg[0] = AMBER_SERIAL_SOF;
    msg[1] = DEVMGMT_ID;
    msg[2] = DEVMGMT_MSG_PING_REQ;
    msg[3] = 0;

    sent_command_ = DEVMGMT_MSG_PING_REQ;
    serial()->send(msg);

    waitForResponse();
    */
    pthread_mutex_unlock(&command_lock_);
    return true;
}

uint32_t WMBusAmber::getDeviceId() {
    pthread_mutex_lock(&command_lock_);

    vector<uchar> msg(4);
    msg[0] = AMBER_SERIAL_SOF;
    msg[1] = CMD_SERIALNO_REQ;
    msg[2] = 0; // No payload
    msg[3] = xorChecksum(msg, 3);

    assert(msg[3] == 0xf4);

    sent_command_ = CMD_SERIALNO_REQ;
    verbose("(amb8465) get device id\n");
    serial()->send(msg);

    waitForResponse();

    uint32_t id = 0;
    if (received_command_ == (CMD_SERIALNO_REQ | 0x80)) {
        id = received_payload_[4] << 24 |
            received_payload_[5] << 16 |
            received_payload_[6] << 8 |
            received_payload_[7];
        verbose("(amb8465) device id %08x\n", id);
    }

    pthread_mutex_unlock(&command_lock_);
    return id;
}

LinkMode WMBusAmber::getLinkMode() {
    // It is not possible to read the volatile mode set using setLinkMode below.
    // (It is possible to read the non-volatile settings, but this software
    // does not change those.) So we remember the state for the device.
    getConfiguration();
    return link_mode_;
}

void WMBusAmber::getConfiguration()
{
    pthread_mutex_lock(&command_lock_);

    vector<uchar> msg(6);
    msg[0] = AMBER_SERIAL_SOF;
    msg[1] = CMD_GET_REQ;
    msg[2] = 0x02;
    msg[3] = 0x00;
    msg[4] = 0x80;
    msg[5] = xorChecksum(msg, 5);

    assert(msg[5] == 0x77);

    verbose("(amb8465) get config\n");
    serial()->send(msg);

    waitForResponse();

    if (received_command_ == (0x80|CMD_GET_REQ))
    {
        // These are the non-volatile values stored inside the dongle.
        // However the link mode, radio channel etc might not be the one
        // that we are actually using! Since setting the link mode
        // is possible without changing the non-volatile memory.
        // But there seems to be no way of reading out the set link mode....???
        // Ie there is a disconnect between the flash and the actual running dongle.
        // Oh well.
        //
        // These are just some random config settings store in non-volatile memory.
        verbose("(amb8465) config: uart %02x\n", received_payload_[2]);
        verbose("(amb8465) config: radio Channel %02x\n", received_payload_[60+2]);
        verbose("(amb8465) config: rssi enabled %02x\n", received_payload_[69+2]);
        verbose("(amb8465) config: mode Preselect %02x\n", received_payload_[70+2]);
    }

    pthread_mutex_unlock(&command_lock_);
}

void WMBusAmber::setLinkMode(LinkMode lm) {
    if (lm != LinkModeC1 && lm != LinkModeT1) {
        error("LinkMode %d is not implemented\n", (int)lm);
    }

    pthread_mutex_lock(&command_lock_);

    vector<uchar> msg(8);
    msg[0] = AMBER_SERIAL_SOF;
    msg[1] = CMD_SET_MODE_REQ;
    sent_command_ = msg[1];
    msg[2] = 1; // Len
    if (lm == LinkModeC1) {
        msg[3] = 0x0E; // Reception of C1 and C2 messages
    } else {
        msg[3] = 0x05; // T1-Meter
    }
    msg[4] = xorChecksum(msg, 4);

    verbose("(amb8465) set link mode %02x\n", msg[3]);
    serial()->send(msg);

    waitForResponse();
    link_mode_ = lm;
    pthread_mutex_unlock(&command_lock_);
}

void WMBusAmber::onTelegram(function<void(Telegram*)> cb) {
    telegram_listeners_.push_back(cb);
}

void WMBusAmber::waitForResponse() {
    while (manager_->isRunning()) {
        int rc = sem_wait(&command_wait_);
        if (rc==0) break;
        if (rc==-1) {
            if (errno==EINTR) continue;
            break;
        }
    }
}

FrameStatus WMBusAmber::checkAMB8465Frame(vector<uchar> &data,
                                   size_t *frame_length, int *msgid_out, int *payload_len_out, int *payload_offset)
{
    if (data.size() == 0) return PartialFrame;
    int payload_len = 0;
    if (data[0] == 0xff) {
        // A command response begins with 0xff
        *msgid_out = data[1];
        payload_len = data[2];
        *payload_len_out = payload_len;
        *payload_offset = 3;
        *frame_length = 3+payload_len+1; // expect device to have checksumbyte at end enabled.
        // Check checksum here!
        if (data.size() < *frame_length) return PartialFrame;

        return FullFrame;
    }
    // If it is not a 0xff we assume it is a message beginning with a length.
    // There might be a different mode where the data is wrapped in 0xff. But for the moment
    // this is what I see.
    payload_len = data[0];
    *msgid_out = 0; // 0 is used to signal
    *payload_len_out = payload_len;
    *payload_offset = 1;
    *frame_length = payload_len+1;
    if (data.size() < *frame_length) return PartialFrame;

    return FullFrame;
}

void WMBusAmber::processSerialData()
{
    vector<uchar> data;

    // Receive and accumulated serial data until a full frame has been received.
    serial_->receive(&data);

    read_buffer_.insert(read_buffer_.end(), data.begin(), data.end());

    size_t frame_length;
    int msgid;
    int payload_len, payload_offset;

    FrameStatus status = checkAMB8465Frame(read_buffer_, &frame_length, &msgid, &payload_len, &payload_offset);

    if (status == ErrorInFrame) {
        verbose("(amb8465) protocol error in message received!\n");
        string msg = bin2hex(read_buffer_);
        debug("(amb8465) protocol error \"%s\"\n", msg.c_str());
        read_buffer_.clear();
    } else
    if (status == FullFrame) {

        vector<uchar> payload;
        if (payload_len > 0) {
            uchar l = payload_len;
            int minus = 0;
            payload.insert(payload.end(), &l, &l+1); // Re-insert the len byte.
            if (msgid == 0) {
                // Copy the telegram payload minus 4 bytes at the end. Could these extra bytes be some
                // AMB8465 crc/rssi/else specific data that is dependent on the non-volatile
                // bit settings in the usb stick? Perhaps.
                minus = 4;
            }
            payload.insert(payload.end(), read_buffer_.begin()+payload_offset, read_buffer_.begin()+payload_offset+payload_len-minus);
        }

        read_buffer_.erase(read_buffer_.begin(), read_buffer_.begin()+frame_length);

        handleMessage(msgid, payload);
    }
}

void WMBusAmber::handleMessage(int msgid, vector<uchar> &frame)
{
    switch (msgid) {
    case (0):
    {
        Telegram t;
        t.parse(frame);
        for (auto f : telegram_listeners_) {
            if (f) f(&t);
            if (isVerboseEnabled() && !t.handled) {
                verbose("(amb8465) telegram ignored by all configured meters!\n");
            }
        }
        break;
    }
    case (0x80|CMD_SET_MODE_REQ):
    {
        verbose("(amb8465) set link mode completed\n");
        received_command_ = msgid;
        received_payload_.clear();
        received_payload_.insert(received_payload_.end(), frame.begin(), frame.end());
        debugPayload("(amb8465) set link mode", received_payload_);
        sem_post(&command_wait_);
        break;
    }
    case (0x80|CMD_GET_REQ):
    {
        verbose("(amb8465) get config completed\n");
        received_command_ = msgid;
        received_payload_.clear();
        received_payload_.insert(received_payload_.end(), frame.begin(), frame.end());
        debugPayload("(amb8465) get config", received_payload_);
        sem_post(&command_wait_);
        break;
    }
    case (0x80|CMD_SERIALNO_REQ):
    {
        verbose("(amb8465) get device id completed\n");
        received_command_ = msgid;
        received_payload_.clear();
        received_payload_.insert(received_payload_.end(), frame.begin(), frame.end());
        debugPayload("(amb8465) get device id", received_payload_);
        sem_post(&command_wait_);
        break;
    }
    default:
        verbose("(amb8465) unhandled device message %d\n", msgid);
        received_payload_.clear();
        received_payload_.insert(received_payload_.end(), frame.begin(), frame.end());
        debugPayload("(amb8465) unknown", received_payload_);
    }
}

bool detectAMB8465(string device, SerialCommunicationManager *manager)
{
    // Talk to the device and expect a very specific answer.
    SerialDevice *serial = manager->createSerialDeviceTTY(device.c_str(), 9600);
    bool ok = serial->open(false);
    if (!ok) return false;

    vector<uchar> data;
    // First clear out any data in the queue.
    serial->receive(&data);
    data.clear();

    vector<uchar> msg(4);
    msg[0] = AMBER_SERIAL_SOF;
    msg[1] = CMD_SERIALNO_REQ;
    msg[2] = 0; // No payload
    msg[3] = xorChecksum(msg, 3);

    assert(msg[3] == 0xf4);

    verbose("(amb8465) are you there?\n");
    serial->send(msg);
    // Wait for 100ms so that the USB stick have time to prepare a response.
    usleep(1000*100);
    serial->receive(&data);
    int limit = 0;
    while (data.size() > 8 && data[0] != 0xff) {
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

    string sent = bin2hex(msg);
    string recv = bin2hex(data);

    if (data.size() < 8 ||
        data[0] != 0xff ||
        data[1] != (0x80 | msg[1]) ||
        data[2] != 0x04 ||
        data[7] != xorChecksum(data, 7)) {
        return false;
    }
    return true;
}
