/*
 Copyright (C) 2020 Fredrik Öhrström (gpl-3.0-or-later)

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
#include<stdexcept>

using namespace std;

const int DEFAULT_BAUD_RATE = 19200;

enum class RcUartBaudRate : uchar
{
    b2400 = 1,
    b4800 = 2,
    b9600 = 3,
    b14400 = 4,
    b19200 = 5,
    b28800 = 6,
    b38400 = 7,
    b57600 = 8,
    b76800 = 9,
    b115200 = 10,
    b230400 = 11,
};

static RcUartBaudRate rcUartBaudRateFromBauds(int baud_rate)
{
    switch (baud_rate)
    {
        case 2400: return RcUartBaudRate::b2400;
        case 4800: return RcUartBaudRate::b4800;
        case 9600: return RcUartBaudRate::b9600;
        case 14400: return RcUartBaudRate::b14400;
        case 19200: return RcUartBaudRate::b19200;
        case 28800: return RcUartBaudRate::b28800;
        case 38400: return RcUartBaudRate::b38400;
        case 57600: return RcUartBaudRate::b57600;
        case 76800: return RcUartBaudRate::b76800;
        case 115200: return RcUartBaudRate::b115200;
        case 230400: return RcUartBaudRate::b230400;
    }
    throw std::invalid_argument("Unable to convert baud_rate: " + std::to_string(baud_rate) + " to RC enum");
}

static int getConfiguredBaudRate(const Detected& d) noexcept
try
{
    if (d.specified_device.bps.empty())
    {
        return DEFAULT_BAUD_RATE;
    }
    const int result = stoi(d.specified_device.bps);
    info("(rc1180) Boud rate overwritten to %i", result);
    return result;
}
catch(const std::exception& e)
{
    warning("(rc1180) Unable to convert baud_rate: \"%s\" to int: %s - using default",
            d.specified_device.bps.c_str(), e.what());
    return DEFAULT_BAUD_RATE;
}

struct ConfigRC1180
{
    // first variable group
    uchar radio_channel {}; // S=11 T1=12 R2=1-10
    uchar radio_power {};
    uchar radio_data_rate {}; // S=2 T1=3 R2=1
    uchar mbus_mode {}; // S1=0 T1=1
    uchar sleep_mode {}; // 0=disable sleep 1=enable sleep
    uchar rssi_mode {}; // 0=disabled 1=enabled (append rssi to telegram)

    uchar preamble_length {}; // S: 4(short) 70(long) T: 4(meter) 3(other) R: 10

    uint16_t mfct {};
    uint32_t id {};
    uchar version {};
    uchar media {};

    RcUartBaudRate uart_baud_rate {}; // 5=19200
    uchar uart_flow_ctrl {}; // 0=None 1=CTS only 2=CTS/RTS 3=RXTX(RS485)
    uchar data_interface {}; // 0=MBUS with DLL 1=App data without mbus header

    string dongleId()
    {
        return tostrprintf("%08x", id);
    }

    bool usingRssi()
    {
        return rssi_mode == 1;
    }

    string str()
    {
        string mfct_flag = manufacturerFlag(mfct);

        return tostrprintf("id=%08x mfct=%04x (%s) media=%02x version=%02x rssi_mode=%02x "
                           "uart_baud_rate=%02x uart_flow_ctrl=%02x data_interface=%02x "
                           "radio_channel=%02x radio_power=%02x radio_data_rate=%02x preamble_length=%02x mbus_mode=%02x",
                           id, mfct, mfct_flag.c_str(), media, version, rssi_mode,
                           uart_baud_rate, uart_flow_ctrl, data_interface,
                           radio_channel, radio_power, radio_data_rate, preamble_length, mbus_mode);
    }

    bool decode(vector<uchar> &bytes)
    {
        if (bytes.size() != 257) return false;

        // The last byte should be 0x3e.
        if (bytes[256] != 0x3e) return false;
        radio_channel = bytes[0];
        radio_power = bytes[1];
        radio_data_rate = bytes[2];
        mbus_mode = bytes[3];
        sleep_mode = bytes[4];
        rssi_mode = bytes[5];

        preamble_length = bytes[0x0a];

        mfct = bytes[0x1a] << 8 | bytes[0x19];
        id = bytes[0x1e] << 24 | bytes[0x1d] << 16 | bytes[0x1c] << 8 | bytes[0x1b];
        version = bytes[0x1f];
        media = bytes[0x20];

        uart_baud_rate = static_cast<RcUartBaudRate>(bytes[0x30]);
        uart_flow_ctrl = bytes[0x35];
        data_interface = bytes[0x36];

        return true;
    }
};

struct WMBusRC1180 : public virtual BusDeviceCommonImplementation
{
    bool ping();
    string getDeviceId();
    string getDeviceUniqueId();
    LinkModeSet getLinkModes();
    void deviceReset();
    bool deviceSetLinkModes(LinkModeSet lms);
    LinkModeSet supportedLinkModes() {
        return
            T1_bit;
        // This device can be set to S1,S1-m,S2,T1,T2,R2
        // with a combination of radio_channel+radio_data_rate+mbus_mode+preamble_length.
        // However it is unclear from the documentation if these
        // settings are for transmission only or also for listening...?
        // My dongle has mbus_mode=1 and hears T1 telegrams,
        // even though the radio_channel and the preamble_length
        // is wrongfor T1 mode. So I will leave this
        // dongle in default mode, which seems to be T1
        // until someone can double check with an s1 meter.
    }
    int numConcurrentLinkModes() { return 1; }
    bool canSetLinkModes(LinkModeSet lms)
    {
        if (lms.empty()) return false;
        if (!supportedLinkModes().supports(lms)) return false;
        // Ok, the supplied link modes are compatible,
        // but rc1180 can only listen to one at a time.
        return 1 == countSetBits(lms.asBits());
    }
    void processSerialData();
    void simulate();

    WMBusRC1180(string bus_alias, shared_ptr<SerialDevice> serial, shared_ptr<SerialCommunicationManager> manager);
    ~WMBusRC1180() { }

private:
    ConfigRC1180 device_config_;

    vector<uchar> read_buffer_;
    vector<uchar> request_;
    vector<uchar> response_;

    LinkModeSet link_modes_ {};
};

shared_ptr<BusDevice> openRC1180(Detected detected, shared_ptr<SerialCommunicationManager> manager, shared_ptr<SerialDevice> serial_override)
{
    assert(detected.found_file != "");

    string bus_alias = detected.specified_device.bus_alias;
    string device = detected.found_file;
    assert(device != "");

    if (serial_override)
    {
        WMBusRC1180 *imp = new WMBusRC1180(bus_alias, serial_override, manager);
        imp->markAsNoLongerSerial();
        return shared_ptr<BusDevice>(imp);
    }

    auto serial = manager->createSerialDeviceTTY(device.c_str(), getConfiguredBaudRate(detected), PARITY::NONE, "rc1180");
    WMBusRC1180 *imp = new WMBusRC1180(bus_alias, serial, manager);
    return shared_ptr<BusDevice>(imp);
}

WMBusRC1180::WMBusRC1180(string bus_alias, shared_ptr<SerialDevice> serial, shared_ptr<SerialCommunicationManager> manager) :
    BusDeviceCommonImplementation(bus_alias, DEVICE_RC1180, manager, serial, true)
{
    reset();
}

bool WMBusRC1180::ping()
{
    verbose("(rc1180) ping\n");
    return true;
}

string WMBusRC1180::getDeviceId()
{
    if (serial()->readonly()) return "?"; // Feeding from stdin or file.
    if (cached_device_id_ != "") return cached_device_id_;

    bool ok = false;
    string hex;

    LOCK_WMBUS_EXECUTING_COMMAND(getDeviceId);

    serial()->disableCallbacks();

    request_.resize(1);
    request_[0] = 0;

    verbose("(rc1180) get config to get device id\n");

    // Enter config mode.
    ok = serial()->send(request_);
    if (!ok) goto err;

    // Wait for the dongle to enter config mode.
    usleep(200*1000);

    // Config mode active....
    ok = serial()->waitFor('>');
    if (!ok) goto err;

    response_.clear();

    // send config command '0' to get all config data
    request_[0] = '0';

    ok = serial()->send(request_);
    if (!ok) goto err;

    // Wait for the dongle to respond.
    usleep(200*1000);

    ok = serial()->receive(&response_);
    if (!ok) goto err;

    device_config_.decode(response_);

    cached_device_id_ = tostrprintf("%08x", device_config_.id);

    verbose("(rc1180) got device id %s\n", cached_device_id_.c_str());

    // Send config command 'X' to exit config mode.
    request_[0] = 'X';
    ok = serial()->send(request_);
    if (!ok) goto err;

    serial()->enableCallbacks();

    return cached_device_id_;

err:
    serial()->enableCallbacks();

    return "ERR";
}

string WMBusRC1180::getDeviceUniqueId()
{
    return "?";
}

LinkModeSet WMBusRC1180::getLinkModes()
{
    return link_modes_;
}

void WMBusRC1180::deviceReset()
{
    // No device specific settings needed right now.
    // The common code in wmbus.cc reset()
    // will open the serial device and potentially
    // set the link modes properly.
}

bool WMBusRC1180::deviceSetLinkModes(LinkModeSet lms)
{
    if (serial()->readonly()) return true; // Feeding from stdin or file.

    if (!canSetLinkModes(lms))
    {
        string modes = lms.hr();
        error("(rc1180) setting link mode(s) %s is not supported\n", modes.c_str());
    }

    // Do not actually try to change the link mode, we assume it is T1.
    return true;
}

void WMBusRC1180::simulate()
{
}

void WMBusRC1180::processSerialData()
{
    vector<uchar> data;

    // Receive and accumulated serial data until a full frame has been received.
    serial()->receive(&data);

    LOCK_WMBUS_RECEIVING_BUFFER(processSerialData);

    read_buffer_.insert(read_buffer_.end(), data.begin(), data.end());

    size_t frame_length;
    int payload_len, payload_offset;

    for (;;)
    {
        FrameStatus status = checkWMBusFrame(read_buffer_, &frame_length, &payload_len, &payload_offset, false);

        if (status == PartialFrame)
        {
            // Partial frame, stop eating.
            break;
        }
        if (status == ErrorInFrame)
        {
            verbose("(rawtty) protocol error in message received!\n");
            string msg = bin2hex(read_buffer_);
            debug("(rawtty) protocol error \"%s\"\n", msg.c_str());
            read_buffer_.clear();
            break;
        }
        if (status == FullFrame)
        {
            vector<uchar> payload;
            int rssi = 0;
            bool extra_byte = device_config_.usingRssi();

            if ((!extra_byte && payload_len > 0) || (extra_byte && payload_len > 1))
            {
                if (device_config_.usingRssi())
                {
                    rssi = read_buffer_[payload_offset+payload_len-1];
                    payload_len--;
                }
                uchar l = payload_len;
                payload.insert(payload.end(), &l, &l+1); // Re-insert the len byte.
                payload.insert(payload.end(), read_buffer_.begin()+payload_offset, read_buffer_.begin()+payload_offset+payload_len);
            }
            read_buffer_.erase(read_buffer_.begin(), read_buffer_.begin()+frame_length);
            AboutTelegram about("rc1180["+cached_device_id_+"]", rssi, LinkMode::UNKNOWN, FrameType::WMBUS);
            handleTelegram(about, payload);
        }
    }
}

AccessCheck detectRC1180(Detected *detected, shared_ptr<SerialCommunicationManager> manager)
try
{
    // Talk to the device and expect a very specific answer.
    const int baud_rate = getConfiguredBaudRate(*detected);
    auto serial = manager->createSerialDeviceTTY(detected->found_file.c_str(), baud_rate, PARITY::NONE, "detect rc1180");
    serial->disableCallbacks();
    bool ok = serial->open(false);
    if (!ok) return AccessCheck::NoSuchDevice;

    vector<uchar> data;
    vector<uchar> msg(1);

    // Send a single 0x00 byte. This will trigger the device to enter command mode
    // the device then responds with '>'
    msg[0] = 0;

    serial->send(msg);
    usleep(200*1000);
    serial->receive(&data);

    if (!data.empty() && data[0] != '>')
    {
       // no RC1180 device detected
       serial->close();
       verbose("(rc1180) are you there? no.\n");
       return AccessCheck::NoProperResponse;
    }

    data.clear();

    // send '0' to get get the dongle configuration data.
    msg[0] = '0';

    serial->send(msg);
    // Wait for 200ms so that the USB stick have time to prepare a response.
    usleep(1000*200);

    serial->receive(&data);

    ConfigRC1180 co;
    ok = co.decode(data);
    if (!ok || co.uart_baud_rate != rcUartBaudRateFromBauds(baud_rate))
    {
        // Decode must be ok and the uart_baud_rate mus match the speed we are using.
        serial->close();
        verbose("(rc1180) are you there? no.\n");
        return AccessCheck::NoProperResponse;
    }

    verbose("(rc1180) config: %s\n", co.str().c_str());

    /*
      Modification of the non-volatile memory should be done using the
      wmbusmeters-admin program. So this code should not execute here.
    if (co.rssi_mode == 0)
    {
        // Change the config so that the device appends an rssi byte.
        vector<uchar> updat(4);
        update[0] = 'M';
        update[1] = 0x05; // Register 5, rssi_mode
        update[2] = 1;    // Set value to 1 = enabled.
        update[3] = 0xff; // Stop modifying memory.
        serial->send(update);
        usleep(1000*200);
        // Reboot dongle.
    }
    */
    // Now exit config mode and continue listeing.
    msg[0] = 'X';
    serial->send(msg);

    usleep(1000*200);

    serial->close();

    detected->setAsFound(co.dongleId(), BusDeviceType::DEVICE_RC1180, baud_rate, false,
        detected->specified_device.linkmodes);

    verbose("(rc1180) are you there? yes %s\n", co.dongleId().c_str());

    return AccessCheck::AccessOK;
}
catch(const std::exception& e)
{
    warning("(rc1180) are you there? dunno, exception occured: %s\n", e.what());
    return AccessCheck::NoProperResponse;
}