/*
 Copyright (C) 2019 Fredrik Öhrström

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
#include"serial.h"

#include<assert.h>
#include<pthread.h>
#include<semaphore.h>
#include<sys/errno.h>
#include<unistd.h>

using namespace std;

enum FrameStatus { PartialFrame, FullFrame, ErrorInFrame };

struct WMBusRawTTY : public WMBus
{
    bool ping();
    uint32_t getDeviceId();
    LinkModeSet getLinkModes();
    void setLinkModes(LinkModeSet lms);
    LinkModeSet supportedLinkModes() { return Any_bit; }
    int numConcurrentLinkModes() { return 0; }
    bool canSetLinkModes(LinkModeSet desired_modes) { return true; }
    void onTelegram(function<void(Telegram*)> cb);

    void processSerialData();
    void getConfiguration();
    SerialDevice *serial() { return serial_.get(); }
    void simulate() { }

    WMBusRawTTY(unique_ptr<SerialDevice> serial, SerialCommunicationManager *manager);
    ~WMBusRawTTY() { }

private:
    unique_ptr<SerialDevice> serial_;
    SerialCommunicationManager *manager_;
    vector<uchar> read_buffer_;
    sem_t command_wait_;
    LinkModeSet link_modes_;
    vector<uchar> received_payload_;
    vector<function<void(Telegram*)>> telegram_listeners_;

    void waitForResponse();
    FrameStatus checkRawTTYFrame(vector<uchar> &data,
                                 size_t *frame_length,
                                 int *payload_len_out,
                                 int *payload_offset);
    void handleMessage(vector<uchar> &frame);
};

unique_ptr<WMBus> openRawTTY(string device, int baudrate, SerialCommunicationManager *manager)
{
    auto serial = manager->createSerialDeviceTTY(device.c_str(), baudrate);
    WMBusRawTTY *imp = new WMBusRawTTY(std::move(serial), manager);
    return unique_ptr<WMBus>(imp);
}

unique_ptr<WMBus> openRawTTY(string device, SerialCommunicationManager *manager, SerialDevice *serial)
{
    WMBusRawTTY *imp = new WMBusRawTTY(unique_ptr<SerialDevice>(serial), manager);
    return unique_ptr<WMBus>(imp);
}

WMBusRawTTY::WMBusRawTTY(unique_ptr<SerialDevice> serial, SerialCommunicationManager *manager) :
    serial_(std::move(serial)), manager_(manager)
{
    sem_init(&command_wait_, 0, 0);
    manager_->listenTo(serial_.get(),call(this,processSerialData));
    serial_->open(true);
}

bool WMBusRawTTY::ping()
{
    return true;
}

uint32_t WMBusRawTTY::getDeviceId()
{
    return 0;
}

LinkModeSet WMBusRawTTY::getLinkModes() {
    return link_modes_;
}

void WMBusRawTTY::getConfiguration()
{
}

void WMBusRawTTY::setLinkModes(LinkModeSet lms)
{
    link_modes_ = lms;
}

void WMBusRawTTY::onTelegram(function<void(Telegram*)> cb) {
    telegram_listeners_.push_back(cb);
}

void WMBusRawTTY::waitForResponse() {
    while (manager_->isRunning()) {
        int rc = sem_wait(&command_wait_);
        if (rc==0) break;
        if (rc==-1) {
            if (errno==EINTR) continue;
            break;
        }
    }
}

FrameStatus WMBusRawTTY::checkRawTTYFrame(vector<uchar> &data,
                                          size_t *frame_length,
                                          int *payload_len_out,
                                          int *payload_offset)
{
    // telegram=|2A442D2C998734761B168D2021D0871921|58387802FF2071000413F81800004413F8180000615B|+96
    if (data.size() == 0) return PartialFrame;
    int payload_len = data[0];
    *payload_len_out = payload_len;
    *payload_offset = 1;
    *frame_length = payload_len+1;
    if (data.size() < *frame_length) return PartialFrame;

    return FullFrame;
}

void WMBusRawTTY::processSerialData()
{
    vector<uchar> data;

    // Receive and accumulated serial data until a full frame has been received.
    serial_->receive(&data);

    read_buffer_.insert(read_buffer_.end(), data.begin(), data.end());

    size_t frame_length;
    int payload_len, payload_offset;

    FrameStatus status = checkRawTTYFrame(read_buffer_, &frame_length, &payload_len, &payload_offset);

    if (status == ErrorInFrame) {
        verbose("(rawtty) protocol error in message received!\n");
        string msg = bin2hex(read_buffer_);
        debug("(rawtty) protocol error \"%s\"\n", msg.c_str());
        read_buffer_.clear();
    } else
    if (status == FullFrame) {

        vector<uchar> payload;
        if (payload_len > 0) {
            uchar l = payload_len;
            payload.insert(payload.end(), &l, &l+1); // Re-insert the len byte.
            payload.insert(payload.end(), read_buffer_.begin()+payload_offset, read_buffer_.begin()+payload_offset+payload_len);
        }

        read_buffer_.erase(read_buffer_.begin(), read_buffer_.begin()+frame_length);
        handleMessage(payload);
    }
}

void WMBusRawTTY::handleMessage(vector<uchar> &frame)
{
    Telegram t;
    t.parse(frame);
    bool handled = false;
    for (auto f : telegram_listeners_)
    {
        Telegram copy = t;
        if (f) f(&copy);
        if (copy.handled) handled = true;
    }
    if (isVerboseEnabled() && !handled)
    {
        verbose("(dummyserial) telegram ignored by all configured meters!\n");
    }
}

bool detectRawTTY(string device, SerialCommunicationManager *manager)
{
    // Since we do not know how to talk to the other end, it might not
    // even respond. The only thing we can do is to try to open the serial device.
    auto serial = manager->createSerialDeviceTTY(device.c_str(), 38400);
    bool ok = serial->open(false);
    if (!ok) return false;

    serial->close();
    return true;
}
