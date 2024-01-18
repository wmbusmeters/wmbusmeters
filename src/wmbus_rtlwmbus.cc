/*
 Copyright (C) 2019-2021 Fredrik Öhrström (gpl-3.0-or-later)

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
#include"rtlsdr.h"
#include"serial.h"
#include"shell.h"

#include<assert.h>
#include<algorithm>
#include<fcntl.h>
#include<grp.h>
#include<pthread.h>
#include<semaphore.h>
#include<string.h>
#include<errno.h>
#include<sys/stat.h>
#include<sys/types.h>
#include<time.h>
#include<unistd.h>

using namespace std;

struct WMBusRTLWMBUS : public virtual BusDeviceCommonImplementation
{
    bool ping();
    string getDeviceId();
    string getDeviceUniqueId();
    LinkModeSet getLinkModes();
    void deviceReset();
    bool deviceSetLinkModes(LinkModeSet lms);
    LinkModeSet supportedLinkModes() {
        return
            C1_bit |
            S1_bit |
            T1_bit;
    }
    int numConcurrentLinkModes() { return 3; }
    bool canSetLinkModes(LinkModeSet lms)
    {
        //if (!supportedLinkModes().supports(lms)) return false;
        // The rtlwmbus listens to both modes always.
        return true;
    }

    void processSerialData();
    void simulate();

    WMBusRTLWMBUS(string alias, string serialnr, shared_ptr<SerialDevice> serial, shared_ptr<SerialCommunicationManager> manager);
    ~WMBusRTLWMBUS() { }

private:

    string serialnr_;
    vector<uchar> read_buffer_;
    vector<uchar> received_payload_;
    bool warning_dll_len_printed_ {};

    LinkModeSet device_link_modes_;

    FrameStatus checkRTLWMBUSFrame(vector<uchar> &data,
                                   size_t *hex_frame_length,
                                   int *hex_payload_len_out,
                                   int *hex_payload_offset,
                                   double *rssi,
                                   struct tm *timestamp);
    void handleMessage(vector<uchar> &frame);

    string setup_;
};

shared_ptr<BusDevice> openRTLWMBUS(Detected detected,
                               string bin_dir,
                               bool daemon,
                               shared_ptr<SerialCommunicationManager> manager,
                               shared_ptr<SerialDevice> serial_override)
{
    string bus_alias = detected.specified_device.bus_alias;
    string identifier = detected.found_device_id;
    SpecifiedDevice &device = detected.specified_device;
    string command;
    int id = 0;
    map<string,string> extras;

    bool ok = parseExtras(detected.specified_device.extras, &extras);
    if (!ok)
    {
        error("(rtlwmbus) invalid extra parameters to rtlwmbus (%s)\n", detected.specified_device.extras.c_str());
    }
    string ppm = "";
    if (extras.size() > 0)
    {
        if (extras.count("ppm") > 0)
        {
            ppm = string("-p ")+extras["ppm"];
        }
    }
    if (!serial_override)
    {
        id = indexFromRtlSdrSerial(identifier);

        command = "";
        if (device.command != "")
        {
            command = device.command;
            identifier = "cmd_"+to_string(device.index);
        }
        string freq = "868.625M";
        bool force_freq = false;
        if (device.fq != "")
        {
            freq = device.fq;
            // This will disable the listen to s1,t1 and c1 at the same time setting.
            force_freq = true;
        }
        string rtl_sdr;
        string rtl_wmbus;
        rtl_sdr = lookForExecutable("rtl_sdr", bin_dir, "/usr/bin");
        if (rtl_sdr == "")
        {
            if (daemon)
            {
                error("(rtlwmbus) error: when starting as daemon, wmbusmeters looked for %s/rtl_sdr and %s/rtl_sdr, but found neither!\n",
                      bin_dir.c_str(), "/usr/bin");
            }
            else
            {
                // Look for it in the PATH
                rtl_sdr = "rtl_sdr";
            }
        }
        rtl_wmbus = lookForExecutable("rtl_wmbus", bin_dir, "/usr/bin");
        if (rtl_wmbus == "")
        {
            if (daemon)
            {
                error("(rtlwmbus) error: when starting as daemon, wmbusmeters looked for %s/rtl_wmbus and %s/rtl_wmbus, but found neither!\n",
                      bin_dir.c_str(), "/usr/bin");
            }
            else
            {
                // Look for it in the PATH
                rtl_wmbus = "rtl_wmbus";
            }
        }

        string out;
        invokeShellCaptureOutput(rtl_wmbus, { "--help" }, {}, &out, true);
        debug("(rtlwmbus) help %s\n", out.c_str());
        string add_f = "";
        if (out.find("-f exit if flow") != string::npos)
        {
            add_f = " -f";
        }
        else
        {
            warning("Warning! rtl_wbus executable lacks -f option! Without this option rtl_wmbus cannot detect when rtl-sdr stops working.\n"
                    "Please upgrade rtl_wmbus.\n");
        }

        if (command == "")
        {
            if (!force_freq)
            {
                command = "ERRFILE=$(mktemp --suffix=_wmbusmeters_rtlsdr) ; echo ERRFILE=$ERRFILE ;  date -Iseconds > $ERRFILE ; tail -f $ERRFILE & "+rtl_sdr+" "+ppm+" -d "+to_string(id)+" -f "+freq+" -s 1.6e6 - 2>>$ERRFILE | "+rtl_wmbus+" -s"+add_f;
            }
            else
            {
                command = "ERRFILE=$(mktemp --suffix=_wmbusmeters_rtlsdr) ; echo ERRFILE=$ERRFILE ; date -Iseconds > $ERRFILE ; tail -f $ERRFILE & "+rtl_sdr+" "+ppm+" -d "+to_string(id)+" -f "+freq+" -s 1.6e6 - 2>>$ERRFILE | "+rtl_wmbus+" "+add_f;
            }
        }
        verbose("(rtlwmbus) using command: %s\n", command.c_str());
    }

    debug("(rtlwmbus) opening %s\n", identifier.c_str());

    vector<string> args;
    vector<string> envs;
    args.push_back("-c");
    args.push_back(command);
    if (serial_override)
    {
        WMBusRTLWMBUS *imp = new WMBusRTLWMBUS(bus_alias, identifier, serial_override, manager);
        imp->markSerialAsOverriden();
        return shared_ptr<BusDevice>(imp);
    }
    auto serial = manager->createSerialDeviceCommand(identifier, "/bin/sh", args, envs, "rtlwmbus");
    WMBusRTLWMBUS *imp = new WMBusRTLWMBUS(bus_alias, identifier, serial, manager);
    return shared_ptr<BusDevice>(imp);
}

WMBusRTLWMBUS::WMBusRTLWMBUS(string alias, string serialnr, shared_ptr<SerialDevice> serial, shared_ptr<SerialCommunicationManager> manager) :
    BusDeviceCommonImplementation(alias, DEVICE_RTLWMBUS, manager, serial, false), serialnr_(serialnr)
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

string WMBusRTLWMBUS::getDeviceUniqueId()
{
    return "?";
}

LinkModeSet WMBusRTLWMBUS::getLinkModes()
{
    return device_link_modes_;
}

void WMBusRTLWMBUS::deviceReset()
{
}

bool WMBusRTLWMBUS::deviceSetLinkModes(LinkModeSet lm)
{
    LinkModeSet lms;
    lms.addLinkMode(LinkMode::C1);
    lms.addLinkMode(LinkMode::T1);
    device_link_modes_ = lms;
    return true;
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
    struct tm timestamp;

    for (;;)
    {
        double rssi = 0;
        FrameStatus status = checkRTLWMBUSFrame(read_buffer_, &frame_length, &hex_payload_len, &hex_payload_offset, &rssi, &timestamp);

        if (status == PartialFrame)
        {
            break;
        }
        else if (status == TextAndNotFrame)
        {
            const char *exit_message = "rtl_wmbus: exiting";
            auto end = read_buffer_.begin()+frame_length;
            auto it = std::search(read_buffer_.begin(),
                                  end,
                                  exit_message,
                                  exit_message + strlen(exit_message));
            if (it != end)
            {
                warning("Warning! Detected rtl_wmbus exit due to stopped data flow. Resetting pipeline!\n");
                reset();
            }
            // The buffer has already been printed by serial cmd.
            read_buffer_.erase(read_buffer_.begin(), read_buffer_.begin()+frame_length);
        }
        else if (status == ErrorInFrame)
        {
            debug("(rtlwmbus) error in received message.\n");
            read_buffer_.erase(read_buffer_.begin(), read_buffer_.begin()+frame_length);
        }
        else if (status == FullFrame)
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
            AboutTelegram about(id, rssi, FrameType::WMBUS, timestamp.tm_mday ? timegm(&timestamp) : 0);
            handleTelegram(about, payload);
        }
        else
        {
            assert(0);
        }
    }
}

FrameStatus WMBusRTLWMBUS::checkRTLWMBUSFrame(vector<uchar> &data,
                                              size_t *hex_frame_length,
                                              int *hex_payload_len_out,
                                              int *hex_payload_offset,
                                              double *rssi,
                                              struct tm *timestamp)
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

    // Export how long the current line is, so that it can be removed from the buffer.
    *hex_frame_length = eolp+1;

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
            !(data[0] == 'C' && data[1] == '1') &&
            !(data[0] == 'S' && data[1] == '1'))
        {

            debug("(rtlwmbus) only text\n");
            return TextAndNotFrame;
        }

        // And the checksums should match.
        if (strncmp((const char*)&data[1], "1;1", 3))
        {
            // Packages that begin with C1;1 or with T1;1 or with S1;1 are good. The full format is:
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
    *timestamp = { 0 };
    // Look for packet timestamp and rssi
    for (; i+1 < data.size(); ++i) {
        if (data[i] != ';') continue;
        count++;
        if (count == 3 && !strptime((const char*)&data[i+1], "%Y-%m-%d %H:%M:%S", timestamp)) {
            debug("(rtlwmbus) invalid timestamp\n");
            return ErrorInFrame;
        }
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

    debug("(rtlwmbus) received full frame\n");
    return FullFrame;
}

AccessCheck detectRTLWMBUS(Detected *detected, shared_ptr<SerialCommunicationManager> handler)
{
    assert(0);
    return AccessCheck::NoSuchDevice;
}
