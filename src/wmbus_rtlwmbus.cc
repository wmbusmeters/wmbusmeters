/*
 Copyright (C) 2019 Fredrik Öhrström

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

enum FrameStatus { PartialFrame, FullFrame, ErrorInFrame };

struct WMBusRTLWMBUS : public WMBus {
    bool ping();
    uint32_t getDeviceId();
    LinkMode getLinkMode();
    void setLinkMode(LinkMode lm);
    void onTelegram(function<void(Telegram*)> cb);

    void processSerialData();
    SerialDevice *serial() { return NULL; }
    void simulate();

    WMBusRTLWMBUS(unique_ptr<SerialDevice> serial, SerialCommunicationManager *manager);

private:
    unique_ptr<SerialDevice> serial_;
    vector<uchar> read_buffer_;
    vector<uchar> received_payload_;
    vector<function<void(Telegram*)>> telegram_listeners_;

    FrameStatus checkRTLWMBUSFrame(vector<uchar> &data,
                                   size_t *hex_frame_length,
                                   int *hex_payload_len_out,
                                   int *hex_payload_offset);
    void handleMessage(vector<uchar> &frame);

    string setup_;
    SerialCommunicationManager *manager_ {};
};

unique_ptr<WMBus> openRTLWMBUS(string command, SerialCommunicationManager *manager,
                               function<void()> on_exit)
{
    vector<string> args;
    vector<string> envs;
    args.push_back("-c");
    args.push_back(command);
    auto serial = manager->createSerialDeviceCommand("/bin/sh", args, envs, on_exit);
    WMBusRTLWMBUS *imp = new WMBusRTLWMBUS(std::move(serial), manager);
    return unique_ptr<WMBus>(imp);
}

WMBusRTLWMBUS::WMBusRTLWMBUS(unique_ptr<SerialDevice> serial, SerialCommunicationManager *manager) :
    serial_(std::move(serial)), manager_(manager)
{
    manager_->listenTo(serial_.get(),call(this,processSerialData));
    serial_->open(true);
}

bool WMBusRTLWMBUS::ping() {
    return true;
}

uint32_t WMBusRTLWMBUS::getDeviceId() {
    return 0x11111111;
}

LinkMode WMBusRTLWMBUS::getLinkMode() {

    return LinkMode::Any;
}

void WMBusRTLWMBUS::setLinkMode(LinkMode lm)
{
}

void WMBusRTLWMBUS::onTelegram(function<void(Telegram*)> cb) {
    telegram_listeners_.push_back(cb);
}

void WMBusRTLWMBUS::simulate()
{
}

void WMBusRTLWMBUS::processSerialData()
{
    vector<uchar> data;

    // Receive and accumulated serial data until a full frame has been received.
    serial_->receive(&data);

    read_buffer_.insert(read_buffer_.end(), data.begin(), data.end());

    size_t frame_length;
    int hex_payload_len, hex_payload_offset;

    FrameStatus status = checkRTLWMBUSFrame(read_buffer_, &frame_length, &hex_payload_len, &hex_payload_offset);

    if (status == ErrorInFrame) {
        verbose("(rtl_wmbus) protocol error in message received!\n");
        string msg = bin2hex(read_buffer_);
        debug("(rtl_wmbus) protocol error \"%s\"\n", msg.c_str());
        read_buffer_.clear();
    } else
    if (status == FullFrame) {
        vector<uchar> payload;
        if (hex_payload_len > 0) {
            vector<uchar> hex;
            hex.insert(hex.end(), read_buffer_.begin()+hex_payload_offset, read_buffer_.begin()+hex_payload_offset+hex_payload_len);
            hex2bin(hex, &payload);
        }

        read_buffer_.erase(read_buffer_.begin(), read_buffer_.begin()+frame_length);
        handleMessage(payload);
    }
}

void WMBusRTLWMBUS::handleMessage(vector<uchar> &frame)
{
    Telegram t;
    t.parse(frame);
    for (auto f : telegram_listeners_)
    {
        if (f) f(&t);
        if (isVerboseEnabled() && !t.handled) {
            verbose("(rtl_wmbus) telegram ignored by all configured meters!\n");
        }
    }
}

FrameStatus WMBusRTLWMBUS::checkRTLWMBUSFrame(vector<uchar> &data,
                                              size_t *hex_frame_length,
                                              int *hex_payload_len_out,
                                              int *hex_payload_offset)
{
    //C1;1;1;2019-02-09 07:14:18.000;117;102;94740459;0x49449344590474943508780dff5f3500827f0000f10007b06effff530100005f2c620100007f2118010000008000800080008000000000000000000e003f005500d4ff2f046d10086922
    if (data.size() == 0) return PartialFrame;
    int payload_len = 0;
    size_t eolp = 0;
    // Look for end of line.
    for (; eolp < data.size(); ++eolp) {
        if (data[eolp] == '\n') break;
    }
    if (eolp >= data.size()) return PartialFrame;

    // We got a full line, but if it is too short, then
    // there is something wrong. Discard the data.
    if (data.size() < 72) return ErrorInFrame;

    // Discard lines that are not T1 or C1 telegrams
    if (data[0] != 'T' && data[0] != 'C') return ErrorInFrame;

    // And the checksums should match.
    if (strncmp((const char*)&data[1], "1;1;1", 5)) return ErrorInFrame;

    // Look for start of telegram 0x
    size_t i = 0;
    for (; i+1 < data.size(); ++i) {
        if (data[i] == '0' && data[i+1] == 'x') break;
    }
    if (i+1 >= data.size()) return ErrorInFrame; // No 0x found, then discard the frame.
    i+=2; // Skip 0x

    payload_len = eolp-i;
    *hex_payload_len_out = payload_len;
    *hex_payload_offset = i;
    *hex_frame_length = eolp+1;

    return FullFrame;
}

bool detectRTLSDR(string device, SerialCommunicationManager *manager)
{
    // No more advanced test than that the /dev/rtlsdr link exists.
    AccessCheck ac = checkIfExistsAndSameGroup(device);
    return ac == AccessCheck::OK;
}
