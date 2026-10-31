/*
 Copyright (C) 2024-2026 Fredrik Öhrström (gpl-3.0-or-later)
 Copyright (C) 2026 Martin Klein (gpl-3.0-or-later)

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

// MBus over TCP — connects to a remote serial-to-mbus gateway via TCP.
// Usage: MAIN=tcp://10.200.0.110:4000:mbus
//        minomess:MAIN:mbus
//
// The TCP connection is managed by SerialDeviceTCP (in serial.cc).
// This file provides the MBus protocol layer on top, reusing the same
// frame parsing logic as MBusRawTTY (mbus_rawtty.cc).
//
// Reconnect handling:
//   When the TCP connection drops, SerialDeviceTCP::close() triggers
//   on_disappear_ → disconnectedFromDevice(). The 2-second HOT_PLUG_DETECTOR
//   timer then calls checkForDeadWmbusDevices() which removes the dead bus
//   device, and detectAndConfigureWmbusDevices() re-detects the tcp://
//   specification and reconnects — identical to how USB device hot-plug works.

#include"always.h"
#include"log.h"
#include"wmbus.h"
#include"wmbus_common_implementation.h"
#include"wmbus_utils.h"
#include"serial.h"
#include"util.h"

#include<assert.h>
#include<pthread.h>
#include<semaphore.h>
#include<unistd.h>

using namespace std;

struct MBusTCP : public BusDeviceCommonImplementation
{
    bool ping();
    string getDeviceId();
    string getDeviceUniqueId();
    LinkModeSet getLinkModes();
    void deviceReset();
    bool deviceSetLinkModes(LinkModeSet lms);
    LinkModeSet supportedLinkModes() { return Any_bit; }
    int numConcurrentLinkModes() { return 0; }
    bool canSetLinkModes(LinkModeSet desired_modes) { return true; }
    bool sendTelegram(LinkMode lm, TelegramFormat format, vector<uchar> &content);

    void processSerialData();
    void simulate() { }

    MBusTCP(string alias, shared_ptr<SerialDevice> serial, shared_ptr<SerialCommunicationManager> manager);
    ~MBusTCP() { }

private:

    vector<uchar> read_buffer_;
    LinkModeSet link_modes_;
    vector<uchar> received_payload_;
};

shared_ptr<BusDevice> openMBusTCP(Detected detected,
                                   shared_ptr<SerialCommunicationManager> manager,
                                   shared_ptr<SerialDevice> serial_override)
{
    string bus_alias = detected.specified_device.bus_alias;
    string host = detected.specified_device.tcp_host;
    int port = detected.specified_device.tcp_port;

    assert(host != "");
    assert(port > 0);

    if (serial_override)
    {
        MBusTCP *imp = new MBusTCP(bus_alias, serial_override, manager);
        imp->markAsNoLongerSerial();
        return shared_ptr<BusDevice>(imp);
    }

    auto serial = manager->createSerialDeviceTCP(host, port, "mbus_tcp");

    // Try to connect BEFORE creating the bus device.
    // If the remote gateway is unreachable, return NULL so the hot-plug timer
    // retries the connection on the next 2-second cycle.
    bool ok = serial->open(false);
    if (!ok)
    {
        verbose("(mbus_tcp) could not connect to %s:%d, will retry later\n", host.c_str(), port);
        // Mark the serial device for cleanup by the event loop.
        serial->close();
        return shared_ptr<BusDevice>(nullptr);
    }

    // Connection established — the MBusTCP constructor will call reset() → open(),
    // but SerialDeviceTCP::open() has a guard against double-open, so the
    // already-connected socket is reused.
    MBusTCP *imp = new MBusTCP(bus_alias, serial, manager);
    return shared_ptr<BusDevice>(imp);
}

MBusTCP::MBusTCP(string bus_alias, shared_ptr<SerialDevice> serial, shared_ptr<SerialCommunicationManager> manager) :
    BusDeviceCommonImplementation(bus_alias, DEVICE_MBUS_TCP, manager, serial, true)
{
    reset();
}

bool MBusTCP::ping()
{
    return true;
}

string MBusTCP::getDeviceId()
{
    return "?";
}

string MBusTCP::getDeviceUniqueId()
{
    return "?";
}

LinkModeSet MBusTCP::getLinkModes() {
    return link_modes_;
}

void MBusTCP::deviceReset()
{
    // Send an NKE message that resets the communication with all meters connected to the mbus.
    vector<uchar> buf;
    buf.resize(5);
    buf[0] = 0x10; // Start
    buf[1] = 0x40; // SND_NKE
    buf[2] = 0xff; // broadcast address 255
    uchar cs = 0;
    for (int i=1; i<3; ++i) cs += buf[i];
    buf[3] = cs; // checksum
    buf[4] = 0x16; // Stop

    verbose("Sending broadcast NKE to mbus_tcp %s\n", busAlias().c_str());
    serial()->send(buf);

    sleep(1);
}

bool MBusTCP::deviceSetLinkModes(LinkModeSet lms)
{
    return true;
}

void MBusTCP::processSerialData()
{
    vector<uchar> data;

    // Receive and accumulate serial data until a full frame has been received.
    serial()->receive(&data);

    if (data.size() == 0)
    {
        // EOF or connection closed — the event loop will detect !working()
        return;
    }

    read_buffer_.insert(read_buffer_.end(), data.begin(), data.end());

    size_t frame_length;
    int payload_len, payload_offset;

    for (;;)
    {
        FrameStatus status = checkMBusFrame(read_buffer_, &frame_length, &payload_len, &payload_offset, false);

        if (status == PartialFrame)
        {
            // Partial frame, stop eating.
            break;
        }
        if (status == ErrorInFrame)
        {
            verbose("(mbus_tcp) protocol error in message received!\n");
            string msg = bin2hex(read_buffer_);
            debug("(mbus_tcp) protocol error \"%s\"\n", msg.c_str());
            read_buffer_.clear();
            break;
        }
        if (status == FullFrame)
        {
            vector<uchar> payload;
            if (payload_len > 0)
            {
                payload.insert(payload.end(), read_buffer_.begin()+payload_offset, read_buffer_.begin()+payload_offset+payload_len);
            }
            read_buffer_.erase(read_buffer_.begin(), read_buffer_.begin()+frame_length);
            AboutTelegram about(busAlias(), 0, LinkMode::UNKNOWN, FrameType::MBUS);
            handleTelegram(about, payload);
        }
    }
}

bool MBusTCP::sendTelegram(LinkMode lm, TelegramFormat format, vector<uchar> &content)
{
    if (serial()->readonly()) return true;
    if (content.size() > 250) return false;

    vector<uchar> msg;
    if (format == TelegramFormat::MBUS_SHORT_FRAME)
    {
        msg.push_back(0x10);
    }
    else if (format == TelegramFormat::MBUS_LONG_FRAME)
    {
        msg.push_back(0x68);
        msg.push_back((uchar)content.size());
        msg.push_back((uchar)content.size());
        msg.push_back(0x68);
    }
    else
    {
        warning("(mbus_tcp) cannot use telegram format %s for sending on mbus\n", toString(format));
        return false;
    }

    uchar crc = 0;
    for (size_t i=0; i<content.size(); ++i)
    {
        msg.push_back(content[i]);
        crc += content[i];
    }
    msg.push_back(crc);
    msg.push_back(0x16);

    bool sent = serial()->send(msg);
    if (!sent) return false;

    return true;
}
