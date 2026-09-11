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

#include"always.h"
#include"command_handler.h"
#include"log.h"
#include"wmbus.h"
#include"wmbus_common_implementation.h"
#include"wmbus_utils.h"
#include"serial.h"
#include"meters.h"
#include"drivers.h"
#include"utils/doc.h"
#include"xmq.h"

#include<pthread.h>
#include<semaphore.h>
#include<errno.h>
#include<unistd.h>
#include<sstream>
#include<string.h>
#include<iomanip>
#include<map>

using namespace std;

struct WMBusSocket : public BusDeviceCommonImplementation
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

    WMBusSocket(string bus_alias, shared_ptr<SerialDevice> serial,
                shared_ptr<SerialCommunicationManager> manager);
    ~WMBusSocket() { }

private:

    string line_buffer_;
    LinkModeSet link_modes_;

    CommandHandler command_handler_;
};

shared_ptr<BusDevice> openSocket(Detected detected,
                                 shared_ptr<SerialCommunicationManager> manager,
                                 shared_ptr<SerialDevice> serial_override)
{
    string bus_alias = detected.specified_device.bus_alias;
    string socket_path = detected.specified_device.extras;

    if (socket_path.empty())
    {
        error(EXIT_SOCKET_ERROR, "(socket) no socket path specified. Use SOCKET(/path/to/socket)\n");
    }

    auto serial = manager->createSerialDeviceSocket(socket_path, "socket");
    WMBusSocket *imp = new WMBusSocket(bus_alias, serial, manager);
    return shared_ptr<BusDevice>(imp);
}

WMBusSocket::WMBusSocket(string bus_alias, shared_ptr<SerialDevice> serial,
                         shared_ptr<SerialCommunicationManager> manager) :
    BusDeviceCommonImplementation(bus_alias, DEVICE_SOCKET, manager, serial, true)
{
    reset();
}

bool WMBusSocket::ping()
{
    return true;
}

string WMBusSocket::getDeviceId()
{
    return "?";
}

string WMBusSocket::getDeviceUniqueId()
{
    return "?";
}

LinkModeSet WMBusSocket::getLinkModes()
{
    return link_modes_;
}

void WMBusSocket::deviceReset()
{
}

bool WMBusSocket::deviceSetLinkModes(LinkModeSet lms)
{
    return true;
}

void WMBusSocket::processSerialData()
{
    // If no client is connected, try to accept one
    if (!serial()->hasClient())
    {
        if (serial()->acceptClient())
        {
            verbose("(socket) client connected\n");
            line_buffer_.clear();
        }
        return;
    }

    // Client is connected, read data
    vector<uchar> data;
    int n = serial()->receive(&data);

    if (n == 0 && serial()->hasClient())
    {
        // read() returned 0 means EOF — client disconnected
        verbose("(socket) client disconnected\n");
        serial()->disconnectClient();
        line_buffer_.clear();
        return;
    }

    // Append to line buffer and process complete lines
    for (uchar c : data)
    {
        if (c == '\n')
        {
            if (!line_buffer_.empty())
            {
                string rsp = command_handler_.processRequest(line_buffer_);
                string line = rsp + "\n";
                vector<uchar> data(line.begin(), line.end());
                serial()->send(data);
                line_buffer_.clear();
            }
        }
        else if (c != '\r')
        {
            line_buffer_ += (char)c;
        }
    }
}
