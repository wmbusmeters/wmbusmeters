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
#include"wmbus_common_implementation.h"
#include"wmbus_utils.h"
#include"serial.h"

#include<assert.h>
#include<fcntl.h>
#include<grp.h>
#include<pthread.h>
#include<semaphore.h>
#include<string.h>
#include<errno.h>
#include<sys/stat.h>
#include<sys/types.h>
#include<unistd.h>

using namespace std;

struct WMBusRTL433 : public virtual WMBusCommonImplementation
{
    bool ping();
    string getDeviceId();
    LinkModeSet getLinkModes();
    void deviceReset();
    void deviceSetLinkModes(LinkModeSet lms);
    LinkModeSet supportedLinkModes() {
        return
            C1_bit |
            T1_bit;
    }
    int numConcurrentLinkModes() { return 2; }
    bool canSetLinkModes(LinkModeSet lms)
    {
        if (!supportedLinkModes().supports(lms)) return false;
        // The rtl433 listens to both modes always.
        return true;
    }

    void processSerialData();
    void simulate();

    WMBusRTL433(shared_ptr<SerialDevice> serial, shared_ptr<SerialCommunicationManager> manager);

private:
    shared_ptr<SerialDevice> serial_;
    vector<uchar> read_buffer_;
    vector<uchar> received_payload_;
    bool warning_dll_len_printed_ {};

    FrameStatus checkRTL433Frame(vector<uchar> &data,
                                   size_t *hex_frame_length,
                                   int *hex_payload_len_out,
                                   int *hex_payload_offset);
    void handleMessage(vector<uchar> &frame);

    string setup_;
};

shared_ptr<WMBus> openRTL433(string identifier, string command, shared_ptr<SerialCommunicationManager> manager,
                             function<void()> on_exit, shared_ptr<SerialDevice> serial_override)
{
    assert(identifier != "");
    vector<string> args;
    vector<string> envs;
    args.push_back("-c");
    args.push_back(command);
    if (serial_override)
    {
        WMBusRTL433 *imp = new WMBusRTL433(serial_override, manager);
        return shared_ptr<WMBus>(imp);
    }
    auto serial = manager->createSerialDeviceCommand(identifier, "/bin/sh", args, envs, on_exit, "rtl433");
    WMBusRTL433 *imp = new WMBusRTL433(serial, manager);
    return shared_ptr<WMBus>(imp);
}

WMBusRTL433::WMBusRTL433(shared_ptr<SerialDevice> serial, shared_ptr<SerialCommunicationManager> manager) :
    WMBusCommonImplementation(DEVICE_RTL433, manager, serial)
{
    reset();
}

bool WMBusRTL433::ping()
{
    return true;
}

string WMBusRTL433::getDeviceId()
{
    return "?";
}

LinkModeSet WMBusRTL433::getLinkModes()
{

    return Any_bit;
}

void WMBusRTL433::deviceReset()
{
}

void WMBusRTL433::deviceSetLinkModes(LinkModeSet lm)
{
}

void WMBusRTL433::simulate()
{
}

void WMBusRTL433::processSerialData()
{
    vector<uchar> data;

    // Receive and accumulated serial data until a full frame has been received.
    serial()->receive(&data);
    read_buffer_.insert(read_buffer_.end(), data.begin(), data.end());

    size_t frame_length;
    int hex_payload_len, hex_payload_offset;

    for (;;)
    {
        FrameStatus status = checkRTL433Frame(read_buffer_, &frame_length, &hex_payload_len, &hex_payload_offset);

        if (status == PartialFrame)
        {
            break;
        }
        if (status == TextAndNotFrame)
        {
            // The buffer has already been printed by serial cmd.
            read_buffer_.erase(read_buffer_.begin(), read_buffer_.begin()+frame_length);
            if (read_buffer_.size() == 0)
            {
                break;
            }
            else
            {
                // Decode next line.
                continue;
            }
        }
        if (status == ErrorInFrame)
        {
            debug("(rtl433) error in received message.\n");
            read_buffer_.erase(read_buffer_.begin(), read_buffer_.begin()+frame_length);
            if (read_buffer_.size() == 0)
            {
                break;
            }
            else
            {
                // Decode next line.
                continue;
            }
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
                        warning("(rtl433) warning: the hex string is not an even multiple of two! Dropping last char.\n");
                        hex.pop_back();
                        ok = hex2bin(hex, &payload);
                    }
                    if (!ok)
                    {
                        warning("(rtl433) warning: the hex string contains bad characters! Decode stopped partway.\n");
                    }
                }
            }

            read_buffer_.erase(read_buffer_.begin(), read_buffer_.begin()+frame_length);
            if (payload.size() > 0)
            {
                if (payload[0] != payload.size()-1)
                {
                    if (!warning_dll_len_printed_)
                    {
                        warning("(rtl433) dll_len adjusted to %d from %d. Fix rtl_433? This warning will not be printed again.\n", payload.size()-1, payload[0]);
                        warning_dll_len_printed_ = true;
                    }
                    payload[0] = payload.size()-1;
                }
            }
            AboutTelegram about("", 0);
            handleTelegram(about, payload);
        }
    }
}

FrameStatus WMBusRTL433::checkRTL433Frame(vector<uchar> &data,
                                          size_t *hex_frame_length,
                                          int *hex_payload_len_out,
                                          int *hex_payload_offset)
{
    // 2020-08-10 20:40:47,,,Wireless-MBus,,22232425,,,,CRC,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,25442d2c252423221b168d209f38810821c3f371825d5c25b5bdea9821786aec9e2d,,,,,22,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,C,27,Cold Water,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,

    if (data.size() == 0) return PartialFrame;

    if (isDebugEnabled())
    {
        string msg = safeString(data);
        debug("(rtl433) checkRTL433Frame \"%s\"\n", msg.c_str());
    }

    int payload_len = 0;
    size_t eolp = 0;
    // Look for end of line
    for (; eolp < data.size(); ++eolp)
    {
        if (data[eolp] == '\n')
        {
            data[eolp] = 0;
            break;
        }
    }
    if (eolp >= data.size())
    {
        return PartialFrame;
    }

    *hex_frame_length = eolp+1;

    const char *needle = strstr((const char*)&data[0], "Wireless-MBus");
    if (needle == NULL)
    {
        // rtl_433 found some other protocol on 868.95Mhz
        return TextAndNotFrame;
    }

    // Look for start of telegram ,..44..........,
    // This works right now because wmbusmeters currently only listens for 44 SND_NR
    size_t i = 0;
    for (; i+4 < data.size(); ++i) {
        if (data[i] == ',' && data[i+3] == '4' && data[i+4] == '4')
        {
            size_t j = i+1;
            for (; j<data.size(); ++j)
            {
                if (data[j] == ',') break;
            }
            if (j-i < 20)
            {
                // Oups, too short sequence of hex chars, this cannot be a proper telegram.
                continue;
            }
            break;
        }
    }

    if (i+4 >= data.size())
    {
        return ErrorInFrame; // No ,  44 found, then discard the frame.
    }
    i+=1; // Skip the comma ,

    // Look for end of line or semicolon.
    size_t nextcomma = i;
    for (; nextcomma < data.size(); ++nextcomma) {
        if (data[nextcomma] == ',') break;
    }
    if (nextcomma >= data.size())
    {
        return PartialFrame;
    }

    payload_len = nextcomma-i;
    *hex_payload_len_out = payload_len;
    *hex_payload_offset = i;

    return FullFrame;
}

AccessCheck detectRTL433(Detected *detected, shared_ptr<SerialCommunicationManager> handler)
{
    detected->setAsFound("", WMBusDeviceType::DEVICE_RTLWMBUS, 0, false, false);

    return AccessCheck::AccessOK;
}
