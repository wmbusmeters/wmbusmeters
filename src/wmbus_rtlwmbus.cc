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
#include<grp.h>
#include<pthread.h>
#include<semaphore.h>
#include<string.h>
#include<errno.h>
#include<sys/stat.h>
#include<sys/types.h>
#include<unistd.h>

using namespace std;

struct WMBusRTLWMBUS : public virtual WMBusCommonImplementation
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
        // The rtlwmbus listens to both modes always.
        return true;
    }

    void processSerialData();
    void simulate();

    WMBusRTLWMBUS(string serialnr, shared_ptr<SerialDevice> serial, shared_ptr<SerialCommunicationManager> manager);
    ~WMBusRTLWMBUS() { }

private:

    string serialnr_;
    vector<uchar> read_buffer_;
    vector<uchar> received_payload_;
    bool warning_dll_len_printed_ {};

    FrameStatus checkRTLWMBUSFrame(vector<uchar> &data,
                                   size_t *hex_frame_length,
                                   int *hex_payload_len_out,
                                   int *hex_payload_offset,
                                   double *rssi);
    void handleMessage(vector<uchar> &frame);

    string setup_;
};

shared_ptr<WMBus> openRTLWMBUS(string serialnr, string command, shared_ptr<SerialCommunicationManager> manager,
                               function<void()> on_exit, shared_ptr<SerialDevice> serial_override)
{
    debug("(rtlwmbus) opening %s\n", serialnr.c_str());

    vector<string> args;
    vector<string> envs;
    args.push_back("-c");
    args.push_back(command);
    if (serial_override)
    {
        WMBusRTLWMBUS *imp = new WMBusRTLWMBUS(serialnr, serial_override, manager);
        imp->markSerialAsOverriden();
        return shared_ptr<WMBus>(imp);
    }
    auto serial = manager->createSerialDeviceCommand(serialnr, "/bin/sh", args, envs, on_exit, "rtlwmbus");
    WMBusRTLWMBUS *imp = new WMBusRTLWMBUS(serialnr, serial, manager);
    return shared_ptr<WMBus>(imp);
}

WMBusRTLWMBUS::WMBusRTLWMBUS(string serialnr, shared_ptr<SerialDevice> serial, shared_ptr<SerialCommunicationManager> manager) :
    WMBusCommonImplementation(DEVICE_RTLWMBUS, manager, serial), serialnr_(serialnr)
{
    reset();
}

bool WMBusRTLWMBUS::ping()
{
    return true;
}

string WMBusRTLWMBUS::getDeviceId()
{
    return serialnr_;
}

LinkModeSet WMBusRTLWMBUS::getLinkModes()
{

    return Any_bit;
}

void WMBusRTLWMBUS::deviceReset()
{
}

void WMBusRTLWMBUS::deviceSetLinkModes(LinkModeSet lm)
{
}

void WMBusRTLWMBUS::simulate()
{
}

void WMBusRTLWMBUS::processSerialData()
{
    vector<uchar> data;

    // Receive and accumulated serial data until a full frame has been received.
    serial()->receive(&data);
    read_buffer_.insert(read_buffer_.end(), data.begin(), data.end());

    size_t frame_length;
    int hex_payload_len, hex_payload_offset;

    for (;;)
    {
        double rssi = 0;
        FrameStatus status = checkRTLWMBUSFrame(read_buffer_, &frame_length, &hex_payload_len, &hex_payload_offset, &rssi);

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
            debug("(rtlwmbus) error in received message.\n");
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
                        warning("(rtlwmbus) warning: the hex string is not an even multiple of two! Dropping last char.\n");
                        hex.pop_back();
                        ok = hex2bin(hex, &payload);
                    }
                    if (!ok)
                    {
                        warning("(rtlwmbus) warning: the hex string contains bad characters! Decode stopped partway.\n");
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
                        warning("(rtlwmbus) dll_len adjusted to %d from %d. Upgrade rtl_wmbus? This warning will not be printed again.\n", payload.size()-1, payload[0]);
                        warning_dll_len_printed_ = true;
                    }
                    payload[0] = payload.size()-1;
                }
            }

            string id = string("rtlwmbus[")+getDeviceId()+"]";
            AboutTelegram about(id, rssi);
            handleTelegram(about, payload);
        }
    }
}

FrameStatus WMBusRTLWMBUS::checkRTLWMBUSFrame(vector<uchar> &data,
                                              size_t *hex_frame_length,
                                              int *hex_payload_len_out,
                                              int *hex_payload_offset,
                                              double *rssi)
{
    // C1;1;1;2019-02-09 07:14:18.000;117;102;94740459;0x49449344590474943508780dff5f3500827f0000f10007b06effff530100005f2c620100007f2118010000008000800080008000000000000000000e003f005500d4ff2f046d10086922
    // There might be a second telegram on the same line ;0x4944.......
    if (data.size() == 0) return PartialFrame;

    if (isDebugEnabled())
    {
        string msg = safeString(data);
        debug("(rtlwmbus) checkRTLWMBusFrame \"%s\"\n", msg.c_str());
    }

    int payload_len = 0;
    size_t eolp = 0;
    // Look for end of line
    for (; eolp < data.size(); ++eolp) {
        if (data[eolp] == '\n') break;
    }
    if (eolp >= data.size())
    {
        debug("(rtlwmbus) no eol found, partial frame\n");
        return PartialFrame;
    }

    // We got a full line, but if it is too short, then
    // there is something wrong. Discard the data.
    if (data.size() < 10)
    {

        debug("(rtlwmbus) too short line\n");
        return ErrorInFrame;
    }

    if (data[0] != '0' || data[1] != 'x')
    {
        // Discard lines that do not begin with T1 or C1, these lines are probably
        // stderr output from rtl_sdr/rtl_wmbus.
        if (!(data[0] == 'T' && data[1] == '1') &&
            !(data[0] == 'C' && data[1] == '1'))
        {

            debug("(rtlwmbus) only text\n");
            return TextAndNotFrame;
        }

        // And the checksums should match.
        if (strncmp((const char*)&data[1], "1;1", 3))
        {
            // Packages that begin with C1;1 or with T1;1 are good. The full format is:
            // MODE;CRC_OK;3OUTOF6OK;TIMESTAMP;PACKET_RSSI;CURRENT_RSSI;LINK_LAYER_IDENT_NO;DATAGRAM_WITHOUT_CRC_BYTES.
            // 3OUTOF6OK makes sense only with mode T1 and no sense with mode C1 (always set to 1).
            if (!strncmp((const char*)&data[1], "1;0", 3)) {
                verbose("(rtlwmbus) telegram received but incomplete or with errors, since rtl_wmbus reports that CRC checks failed.\n");
            }
            return ErrorInFrame;
        }
    }
    size_t i = 0;
    int count = 0;
    // Look for packet rssi
    for (; i+1 < data.size(); ++i) {
        if (data[i] == ';') count++;
        if (count == 4) break;
    }
    if (count == 4)
    {
        size_t from = i+1;
        for (i++; i<data.size(); ++i) {
            if (data[i] == ';') break;
        }
        if ((i-from)<5)
        {
            string rssis = string(data.begin()+from,data.begin()+i);
            *rssi = atof(rssis.c_str());
        }
    }

    // Look for start of telegram 0x
    for (; i+1 < data.size(); ++i) {
        if (data[i] == '0' && data[i+1] == 'x') break;
    }
    if (i+1 >= data.size())
    {
        return ErrorInFrame; // No 0x found, then discard the frame.
    }
    i+=2; // Skip 0x

    // Look for end of line or semicolon.
    for (eolp=i; eolp < data.size(); ++eolp) {
        if (data[eolp] == '\n') break;
        if (data[eolp] == ';' && data[eolp+1] == '0' && data[eolp+2] == 'x') break;
    }
    if (eolp >= data.size())
    {
        debug("(rtlwmbus) no eol or semicolon, partial frame\n");
        return PartialFrame;
    }

    payload_len = eolp-i;
    *hex_payload_len_out = payload_len;
    *hex_payload_offset = i;
    *hex_frame_length = eolp+1;

    debug("(rtlwmbus) received full frame\n");
    return FullFrame;
}

AccessCheck detectRTLWMBUS(Detected *detected, shared_ptr<SerialCommunicationManager> handler)
{
    assert(0);
    return AccessCheck::NotThere;
}
