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

#include"serial.h"
#include"util.h"
#include"wmbus.h"
#include"wmbus_utils.h"

#include<assert.h>
#include<errno.h>
#include<fcntl.h>
#include<pthread.h>
#include<semaphore.h>
#include<sys/types.h>
#include<unistd.h>

using namespace std;

struct WMBusSimulator : public WMBusCommonImplementation
{
    bool ping();
    string getDeviceId();
    LinkModeSet getLinkModes();
    void deviceReset();
    void deviceSetLinkModes(LinkModeSet lms);
    LinkModeSet supportedLinkModes() { return Any_bit; }
    int numConcurrentLinkModes() { return 0; }
    bool canSetLinkModes(LinkModeSet lms) { return true; }

    void processSerialData();
    void simulate();
    string device() { return file_; }

    WMBusSimulator(string file, shared_ptr<SerialCommunicationManager> manager);

private:
    vector<uchar> received_payload_;
    vector<function<void(Telegram*)>> telegram_listeners_;

    string file_;
    LinkModeSet link_modes_;
    vector<string> lines_;
};

shared_ptr<WMBus> openSimulator(string device, shared_ptr<SerialCommunicationManager> manager, shared_ptr<SerialDevice> serial_override)
{
    WMBusSimulator *imp = new WMBusSimulator(device, manager);
    return shared_ptr<WMBus>(imp);
}

WMBusSimulator::WMBusSimulator(string file, shared_ptr<SerialCommunicationManager> manager)
    : WMBusCommonImplementation(DEVICE_SIMULATION, manager, NULL), file_(file)
{
    assert(file != "");
    loadFile(file, &lines_);
}

bool WMBusSimulator::ping()
{
    return true;
}

string WMBusSimulator::getDeviceId()
{
    return "?";
}

LinkModeSet WMBusSimulator::getLinkModes()
{
    string hr = link_modes_.hr();
    return link_modes_;
}

void WMBusSimulator::deviceReset()
{
}

void WMBusSimulator::deviceSetLinkModes(LinkModeSet lms)
{
    link_modes_ = lms;
}

void WMBusSimulator::processSerialData()
{
    assert(0);
}

void WMBusSimulator::simulate()
{
    time_t start_time = time(NULL);

    for (auto l : lines_)
    {
        string hex = "";
        int found_time = 0;
        time_t rel_time = 0;
        if (l.substr(0,9) == "telegram=")
        {
            for (size_t i=9; i<l.length(); ++i)
            {
                if (l[i] == '|') continue;
                if (l[i] == '+')
                {
                    found_time = i;
                    rel_time = atoi(&l[i+1]);
                    break;
                }
                hex += l[i];
            }
            if (found_time)
            {
                debug("(simulation) from file \"%s\" to trigger at relative time %ld\n", hex.c_str(), rel_time);
                time_t curr = time(NULL);
                if (curr < start_time+rel_time)
                {
                    debug("(simulation) waiting %d seconds before simulating telegram.\n", (start_time+rel_time)-curr);
                    for (;;)
                    {
                        curr = time(NULL);
                        if (curr > start_time + rel_time) break;
                        usleep(1000*1000);
                        if (!manager_->isRunning())
                        {
                            debug("(simulation) exiting early\n");
                            break;
                        }
                    }
                }
            }
            else
            {
                debug("(simulation) from file \"%s\"\n", hex.c_str());
            }
        }
        else
        {
            continue;
        }

        vector<uchar> payload;
        bool ok = hex2bin(hex.c_str(), &payload);
        if (!ok)
        {
            error("Not a valid string of hex bytes! \"%s\"\n", l.c_str());
        }
        handleTelegram(payload);
    }
    manager_->stop();
}
