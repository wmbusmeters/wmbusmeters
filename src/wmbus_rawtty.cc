/*
 Copyright (C) 2019-2020 Fredrik Öhrström (gpl-3.0-or-later)

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
#include"serial.h"

#include<assert.h>
#include<pthread.h>
#include<semaphore.h>
#include<errno.h>
#include<unistd.h>

using namespace std;

struct WMBusRawTTY : public virtual BusDeviceCommonImplementation
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

    void processSerialData();
    void simulate() { }

    WMBusRawTTY(string bus_alias, shared_ptr<SerialDevice> serial,
                shared_ptr<SerialCommunicationManager> manager, bool use_hex);
    ~WMBusRawTTY() { }

private:

    void copy(vector<uchar> *from, vector<uchar> *to);

    vector<uchar> read_buffer_;
    vector<uchar> data_buffer_;
    LinkModeSet link_modes_;
    vector<uchar> received_payload_;
};

shared_ptr<BusDevice> openRawTTYInternal(Detected detected,
                                     shared_ptr<SerialCommunicationManager> manager,
                                     shared_ptr<SerialDevice> serial_override,
                                     bool use_hex)
{
    string bus_alias = detected.specified_device.bus_alias;
    string device = detected.found_file;
    int bps = detected.found_bps;

	if (detected.specified_device.command != "")
	{
		string identifier = "cmd_" + to_string(detected.specified_device.index);

		vector<string> args;
		vector<string> envs;
		args.push_back("-c");
		args.push_back(detected.specified_device.command);

		auto serial = manager->createSerialDeviceCommand(identifier, "/bin/sh", args, envs, "rawtty");
		WMBusRawTTY *imp = new WMBusRawTTY(bus_alias, serial, manager, use_hex);
		return shared_ptr<BusDevice>(imp);
	}

    if (serial_override)
    {
        WMBusRawTTY *imp = new WMBusRawTTY(bus_alias, serial_override, manager, use_hex);
        imp->markAsNoLongerSerial();
        return shared_ptr<BusDevice>(imp);
    }
    auto serial = manager->createSerialDeviceTTY(device.c_str(), bps, PARITY::NONE, use_hex?"hextty":"rawtty");
    WMBusRawTTY *imp = new WMBusRawTTY(bus_alias, serial, manager, use_hex);
    return shared_ptr<BusDevice>(imp);
}

shared_ptr<BusDevice> openRawTTY(Detected detected,
                             shared_ptr<SerialCommunicationManager> manager,
                             shared_ptr<SerialDevice> serial_override)
{
    return openRawTTYInternal(detected, manager, serial_override, false);
}

shared_ptr<BusDevice> openHexTTY(Detected detected,
                             shared_ptr<SerialCommunicationManager> manager,
                             shared_ptr<SerialDevice> serial_override)
{
    return openRawTTYInternal(detected, manager, serial_override, true);
}

WMBusRawTTY::WMBusRawTTY(string bus_alias, shared_ptr<SerialDevice> serial,
                         shared_ptr<SerialCommunicationManager> manager, bool use_hex) :
    BusDeviceCommonImplementation(bus_alias, use_hex?DEVICE_HEXTTY:DEVICE_RAWTTY, manager, serial, true)
{
    reset();
}

bool WMBusRawTTY::ping()
{
    return true;
}

string WMBusRawTTY::getDeviceId()
{
    return "?";
}

string WMBusRawTTY::getDeviceUniqueId()
{
    return "?";
}

LinkModeSet WMBusRawTTY::getLinkModes() {
    return link_modes_;
}

void WMBusRawTTY::deviceReset()
{
}

bool WMBusRawTTY::deviceSetLinkModes(LinkModeSet lms)
{
    return true;
}

void WMBusRawTTY::copy(vector<uchar> *from, vector<uchar> *to)
{
    if (type() == BusDeviceType::DEVICE_RAWTTY)
    {
        // We expect binary bytes incoming.
        to->insert(to->end(), from->begin(), from->end());
        debug("copied %zu binary bytes\n", from->size());
        from->clear();
        return;
    }

    if (type() == BusDeviceType::DEVICE_HEXTTY)
    {
        // We expect hex chars incoming. Everything else is thrown away.
        vector<uchar> hex;
        int num_hex = 0;
        int num_other = 0;
        for (uchar c : *from)
        {
            if (isHexChar(c))
            {
                hex.push_back(c); // Just ignore any non-hex chars!
                num_hex++;
            }
            else
            {
                num_other++;
            }
        }
        debug("found %d hex chars and %d other bytes\n", num_hex, num_other);
        from->clear();
        if (hex.size() > 0)
        {
            if (hex.size() % 2 == 1)
            {
                // An odd hexadecimal char at the end!
                // Save it for later!
                from->push_back(hex.back());
                hex.pop_back();
            }
            // We now have an even number of hex chars to work with!
            vector<uchar> bin;
            bool ok = hex2bin(hex, &bin);
            assert(ok);
            debug("converted %zu hex bytes into %zu binary bytes.\n", hex.size(), bin.size());
            to->insert(to->end(), bin.begin(), bin.end());
        }
        return;
    }

    assert(0);
}

void WMBusRawTTY::processSerialData()
{
    vector<uchar> data;

    // Receive and accumulated serial data until a full frame has been received.
    serial()->receive(&data);

    read_buffer_.insert(read_buffer_.end(), data.begin(), data.end());

    copy(&read_buffer_, &data_buffer_);

    size_t frame_length;
    int payload_len, payload_offset;

    for (;;)
    {
        FrameStatus status = checkWMBusFrame(data_buffer_, &frame_length, &payload_len, &payload_offset, false);

        if (payload_len == 0)
        {
            verbose("(rawtty) protocol error in message received length byte is zero!\n");
            string msg = bin2hex(data_buffer_);
            debug("(rawtty) protocol error \"%s\"\n", msg.c_str());
            data_buffer_.clear();
            break;
        }
        if (status == PartialFrame)
        {
            // Partial frame, stop eating.
            break;
        }
        if (status == ErrorInFrame)
        {
            verbose("(rawtty) protocol error in message received!\n");
            string msg = bin2hex(data_buffer_);
            debug("(rawtty) protocol error \"%s\"\n", msg.c_str());
            data_buffer_.clear();
            break;
        }
        if (status == FullFrame)
        {
            vector<uchar> payload;
            if (payload_len > 0)
            {
                uchar l = payload_len;
                payload.insert(payload.end(), &l, &l+1); // Re-insert the len byte.
                payload.insert(payload.end(), data_buffer_.begin()+payload_offset, data_buffer_.begin()+payload_offset+payload_len);
            }
            data_buffer_.erase(data_buffer_.begin(), data_buffer_.begin()+frame_length);
            AboutTelegram about("", 0, FrameType::WMBUS);
            handleTelegram(about, payload);
        }
    }
}

AccessCheck detectRAWTTY(Detected *detected, shared_ptr<SerialCommunicationManager> manager)
{
    string tty = detected->specified_device.file;
    int bps = atoi(detected->specified_device.bps.c_str());

    // Since we do not know how to talk to the other end, it might not
    // even respond. The only thing we can do is to try to open the serial device.
    auto serial = manager->createSerialDeviceTTY(tty.c_str(), bps, PARITY::NONE, "detect rawtty");
    bool ok = serial->open(false);
    if (!ok) return AccessCheck::NoSuchDevice;

    serial->close();

    detected->setAsFound("", BusDeviceType::DEVICE_RAWTTY, false, bps,
        detected->specified_device.linkmodes);

    return AccessCheck::AccessOK;
}
