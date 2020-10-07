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
#include<errno.h>
#include<sys/stat.h>
#include<sys/types.h>
#include<unistd.h>

using namespace std;

#define SET_LINK_MODE 1
#define SET_X01_MODE 2

struct WMBusRC1180 : public virtual WMBusCommonImplementation
{
    bool ping();
    string getDeviceId();
    LinkModeSet getLinkModes();
    void deviceReset();
    void deviceSetLinkModes(LinkModeSet lms);
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
        // but im871a can only listen to one at a time.
        return 1 == countSetBits(lms.bits());
    }
    void processSerialData();
    void simulate();

    WMBusRC1180(shared_ptr<SerialDevice> serial, shared_ptr<SerialCommunicationManager> manager);
    ~WMBusRC1180() { }

private:

    LinkModeSet link_modes_ {};
    vector<uchar> read_buffer_;
    vector<uchar> received_payload_;
    string sent_command_;
    string received_response_;

    FrameStatus checkRC1180Frame(vector<uchar> &data,
                              size_t *hex_frame_length,
                              vector<uchar> &payload);

    string setup_;
};

shared_ptr<WMBus> openRC1180(string device, shared_ptr<SerialCommunicationManager> manager, shared_ptr<SerialDevice> serial_override)
{
    assert(device != "");

    if (serial_override)
    {
        WMBusRC1180 *imp = new WMBusRC1180(serial_override, manager);
        return shared_ptr<WMBus>(imp);
    }

    auto serial = manager->createSerialDeviceTTY(device.c_str(), 38400, "rc1180");
    WMBusRC1180 *imp = new WMBusRC1180(serial, manager);
    return shared_ptr<WMBus>(imp);
}

WMBusRC1180::WMBusRC1180(shared_ptr<SerialDevice> serial, shared_ptr<SerialCommunicationManager> manager) :
    WMBusCommonImplementation(DEVICE_RC1180, manager, serial)
{
    reset();
}

bool WMBusRC1180::ping()
{
    verbose("(rc1180) ping\n");
    return true;
}

string WMBusRC1180::getDeviceId()
{
    verbose("(rc1180) getDeviceId\n");
    return "?";
}

LinkModeSet WMBusRC1180::getLinkModes()
{
    return link_modes_;
}

void WMBusRC1180::deviceReset()
{
    // No device specific settings needed right now.
    // The common code in wmbus.cc reset()
    // will open the serial device and potentially
    // set the link modes properly.
}

void WMBusRC1180::deviceSetLinkModes(LinkModeSet lms)
{
    if (serial()->readonly()) return; // Feeding from stdin or file.

    return;

    if (!canSetLinkModes(lms))
    {
        string modes = lms.hr();
        error("(rc1180) setting link mode(s) %s is not supported\n", modes.c_str());
    }
    // 'brc' command: b - wmbus, r - receive, c - c mode (with t)
    vector<uchar> msg(5);
    msg[0] = 'b';
    msg[1] = 'r';
    if (lms.has(LinkMode::C1)) {
        msg[2] = 'c';
    } else if (lms.has(LinkMode::S1)) {
        msg[2] = 's';
    } else if (lms.has(LinkMode::T1)) {
        msg[2] = 't';
    }
    msg[3] = 0xa;
    msg[4] = 0xd;

    verbose("(rc1180) set link mode %c\n", msg[2]);
    sent_command_ = string(&msg[0], &msg[3]);
    received_response_ = "";
    bool sent = serial()->send(msg);

    if (sent) waitForResponse(0);

    sent_command_ = "";
    debug("(rc1180) received \"%s\"", received_response_.c_str());

    bool ok = true;
    if (lms.has(LinkMode::C1)) {
        if (received_response_ != "CMODE") ok = false;
    } else if (lms.has(LinkMode::S1)) {
        if (received_response_ != "SMODE") ok = false;
    } else if (lms.has(LinkMode::T1)) {
        if (received_response_ != "TMODE") ok = false;
    }

    if (!ok)
    {
        string modes = lms.hr();
        error("(rc1180) setting link mode(s) %s is not supported for this cul device!\n", modes.c_str());
    }

    // X01 - start the receiver
    msg[0] = 'X';
    msg[1] = '0';
    msg[2] = '1';
    msg[3] = 0xa;
    msg[4] = 0xd;

    sent = serial()->send(msg);

    // Any response here, or does it silently move into listening mode?
}

void WMBusRC1180::simulate()
{
}

void WMBusRC1180::processSerialData()
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

AccessCheck detectRC1180(Detected *detected, shared_ptr<SerialCommunicationManager> manager)
{
    // Talk to the device and expect a very specific answer.
    auto serial = manager->createSerialDeviceTTY(detected->found_file.c_str(), 19200, "detect rc1180");
    serial->doNotUseCallbacks();
    AccessCheck rc = serial->open(false);
    if (rc != AccessCheck::AccessOK) return AccessCheck::NotThere;

    vector<uchar> data;
    vector<uchar> msg(1);

    // Send a single 0x00 byte. This will trigger the device to enter command mode
    // the device then responds with '>'
    msg[0] = 0;

    serial->send(msg);
    usleep(200*1000);
    serial->receive(&data);

    if (data[0] != '>') {
       // no RC1180 device detected
       serial->close();
       verbose("(rc1180) are you there? no.\n");
       return AccessCheck::NotThere;
    }

    data.clear();

    // send '0' to get get the version string: "V 1.67 nanoRC1180868" or similar
    msg[0] = '0';

    serial->send(msg);
    // Wait for 200ms so that the USB stick have time to prepare a response.
    usleep(1000*200);

    serial->receive(&data);
    string hex = bin2hex(data);

    msg[0] = 'X';
    serial->send(msg);

    serial->close();

    detected->setAsFound("12345678", WMBusDeviceType::DEVICE_RC1180, 19200, false, false);

    verbose("(rc1180) are you there? yes xxxxxx\n");

    return AccessCheck::AccessOK;
}
