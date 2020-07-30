/*
 Copyright (C) 2019-2020 Fredrik Öhrström

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
#include"serial.h"

#include<assert.h>
#include<pthread.h>
#include<semaphore.h>
#include<errno.h>
#include<unistd.h>

using namespace std;

struct WMBusD1TC : public virtual WMBusCommonImplementation
{
    bool ping();
    uint32_t getDeviceId();
    LinkModeSet getLinkModes();
    void setLinkModes(LinkModeSet lms);
    LinkModeSet supportedLinkModes() { return Any_bit; }
    int numConcurrentLinkModes() { return 0; }
    bool canSetLinkModes(LinkModeSet desired_modes) { return true; }

    void processSerialData();
    void getConfiguration();
    SerialDevice *serial() { return serial_.get(); }
    void simulate() { }
    bool reset();

    WMBusD1TC(unique_ptr<SerialDevice> serial, SerialCommunicationManager *manager);
    ~WMBusD1TC() { }

private:
    unique_ptr<SerialDevice> serial_;
    SerialCommunicationManager *manager_;
    vector<uchar> read_buffer_;
    sem_t command_wait_;
    LinkModeSet link_modes_;
    vector<uchar> received_payload_;

    void waitForResponse();
    FrameStatus checkD1TCFrame(vector<uchar> &data,
                                 size_t *frame_length,
                                 int *payload_len_out,
                                 int *payload_offset);
};

unique_ptr<WMBus> openD1TC(string device, SerialCommunicationManager *manager, unique_ptr<SerialDevice> serial_override)
{
    if (serial_override)
    {
        WMBusD1TC *imp = new WMBusD1TC(std::move(serial_override), manager);
        return unique_ptr<WMBus>(imp);
    }
    auto serial = manager->createSerialDeviceTTY(device.c_str(), 115200);
    WMBusD1TC *imp = new WMBusD1TC(std::move(serial), manager);
    return unique_ptr<WMBus>(imp);
}

WMBusD1TC::WMBusD1TC(unique_ptr<SerialDevice> serial, SerialCommunicationManager *manager) :
    WMBusCommonImplementation(DEVICE_D1TC), serial_(std::move(serial)), manager_(manager)
{
    sem_init(&command_wait_, 0, 0);
    manager_->listenTo(serial_.get(),call(this,processSerialData));
    serial_->open(true);
}

bool WMBusD1TC::ping()
{
    return true;
}

uint32_t WMBusD1TC::getDeviceId()
{
    return 0;
}

LinkModeSet WMBusD1TC::getLinkModes() {
    return link_modes_;
}

void WMBusD1TC::getConfiguration()
{
}

void WMBusD1TC::setLinkModes(LinkModeSet lms)
{
    link_modes_ = lms;
}

void WMBusD1TC::waitForResponse() {
    while (manager_->isRunning()) {
        int rc = sem_wait(&command_wait_);
        if (rc==0) break;
        if (rc==-1) {
            if (errno==EINTR) continue;
            break;
        }
    }
}

FrameStatus WMBusD1TC::checkD1TCFrame(vector<uchar> &data,
                                          size_t *frame_length,
                                          int *payload_len_out,
                                          int *payload_offset)
{
    // Nice clean: 2A442D2C998734761B168D2021D0871921|58387802FF2071000413F81800004413F8180000615B
    // Ugly: 00615B2A442D2C998734761B168D2021D0871921|58387802FF2071000413F81800004413F8180000615B
    // Here the frame is prefixed with some random data.

    debugPayload("(d1tc) checkD1TCFrame", data);

    if (data.size() < 11)
    {
        debug("(d1tc) less than 11 bytes, partial frame");
        return PartialFrame;
    }
    int payload_len = data[0];
    int type = data[1];
    int offset = 1;

    if (type != 0x44)
    {
        // Ouch, we are out of sync with the wmbus frames that we expect!
        // Since we currently do not handle any other type of frame, we can
        // look for the byte 0x44 in the buffer. If we find a 0x44 byte and
        // the length byte before it maps to the end of the buffer,
        // then we have found a valid telegram.
        bool found = false;
        for (size_t i = 0; i < data.size()-2; ++i)
        {
            if (data[i+1] == 0x44)
            {
                payload_len = data[i];
                size_t remaining = data.size()-i;
                if (data[i]+1 == (uchar)remaining && data[i+1] == 0x44)
                {
                    found = true;
                    offset = i+1;
                    verbose("(wmbus_d1tc) out of sync, skipping %d bytes.\n", (int)i);
                    break;
                }
            }
        }
        if (!found)
        {
            // No sensible telegram in the buffer. Flush it!
            verbose("(wmbus_d1tc) no sensible telegram found, clearing buffer.\n");
            data.clear();
            return ErrorInFrame;
        }
    }
    *payload_len_out = payload_len;
    *payload_offset = offset;
    *frame_length = payload_len+offset;
    if (data.size() < *frame_length)
    {
        debug("(rawtty) not enough bytes, partial frame %d %d", data.size(), *frame_length);
        return PartialFrame;
    }
    debug("(d1tc) received full frame\n");
    return FullFrame;
}

void WMBusD1TC::processSerialData()
{

    vector<uchar> data;

    // Receive and accumulated serial data until a full frame has been received.
    serial_->receive(&data);

    read_buffer_.insert(read_buffer_.end(), data.begin(), data.end());

    size_t frame_length;
    int payload_len, payload_offset;

    for (;;)
    {
        FrameStatus status = checkD1TCFrame(read_buffer_, &frame_length, &payload_len, &payload_offset);

        if (status == PartialFrame)
        {
            // Partial frame, stop eating.
            break;
        }
        if (status == ErrorInFrame)
        {
            verbose("(d1tc) protocol error in message received!\n");
            string msg = bin2hex(read_buffer_);
            debug("(d1tc) protocol error \"%s\"\n", msg.c_str());
            read_buffer_.clear();
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
            handleTelegram(payload);
        }
    }
}

bool WMBusD1TC::reset()
{
    return false;
}

AccessCheck detectD1TC(string device, int baud, SerialCommunicationManager *manager)
{
    // Since we do not know how to talk to the other end, it might not
    // even respond. The only thing we can do is to try to open the serial device.
    auto serial = manager->createSerialDeviceTTY(device.c_str(), baud);
    AccessCheck rc = serial->open(false);
    if (rc != AccessCheck::AccessOK) return AccessCheck::NotThere;

    serial->close();
    return AccessCheck::AccessOK;
}
