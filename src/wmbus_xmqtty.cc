/*
 Copyright (C) 2024-2026 Fredrik Öhrström (gpl-3.0-or-later)

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

#include"command_handler.h"
#include"wmbus.h"
#include"wmbus_common_implementation.h"
#include"wmbus_utils.h"
#include"serial.h"
#include"meters.h"
#include"drivers.h"
#include"xmq.h"
#include"util.h"
#include"utils/doc.h"

#include<pthread.h>
#include<semaphore.h>
#include<errno.h>
#include<unistd.h>
#include<sstream>
#include<string.h>
#include<iomanip>
#include<map>

using namespace std;

struct WMBusXmqTTY : public BusDeviceCommonImplementation
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

    string line_buffer_;
    LinkModeSet link_modes_;

    CommandHandler command_handler_;
};

shared_ptr<BusDevice> openXmqTTY(Detected detected,
                                 shared_ptr<SerialCommunicationManager> manager,
                                 shared_ptr<SerialDevice> serial_override)
{
    string bus_alias = detected.specified_device.bus_alias;
    string device = detected.found_file;

    if (detected.specified_device.command != "")
    {
        string identifier = "cmd_" + to_string(detected.specified_device.index);

        vector<string> args;
        vector<string> envs;
        args.push_back("-c");
        args.push_back(detected.specified_device.command);

        auto serial = manager->createSerialDeviceCommand(identifier, "/bin/sh", args, envs, "rawtty");
        WMBusXmqTTY *imp = new WMBusXmqTTY(bus_alias, serial, manager);
        return shared_ptr<BusDevice>(imp);
    }

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
                string rsp = command_handler_.processRequest(line_buffer_);
                printf("%s\n", rsp.c_str());
                fflush(stdout);
                line_buffer_.clear();
            }
        }
        else if (c != '\r')
        {
            line_buffer_ += (char)c;
        }
    }
}
