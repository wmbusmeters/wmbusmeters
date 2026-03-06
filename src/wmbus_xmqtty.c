/*
 Copyright (C) 2024 Fredrik Öhrström (gpl-3.0-or-later)

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
#include"meters.h"
#include"drivers.h"
#include"decode.h"

#include<assert.h>
#include<pthread.h>
#include<semaphore.h>
#include<errno.h>
#include<unistd.h>
#include<string.h>

using namespace std;

struct WMBusXmqTTY : public virtual BusDeviceCommonImplementation
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

    WMBusXmqTTY(string bus_alias, shared_ptr<SerialDevice> serial,
                 shared_ptr<SerialCommunicationManager> manager);
    ~WMBusXmqTTY() { }

private:
    void processLine(const string &line);

    string line_buffer_;
    LinkModeSet link_modes_;
    DecoderSession session_;
};

shared_ptr<BusDevice> openXmqTTY(Detected detected,
                                 shared_ptr<SerialCommunicationManager> manager,
                                 shared_ptr<SerialDevice> serial_override)
{
    string bus_alias = detected.specified_device.bus_alias;
    string device = detected.found_file;

    if (serial_override)
    {
        WMBusXmqTTY *imp = new WMBusXmqTTY(bus_alias, serial_override, manager);
        imp->markAsNoLongerSerial();
        return shared_ptr<BusDevice>(imp);
    }
    auto serial = manager->createSerialDeviceTTY(device.c_str(), 0, PARITY::NONE, "xmqtty");
    WMBusXmqTTY *imp = new WMBusXmqTTY(bus_alias, serial, manager);
    return shared_ptr<BusDevice>(imp);
}

WMBusXmqTTY::WMBusXmqTTY(string bus_alias, shared_ptr<SerialDevice> serial,
                         shared_ptr<SerialCommunicationManager> manager) :
    BusDeviceCommonImplementation(bus_alias, DEVICE_XMQTTY, manager, serial, true)
{
    reset();
    // Load all drivers once at init, not for every telegram
    loadAllBuiltinDrivers();
}

bool WMBusXmqTTY::ping()
{
    return true;
}

string WMBusXmqTTY::getDeviceId()
{
    return "?";
}

string WMBusXmqTTY::getDeviceUniqueId()
{
    return "?";
}

LinkModeSet WMBusXmqTTY::getLinkModes()
{
    return link_modes_;
}

void WMBusXmqTTY::deviceReset()
{
}

bool WMBusXmqTTY::deviceSetLinkModes(LinkModeSet lms)
{
    return true;
}

void WMBusXmqTTY::processLine(const string &line)
{
    string result = decodeLine(session_, line);
    printf("%s\n", result.c_str());
    fflush(stdout);
}

void WMBusXmqTTY::processSerialData()
{
    vector<uchar> data;

    // Receive serial data
    serial()->receive(&data);

    // Append to line buffer
    for (uchar c : data)
    {
        if (c == '\n')
        {
            // Process complete line
            if (!line_buffer_.empty())
            {
                processLine(line_buffer_);
                line_buffer_.clear();
            }
        }
        else if (c != '\r')
        {
            line_buffer_ += (char)c;
        }
    }
}
