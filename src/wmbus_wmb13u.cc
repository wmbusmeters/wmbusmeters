/*
 Copyright (C) 2020 Fredrik Öhrström

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
#include"wmbus_cul.h"
#include"serial.h"
#include"threads.h"

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

/*
 Sadly, the WMB13U-868 dongle uses a prolific pl2303 USB2Serial converter
 and it seems like there are bugs in the linux drivers for this converter.
 Or the device itself is buggy....
 Anyway, the dongle works when first plugged in, but if
 the ttyUSB0 is closed and then opened again, it most likely
 stops working.

 So the dongle can perhaps be used like this:
 configure the dongle using the Windows software to use your
 desired C1 or T1 mode. Then plug it into your linux box.
 This driver intentionally does not write to the dongle,
 if you are lucky, the dongle might receive nicely and
 not hang.

 Update! It seems like the dongle will hang eventually anyway. :-(
*/
struct WMBusWMB13U : public virtual WMBusCommonImplementation
{
    bool ping();
    uint32_t getDeviceId();
    LinkModeSet getLinkModes();
    void deviceReset();
    void deviceSetLinkModes(LinkModeSet lms);
    LinkModeSet supportedLinkModes() {
        return
            C1_bit |
            S1_bit |
            T1_bit;
    }
    int numConcurrentLinkModes() { return 1; }
    bool canSetLinkModes(LinkModeSet lms)
    {
        if (0 == countSetBits(lms.bits())) return false;
        if (!supportedLinkModes().supports(lms)) return false;
        // Ok, the supplied link modes are compatible,
        // but wmb13u can only listen to one at a time.
        return 1 == countSetBits(lms.bits());
    }
    void processSerialData();
    void simulate();

    WMBusWMB13U(shared_ptr<SerialDevice> serial, shared_ptr<SerialCommunicationManager> manager);
    ~WMBusWMB13U() { }

private:

    LinkModeSet link_modes_ {};
    vector<uchar> read_buffer_;
    pthread_mutex_t wmb13u_serial_lock_ = PTHREAD_MUTEX_INITIALIZER;
    const char *wmb13u_serial_lock_func_ = "";
    pid_t       wmb13u_serial_lock_pid_ {};

    FrameStatus checkWMB13UFrame(vector<uchar> &data,
                                 size_t *frame_length,
                                 vector<uchar> &payload);
    bool getConfiguration();
    bool enterConfigModee();
    bool exitConfigModee();
    vector<uchar> config_;
};

shared_ptr<WMBus> openWMB13U(string device, shared_ptr<SerialCommunicationManager> manager, shared_ptr<SerialDevice> serial_override)
{
    if (serial_override)
    {
        WMBusWMB13U *imp = new WMBusWMB13U(serial_override, manager);
        return shared_ptr<WMBus>(imp);
    }

    auto serial = manager->createSerialDeviceTTY(device.c_str(), 19200, "wmb13u");
    WMBusWMB13U *imp = new WMBusWMB13U(serial, manager);
    return shared_ptr<WMBus>(imp);
}

WMBusWMB13U::WMBusWMB13U(shared_ptr<SerialDevice> serial, shared_ptr<SerialCommunicationManager> manager) :
    WMBusCommonImplementation(DEVICE_WMB13U, manager, serial)
{
    reset();
}

bool WMBusWMB13U::ping()
{
    if (serial()->readonly()) return true; // Feeding from stdin or file.

    /*
    if (!enterConfigMode()) return false;
    if (!exitConfigMode()) return false;*/

    return true;
}

uint32_t WMBusWMB13U::getDeviceId()
{
    return 0x11111111;
    /*getConfiguration();
    uchar a = config_[0x22];
    uchar b = config_[0x23];
    uchar c = config_[0x24];
    uchar d = config_[0x25];
    return a << 24 | b << 16 | c << 8 | d;*/
}

LinkModeSet WMBusWMB13U::getLinkModes()
{
    if (serial()->readonly()) { return Any_bit; }  // Feeding from stdin or file.

    // getConfiguration();
    return link_modes_;
}

void WMBusWMB13U::deviceReset()
{
}

void WMBusWMB13U::deviceSetLinkModes(LinkModeSet lms)
{
    if (serial()->readonly()) return; // Feeding from stdin or file.

    if (!canSetLinkModes(lms))
    {
        string modes = lms.hr();
        error("(wmb13u) setting link mode(s) %s is not supported for wmb13u\n", modes.c_str());
    }

    /*
    if (!enterConfigMode()) return;

    vector<uchar> atg(4);
    atg[0] = 'A';
    atg[1] = 'T';
    atg[2] = 'G';
    if (lms.has(LinkMode::C1))
    {
        // Listening to only C1.
        atg[3] = 0x10;
    }
    else if (lms.has(LinkMode::T1))
    {
        // Listening to only T1.
        atg[3] = 0x01;
    }
    else if (lms.has(LinkMode::S1))
    {
        // Listening to only S1.
        atg[3] = 0x03;
    }

    verbose("(wmb13u) set link mode %02x\n", atg[3]);
    serial_->send(atg);
    */
    link_modes_ = lms;

//    exitConfigMode();
}

void WMBusWMB13U::simulate()
{
}

void WMBusWMB13U::processSerialData()
{
    vector<uchar> data;

    // Try to get the serial lock, if not possible, then we
    // are in config mode. Stop this processing.
    if (pthread_mutex_trylock(&wmb13u_serial_lock_) != 0) return;
    // Receive and accumulated serial data until a full frame has been received.
    serial()->receive(&data);
    // Unlock the serial lock.
    UNLOCK("(wmb13u)", "processSerialData", wmb13u_serial_lock_);

    read_buffer_.insert(read_buffer_.end(), data.begin(), data.end());

    size_t frame_length;
    int payload_len, payload_offset;

    for (;;)
    {
        FrameStatus status = checkWMBusFrame(read_buffer_, &frame_length, &payload_len, &payload_offset);

        if (status == PartialFrame)
        {
            // Partial frame, stop eating.
            break;
        }
        if (status == ErrorInFrame)
        {
            verbose("(wmb13u) protocol error in message received!\n");
            string msg = bin2hex(read_buffer_);
            debug("(wmb13u) protocol error \"%s\"\n", msg.c_str());
            read_buffer_.clear();
            break;
        }
        if (status == FullFrame)
        {
            vector<uchar> payload;
            if (payload_len > 0)
            {
                // It appens RSSI and 2 CRC bytes. Remove those.
                uchar l = payload_len - 3;
                payload.insert(payload.end(), &l, &l+1); // Re-insert the len byte.
                payload.insert(payload.end(), read_buffer_.begin()+payload_offset, read_buffer_.begin()+payload_offset+payload_len-3);
            }
            read_buffer_.erase(read_buffer_.begin(), read_buffer_.begin()+frame_length);
            handleTelegram(payload);
        }
    }
}

bool WMBusWMB13U::enterConfigModee()
{
    LOCK("(wmb13u)", "enterConfigMode", wmb13u_serial_lock_);

    vector<uchar> data;

    // send 0xFF to wake up, if sleeping, just to be sure.
    vector<uchar> wakeup(1), at(2);
    wakeup[0] = 0xff;

    serial()->send(wakeup);
    usleep(1000*100);
    serial()->receive(&data);

    if (!startsWith("OK", data)) goto fail;

    // send AT to enter configuration mode.
    at[0] = 'A';
    at[1] = 'T';

    serial()->send(at);
    usleep(1000*100);
    serial()->receive(&data);

    if (!startsWith("OK", data)) goto fail;

    return true;

fail:

    UNLOCK("(wmb13u)", "enterConfigMode", wmb13u_serial_lock_);
    return false;
}

bool WMBusWMB13U::exitConfigModee()
{
    vector<uchar> data;

    // send ATQ to exit config mode.
    vector<uchar> atq(3);
    atq[0] = 'A';
    atq[1] = 'T';
    atq[2] = 'Q';

    serial()->send(atq);
    usleep(1000*100);
    serial()->receive(&data);

    // Always unlock....
    UNLOCK("(wmb13u)", "exitConfigMode", wmb13u_serial_lock_);

    if (!startsWith("OK", data)) return false;

    return true;
}

const char *lmname(int i)
{
    switch (i)
    {
    case 0x00: return "S2";
    case 0x01: return "T1";
    case 0x02: return "T2";
    case 0x03: return "S1";
    case 0x04: return "R2";
    case 0x10: return "C1";
    case 0x11: return "C2";
    }
    return "?";
}

bool WMBusWMB13U::getConfiguration()
{
    bool ok;

    ok = enterConfigModee();
    if (!ok) return false;

    // send AT0 to acquire configuration.
    vector<uchar> at0(3);
    at0[0] = 'A';
    at0[1] = 'T';
    at0[2] = '0';

    serial()->send(at0);
    usleep(1000*100);
    serial()->receive(&config_);

    verbose("(wmb13u) config: link mode %02x (%s)\n", config_[0x01], lmname(config_[0x01]));
    verbose("(wmb13u) config: data frame format %02x\n", config_[0x35]);

    ok = exitConfigModee();
    if (!ok) return false;

    return true;
}

AccessCheck detectWMB13U(string device, shared_ptr<SerialCommunicationManager> manager)
{
    // Talk to the device and expect a very specific answer.
    auto serial = manager->createSerialDeviceTTY(device.c_str(), 19200, "detect wmb13u");
    AccessCheck rc = serial->open(false);
    if (rc != AccessCheck::AccessOK) return AccessCheck::NotThere;

    verbose("(wmb13u) are you there?\n");

    vector<uchar> data;

    // send 0xFF to wake up, if sleeping, just to be sure.
    vector<uchar> wakeup(1);
    wakeup[0] = 0xff;

    serial->send(wakeup);
    usleep(1000*100);
    serial->receive(&data);

    if (!startsWith("OK", data)) return AccessCheck::NotThere;

    // send AT to enter configuration mode.
    vector<uchar> at(2);
    at[0] = 'A';
    at[1] = 'T';

    serial->send(at);
    usleep(1000*100);
    serial->receive(&data);

    if (!startsWith("OK", data)) return AccessCheck::NotThere;

    serial->close();
    return AccessCheck::AccessOK;
}
