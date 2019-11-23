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
#include"wmbus_cul.h"
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

enum FrameStatus { PartialFrame, FullFrame, ErrorInFrame, TextAndNotFrame };

struct WMBusCUL : public WMBus
{
    bool ping();
    uint32_t getDeviceId();
    LinkModeSet getLinkModes();
    void setLinkModes(LinkModeSet lms);
    LinkModeSet supportedLinkModes() {
        return
            C1_bit |
            T1_bit;
    }
    int numConcurrentLinkModes() { return 2; }
    bool canSetLinkModes(LinkModeSet lms)
    {
        if (!supportedLinkModes().supports(lms)) return false;
        // The cul listens to both modes always.
        return true;
    }
    void onTelegram(function<void(Telegram*)> cb);

    void processSerialData();
    SerialDevice *serial() { return serial_.get(); }
    void simulate();

    WMBusCUL(unique_ptr<SerialDevice> serial, SerialCommunicationManager *manager);
    ~WMBusCUL() { }

private:
    unique_ptr<SerialDevice> serial_;
    SerialCommunicationManager *manager_;
    vector<uchar> read_buffer_;
    vector<uchar> received_payload_;
    vector<function<void(Telegram*)>> telegram_listeners_;

    FrameStatus checkCULFrame(vector<uchar> &data,
                                   size_t *hex_frame_length,
                                   int *hex_payload_len_out,
                                   int *hex_payload_offset);
    void handleMessage(vector<uchar> &frame);

    string setup_;
};

unique_ptr<WMBus> openCUL(string device, SerialCommunicationManager *manager, unique_ptr<SerialDevice> serial_override)
{
    if (serial_override)
    {
        WMBusCUL *imp = new WMBusCUL(std::move(serial_override), manager);
        return unique_ptr<WMBus>(imp);
    }

    auto serial = manager->createSerialDeviceTTY(device.c_str(), 38400);
    WMBusCUL *imp = new WMBusCUL(std::move(serial), manager);
    return unique_ptr<WMBus>(imp);
}

WMBusCUL::WMBusCUL(unique_ptr<SerialDevice> serial, SerialCommunicationManager *manager) :
    serial_(std::move(serial)), manager_(manager)
{
    manager_->listenTo(serial_.get(),call(this,processSerialData));
    serial_->open(true);
}

bool WMBusCUL::ping()
{
    verbose("(cul) ping\n");
    return true;
}

uint32_t WMBusCUL::getDeviceId()
{
    verbose("(cul) getDeviceId\n");
    return 0x11111111;
}

LinkModeSet WMBusCUL::getLinkModes()
{
    return Any_bit;
}

void WMBusCUL::setLinkModes(LinkModeSet lm)
{
    verbose("(cul) setLinkModes\n");

    // 'brc' command: b - wmbus, r - receive, c - c mode (with t)
    vector<uchar> msg(5);
    msg[0] = 'b';
    msg[1] = 'r';
    msg[2] = 'c';
    msg[3] = 0xa;
    msg[4] = 0xd;
 
    serial()->send(msg);
    usleep(1000*100);
    
    // TODO: CUL should answer with "CMODE" - check this

    // X01 - start the receiver
    msg[0] = 'X';
    msg[1] = '0';
    msg[2] = '1';
    msg[3] = 0xa;
    msg[4] = 0xd;
 
    serial()->send(msg);
    usleep(1000*100);
}

void WMBusCUL::onTelegram(function<void(Telegram*)> cb) {
    telegram_listeners_.push_back(cb);
}

void WMBusCUL::simulate()
{
}

void WMBusCUL::processSerialData()
{
    vector<uchar> data;

    //verbose("(cul) processSerialData 1\n");

    // Receive and accumulated serial data until a full frame has been received.
    serial_->receive(&data);
    read_buffer_.insert(read_buffer_.end(), data.begin(), data.end());

    size_t frame_length;
    int hex_payload_len, hex_payload_offset;

    //verbose("(cul) processSerialData 2\n");

    for (;;)
    {
        FrameStatus status = checkCULFrame(read_buffer_, &frame_length, &hex_payload_len, &hex_payload_offset);

        if (status == PartialFrame)
        {
            break;
        }
        if (status == TextAndNotFrame)
        {
            // The buffer has already been printed by serial cmd.
            read_buffer_.clear();
            break;
        }
        if (status == ErrorInFrame)
        {
            debug("(cul) error in received message.\n");
            string msg = bin2hex(read_buffer_);
            read_buffer_.clear();
            break;
        }
        if (status == FullFrame)
        {
            vector<uchar> payload;
            if (hex_payload_len > 0)
            {
                vector<uchar> hex;
                hex.insert(hex.end(), read_buffer_.begin()+hex_payload_offset, read_buffer_.begin()+hex_payload_offset+hex_payload_len);
                bool ok = hex2bin(hex, &payload);
                if (!ok)
                {
                    if (hex.size() % 2 == 1)
                    {
                        payload.clear();
                        warning("(cul) warning: the hex string is not an even multiple of two! Dropping last char.\n");
                        hex.pop_back();
                        ok = hex2bin(hex, &payload);
                    }
                    if (!ok)
                    {
                        warning("(cul) warning: the hex string contains bad characters! Decode stopped partway.\n");
                    }
                }
            }

            read_buffer_.erase(read_buffer_.begin(), read_buffer_.begin()+frame_length);

            handleMessage(payload);
        }
    }
}

void WMBusCUL::handleMessage(vector<uchar> &frame)
{
    Telegram t;
    bool ok = t.parse(frame);

    if (ok)
    {
        bool handled = false;
        for (auto f : telegram_listeners_)
        {
            Telegram copy = t;
            if (f) f(&copy);
            if (copy.handled) handled = true;
        }
        if (isVerboseEnabled() && !handled)
        {
            verbose("(cul) telegram ignored by all configured meters!\n");
        }
    }
}

FrameStatus WMBusCUL::checkCULFrame(vector<uchar> &data,
                                              size_t *hex_frame_length,
                                              int *hex_payload_len_out,
                                              int *hex_payload_offset)
{
    if (data.size() == 0) return PartialFrame;
    size_t eolp = 0;
    // Look for end of line
    for (; eolp < data.size(); ++eolp) {
        if (data[eolp] == '\n') break;
    }
    if (eolp >= data.size()) return PartialFrame;

    // We got a full line, but if it is too short, then
    // there is something wrong. Discard the data.
    if (data.size() < 10) return ErrorInFrame;

    if (data[0] != 'b') {
        // C1 and T1 telegrams should start with a 'b'
        return ErrorInFrame;
    }

    if (data[1] != 'Y') {
	verbose("(cul) T1 telegrams currently not supported\n");
	return ErrorInFrame;
    }

    // we received a full C1 frame, TODO check len

    // skip the crc bytes adjusting the length byte by 2
    data[3] -= 2; 

    // remove 8: 2 ('bY') + 4 (CRC) + 2 (CRLF) and start at 2 
    *hex_frame_length = data.size();
    *hex_payload_len_out = data.size()-8;
    *hex_payload_offset = 2;

    debug("(cul) got full frame\n");
    return FullFrame;
}

bool detectCUL(string device, SerialCommunicationManager *manager)
{
    // Talk to the device and expect a very specific answer.
    auto serial = manager->createSerialDeviceTTY(device.c_str(), 38400);
    bool ok = serial->open(false);
    if (!ok) return false;

    vector<uchar> data;
    // send '-'+CRLF -> should be an unsupported command for CUL
    // it should respond with "? (- is unknown) Use one of ..."
    vector<uchar> crlf(3);
    crlf[0] = '-';
    crlf[1] = 0x0d;
    crlf[2] = 0x0a;

    serial->send(crlf);
    usleep(1000*100);
    serial->receive(&data);
    
    if (data[0] != '?') {
       // no CUL device detected
       serial->close();
       return false;
    }

    data.clear();

    // get the version string: "V 1.67 nanoCUL868" or similar
    vector<uchar> msg(3);
    msg[0] = CMD_GET_VERSION;
    msg[1] = 0x0a;
    msg[2] = 0x0d;

    verbose("(cul) are you there?\n");
    serial->send(msg);
    // Wait for 200ms so that the USB stick have time to prepare a response.
    usleep(1000*200);
    serial->receive(&data);
    string strC(data.begin(), data.end());
    verbose("CUL answered: %s", strC.c_str());

    // TODO: check version string somehow

    serial->close();
    return true;
}

