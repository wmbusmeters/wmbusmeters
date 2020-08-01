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

struct WMBusRawTTY : public virtual WMBusCommonImplementation
{
    bool ping();
    uint32_t getDeviceId();
    LinkModeSet getLinkModes();
    void deviceReset();
    void deviceSetLinkModes(LinkModeSet lms);
    LinkModeSet supportedLinkModes() { return Any_bit; }
    int numConcurrentLinkModes() { return 0; }
    bool canSetLinkModes(LinkModeSet desired_modes) { return true; }

    void processSerialData();
    void simulate() { }

    WMBusRawTTY(unique_ptr<SerialDevice> serial, SerialCommunicationManager *manager);
    ~WMBusRawTTY() { }

private:

    vector<uchar> read_buffer_;
    sem_t command_wait_;
    LinkModeSet link_modes_;
    vector<uchar> received_payload_;

    void waitForResponse();
};

unique_ptr<WMBus> openRawTTY(string device, int baudrate, SerialCommunicationManager *manager, unique_ptr<SerialDevice> serial_override)
{
    if (serial_override)
    {
        WMBusRawTTY *imp = new WMBusRawTTY(std::move(serial_override), manager);
        return unique_ptr<WMBus>(imp);
    }
    auto serial = manager->createSerialDeviceTTY(device.c_str(), baudrate);
    WMBusRawTTY *imp = new WMBusRawTTY(std::move(serial), manager);
    return unique_ptr<WMBus>(imp);
}

WMBusRawTTY::WMBusRawTTY(unique_ptr<SerialDevice> serial, SerialCommunicationManager *manager) :
    WMBusCommonImplementation(DEVICE_RAWTTY, manager, std::move(serial))
{
    sem_init(&command_wait_, 0, 0);
    manager_->listenTo(this->serial(),call(this,processSerialData));
    reset();
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

void WMBusRawTTY::deviceReset()
{
}

void WMBusRawTTY::deviceSetLinkModes(LinkModeSet lms)
{
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

void WMBusRawTTY::processSerialData()
{

    vector<uchar> data;

    // Receive and accumulated serial data until a full frame has been received.
    serial()->receive(&data);

    read_buffer_.insert(read_buffer_.end(), data.begin(), data.end());

    size_t frame_length;
    int payload_len, payload_offset;

    for (;;)
    {
        FrameStatus status = checkWMBusFrame(read_buffer_, &frame_length, &payload_len, &payload_offset);

        if (status == PartialFrame)
        {
            // Partial frame, stop eating.
            break;
        }
        if (status == ErrorInFrame)
        {
            verbose("(rawtty) protocol error in message received!\n");
            string msg = bin2hex(read_buffer_);
            debug("(rawtty) protocol error \"%s\"\n", msg.c_str());
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

AccessCheck detectRawTTY(string device, int baud, SerialCommunicationManager *manager)
{
    // Since we do not know how to talk to the other end, it might not
    // even respond. The only thing we can do is to try to open the serial device.
    auto serial = manager->createSerialDeviceTTY(device.c_str(), baud);
    AccessCheck rc = serial->open(false);
    if (rc != AccessCheck::AccessOK) return AccessCheck::NotThere;

    serial->close();
    return AccessCheck::AccessOK;
}
