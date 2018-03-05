// Copyright (c) 2018 Fredrik Öhrström
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include"wmbus.h"
#include"serial.h"

#include<assert.h>
#include<fcntl.h>
#include<pthread.h>
#include<semaphore.h>
#include<sys/types.h>
#include<unistd.h>

using namespace std;

struct WMBusSimulator : public WMBus {
    bool ping();
    uint32_t getDeviceId();
    LinkMode getLinkMode();
    void setLinkMode(LinkMode lm);
    void onTelegram(function<void(Telegram*)> cb);

    void processSerialData();
    SerialDevice *serial() { return NULL; }
    void simulate();

    WMBusSimulator(string file, SerialCommunicationManager *manager);

private:
    vector<uchar> received_payload_;
    vector<function<void(Telegram*)>> telegram_listeners_;

    string file_;
    SerialCommunicationManager *manager_;
    LinkMode link_mode_;
    vector<string> lines_;
};

int loadFile(string file, vector<string> *lines);

WMBus *openSimulator(string device, SerialCommunicationManager *manager)
{
    WMBusSimulator *imp = new WMBusSimulator(device, manager);
    return imp;
}

WMBusSimulator::WMBusSimulator(string file, SerialCommunicationManager *manager)
    : file_(file), manager_(manager)
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

LinkMode WMBusSimulator::getLinkMode() {
    verbose("(simulator) get link mode\n");
    verbose("(simulator) config: link mode %02x\n", link_mode_);
    return link_mode_;
}

void WMBusSimulator::setLinkMode(LinkMode lm)
{
    if (lm != LinkModeC1 && lm != LinkModeT1) {
        error("LinkMode %d is not implemented\n", (int)lm);
    }
    link_mode_ = lm;
    verbose("(simulator) set link mode %02x\n", lm);
    verbose("(simulator) set link mode completed\n");
}

void WMBusSimulator::onTelegram(function<void(Telegram*)> cb) {
    telegram_listeners_.push_back(cb);
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
    for (auto l : lines_) {
        string hex = "";
        if (l.substr(0,9) == "telegram=") {
            for (size_t i=9; i<l.length(); ++i) {
                if (l[i] == '|') continue;
                hex += l[i];
            }
            verbose("(simulator) from file \"%s\"\n", hex.c_str());
        } else {
            continue;
        }

        vector<uchar> payload;
        bool ok = hex2bin(hex.c_str(), &payload);
        if (!ok) {
            error("Not a valid string of hex bytes! \"%s\"\n", l.c_str());
        }
        Telegram t;
        t.parse(payload);
        for (auto f : telegram_listeners_) {
            if (f) f(&t);
        }
        if (isVerboseEnabled() && !t.handled) {
            verbose("(wmbus simulator) telegram ignored by all configured meters!\n");
        }
    }
    manager_->stop();
}

bool detectSimulator(string device, SerialCommunicationManager *manager)
{
    return true;
}
