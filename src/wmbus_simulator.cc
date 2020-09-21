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
#include<fcntl.h>
#include<pthread.h>
#include<semaphore.h>
#include<errno.h>
#include<sys/types.h>
#include<unistd.h>

using namespace std;

struct WMBusSimulator : public WMBusCommonImplementation
{
    bool ping();
    uint32_t getDeviceId();
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

int loadFile(string file, vector<string> *lines);

shared_ptr<WMBus> openSimulator(string device, shared_ptr<SerialCommunicationManager> manager, shared_ptr<SerialDevice> serial_override)
{
    WMBusSimulator *imp = new WMBusSimulator(device, manager);
    return shared_ptr<WMBus>(imp);
}

WMBusSimulator::WMBusSimulator(string file, shared_ptr<SerialCommunicationManager> manager)
    : WMBusCommonImplementation(DEVICE_SIMULATOR, manager, NULL), file_(file)
{
    vector<string> lines;
    loadFile(file, &lines_);
}

bool WMBusSimulator::ping() {
    verbose("(simulator) ping\n");
    verbose("(simulator) pong\n");
    return true;
}

uint32_t WMBusSimulator::getDeviceId() {
    verbose("(simulator) get device info\n");
    verbose("(simulator) device info: 11111111\n");
    return 0x11111111;
}

LinkModeSet WMBusSimulator::getLinkModes() {
    verbose("(simulator) get link mode\n");
    string hr = link_modes_.hr();
    verbose("(simulator) config: link mode %s\n", hr.c_str());
    return link_modes_;
}

void WMBusSimulator::deviceReset()
{
}

void WMBusSimulator::deviceSetLinkModes(LinkModeSet lms)
{
    link_modes_ = lms;
    string hr = lms.hr();
    verbose("(simulator) set link mode %s\n", hr.c_str());
    verbose("(simulator) set link mode completed\n");
}

int loadFile(string file, vector<string> *lines)
{
    char block[32768+1];
    vector<uchar> buf;

    int fd = open(file.c_str(), O_RDONLY);
    if (fd == -1) {
        return -1;
    }
    while (true) {
        ssize_t n = read(fd, block, sizeof(block));
        if (n == -1) {
            if (errno == EINTR) {
                continue;
            }
            error("Could not read file %s errno=%d\n", file.c_str(), errno);
            close(fd);
            return -1;
        }
        buf.insert(buf.end(), block, block+n);
        if (n < (ssize_t)sizeof(block)) {
            break;
        }
    }
    close(fd);

    bool eof, err;
    auto i = buf.begin();
    for (;;) {
        string line = eatTo(buf, i, '\n', 32768, &eof, &err);
        if (err) {
            error("Error parsing simulation file.\n");
        }
        if (line.length() > 0) {
            lines->push_back(line);
        }
        if (eof) break;
    }

    return 0;
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
                debug("(simulator) from file \"%s\" to trigger at relative time %ld\n", hex.c_str(), rel_time);
                time_t curr = time(NULL);
                if (curr < start_time+rel_time)
                {
                    debug("(simulator) waiting %d seconds before simulating telegram.\n", (start_time+rel_time)-curr);
                    for (;;)
                    {
                        curr = time(NULL);
                        if (curr > start_time + rel_time) break;
                        usleep(1000*1000);
                        if (!manager_->isRunning())
                        {
                            debug("(simulator) exiting early\n");
                            break;
                        }
                    }
                }
            }
            else
            {
                debug("(simulator) from file \"%s\"\n", hex.c_str());
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
