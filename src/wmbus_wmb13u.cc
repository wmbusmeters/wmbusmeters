/*
 Copyright (C) 2020 Fredrik Öhrström

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
#include"wmbus_cul.h"
#include"serial.h"

#include<assert.h>
#include<fcntl.h>
#include<grp.h>
#include<pthread.h>
#include<semaphore.h>
#include<string.h>
#include<sys/errno.h>
#include<sys/stat.h>
#include<sys/types.h>
#include<unistd.h>

using namespace std;

#define SET_LINK_MODE 1
#define SET_X01_MODE 2

struct WMBusWMB13U : public virtual WMBusCommonImplementation
{
    bool ping();
    uint32_t getDeviceId();
    LinkModeSet getLinkModes();
    void setLinkModes(LinkModeSet lms);
    LinkModeSet supportedLinkModes() {
        return
            C1_bit |
            S1_bit |
            T1_bit;
    }
    int numConcurrentLinkModes() { return 1; }
    bool canSetLinkModes(LinkModeSet lms)
    {
        if (0 == countSetBits(lms.bits())) return false;
        if (!supportedLinkModes().supports(lms)) return false;
        // Ok, the supplied link modes are compatible,
        // but wmb13u can only listen to one at a time.
        return 1 == countSetBits(lms.bits());
    }
    void processSerialData();
    SerialDevice *serial() { return serial_.get(); }
    void simulate();

    WMBusWMB13U(unique_ptr<SerialDevice> serial, SerialCommunicationManager *manager);
    ~WMBusWMB13U() { }

private:
    unique_ptr<SerialDevice> serial_;
    SerialCommunicationManager *manager_ {};
    LinkModeSet link_modes_ {};
    vector<uchar> read_buffer_;
    vector<uchar> received_payload_;
    sem_t command_wait_;
    string sent_command_;
    string received_response_;

    void waitForResponse();

    FrameStatus checkWMB13UFrame(vector<uchar> &data,
                                 size_t *frame_length,
                                 vector<uchar> &payload);

    string setup_;
};

unique_ptr<WMBus> openWMB13U(string device, SerialCommunicationManager *manager, unique_ptr<SerialDevice> serial_override)
{
    if (serial_override)
    {
        WMBusWMB13U *imp = new WMBusWMB13U(std::move(serial_override), manager);
        return unique_ptr<WMBus>(imp);
    }

    auto serial = manager->createSerialDeviceTTY(device.c_str(), 19200);
    WMBusWMB13U *imp = new WMBusWMB13U(std::move(serial), manager);
    return unique_ptr<WMBus>(imp);
}

WMBusWMB13U::WMBusWMB13U(unique_ptr<SerialDevice> serial, SerialCommunicationManager *manager) :
    WMBusCommonImplementation(DEVICE_WMB13U), serial_(std::move(serial)), manager_(manager)
{
    sem_init(&command_wait_, 0, 0);
    manager_->listenTo(serial_.get(),call(this,processSerialData));
    serial_->open(true);
}

bool WMBusWMB13U::ping()
{
    if (serial_->readonly()) return true; // Feeding from stdin or file.

    verbose("(wmb13u) ping\n");
    return true;
}

uint32_t WMBusWMB13U::getDeviceId()
{
    verbose("(cul) getDeviceId\n");
    return 0x11111111;
}

LinkModeSet WMBusWMB13U::getLinkModes()
{
    return link_modes_;
}

void WMBusWMB13U::setLinkModes(LinkModeSet lms)
{
    if (serial_->readonly()) return; // Feeding from stdin or file.
    // AT<CR> -> OK<CR>      Enter config mode.
    // ATG<01> -> OK<CR>     for T1
    // ATG<03> -> OK<CR>     for S1
    // ATG<10> -> OK<CR>     for C1
    // ATQ<CR> -> OK<CR>     Exit config mode.
}

void WMBusWMB13U::waitForResponse()
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

void WMBusWMB13U::simulate()
{
}

void WMBusWMB13U::processSerialData()
{

    vector<uchar> data;

    // Receive and accumulated serial data until a full frame has been received.
    serial_->receive(&data);

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
            verbose("(wmb13u) protocol error in message received!\n");
            string msg = bin2hex(read_buffer_);
            debug("(wmb13u) protocol error \"%s\"\n", msg.c_str());
            read_buffer_.clear();
            break;
        }
        if (status == FullFrame)
        {
            vector<uchar> payload;
            if (payload_len > 0)
            {
                // It appens RSSI and 2 CRC bytes. Remove those.
                uchar l = payload_len - 3;
                payload.insert(payload.end(), &l, &l+1); // Re-insert the len byte.
                payload.insert(payload.end(), read_buffer_.begin()+payload_offset, read_buffer_.begin()+payload_offset+payload_len-3);
            }
            read_buffer_.erase(read_buffer_.begin(), read_buffer_.begin()+frame_length);
            handleTelegram(payload);
        }
    }
}


bool detectWMB13U(string device, SerialCommunicationManager *manager)
{
    // Talk to the device and expect a very specific answer.
    auto serial = manager->createSerialDeviceTTY(device.c_str(), 19200);
    bool ok = serial->open(false);
    if (!ok) return false;

    verbose("(wmb13u) are you there?\n");

    vector<uchar> data;

    // send 0xFF to wake up, if sleeping, just to be sure.
    vector<uchar> wakeup(1);
    wakeup[0] = 0xff;

    serial->send(wakeup);
    usleep(1000*200);
    data.clear();
    serial->receive(&data);

    usleep(1000*200);
    // send 'AT' to enter configuration mode, expects OK.
    vector<uchar> at(2);
    at[0] = 'A';
    at[1] = 'T';

    serial->send(at);
    usleep(1000*200);
    serial->receive(&data);

    // send 'AT0' to  get 130 bytes of configuration.
    vector<uchar> at0(3);
    at[0] = 'A';
    at[1] = 'T';
    at[2] = '0';

    serial->send(at);
    usleep(1000*200);
    serial->receive(&data);

    vector<uchar> atq(3);
    atq[0] = 'A';
    atq[1] = 'T';
    atq[2] = 'Q';

    serial->send(atq);
    usleep(1000*200);
    serial->receive(&data);

    serial->close();
    return true;
}
