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

#define SET_LINK_MODE 1
#define SET_X01_MODE 2

struct WMBusWMB13U : public virtual WMBusCommonImplementation
{
    bool ping();
    uint32_t getDeviceId();
    LinkModeSet getLinkModes();
    void setLinkModes(LinkModeSet lms);
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
    SerialDevice *serial() { return serial_.get(); }
    void simulate();

    WMBusWMB13U(unique_ptr<SerialDevice> serial, SerialCommunicationManager *manager);
    ~WMBusWMB13U() { }

private:
    unique_ptr<SerialDevice> serial_;
    SerialCommunicationManager *manager_ {};
    LinkModeSet link_modes_ {};
    vector<uchar> read_buffer_;
    pthread_mutex_t serial_lock_ = PTHREAD_MUTEX_INITIALIZER;

    FrameStatus checkWMB13UFrame(vector<uchar> &data,
                                 size_t *frame_length,
                                 vector<uchar> &payload);
    bool getConfiguration();
    bool enterConfigMode();
    bool exitConfigMode();
    vector<uchar> config_;
};

unique_ptr<WMBus> openWMB13U(string device, SerialCommunicationManager *manager, unique_ptr<SerialDevice> serial_override)
{
    if (serial_override)
    {
        WMBusWMB13U *imp = new WMBusWMB13U(std::move(serial_override), manager);
        return unique_ptr<WMBus>(imp);
    }

    auto serial = manager->createSerialDeviceTTY(device.c_str(), 19200);
    WMBusWMB13U *imp = new WMBusWMB13U(std::move(serial), manager);
    return unique_ptr<WMBus>(imp);
}

WMBusWMB13U::WMBusWMB13U(unique_ptr<SerialDevice> serial, SerialCommunicationManager *manager) :
    WMBusCommonImplementation(DEVICE_WMB13U), serial_(std::move(serial)), manager_(manager)
{
    manager_->listenTo(serial_.get(),call(this,processSerialData));
    serial_->open(true);
}

bool WMBusWMB13U::ping()
{
    if (serial_->readonly()) return true; // Feeding from stdin or file.

    verbose("(wmb13u) ping\n");

    if (!enterConfigMode()) return false;
    if (!exitConfigMode()) return false;

    return true;
}

uint32_t WMBusWMB13U::getDeviceId()
{
    getConfiguration();
    uchar a = config_[0x22];
    uchar b = config_[0x23];
    uchar c = config_[0x24];
    uchar d = config_[0x25];
    return a << 24 | b << 16 | c << 8 | d;
}

LinkModeSet WMBusWMB13U::getLinkModes()
{
    if (serial_->readonly()) { return Any_bit; }  // Feeding from stdin or file.

    getConfiguration();
    return link_modes_;
}

void WMBusWMB13U::setLinkModes(LinkModeSet lms)
{
    if (serial_->readonly()) return; // Feeding from stdin or file.

    if (!canSetLinkModes(lms))
    {
        string modes = lms.hr();
        error("(wmb13u) setting link mode(s) %s is not supported for wmb13u\n", modes.c_str());
    }

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

    link_modes_ = lms;

    exitConfigMode();
}

void WMBusWMB13U::simulate()
{
}

void WMBusWMB13U::processSerialData()
{
    vector<uchar> data;

    // Try to get the serial lock, if not possible, then we
    // are in config mode. Stop this processing.
    if (pthread_mutex_trylock(&serial_lock_) != 0) return;
    // Receive and accumulated serial data until a full frame has been received.
    serial_->receive(&data);
    // Unlock the serial lock.
    pthread_mutex_unlock(&serial_lock_);

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

bool WMBusWMB13U::enterConfigMode()
{
    pthread_mutex_lock(&serial_lock_);

    vector<uchar> data;

    // send 0xFF to wake up, if sleeping, just to be sure.
    vector<uchar> wakeup(1), at(2);
    wakeup[0] = 0xff;

    serial_->send(wakeup);
    usleep(1000*100);
    serial_->receive(&data);

    if (!startsWith("OK", data)) goto fail;

    // send AT to enter configuration mode.
    at[0] = 'A';
    at[1] = 'T';

    serial_->send(at);
    usleep(1000*100);
    serial_->receive(&data);

    if (!startsWith("OK", data)) goto fail;

    return true;

fail:
    pthread_mutex_unlock(&serial_lock_);
    return false;
}

bool WMBusWMB13U::exitConfigMode()
{
    vector<uchar> data;

    // send ATQ to exit config mode.
    vector<uchar> atq(3);
    atq[0] = 'A';
    atq[1] = 'T';
    atq[2] = 'Q';

    serial_->send(atq);
    usleep(1000*100);
    serial_->receive(&data);

    // Always unlock....
    pthread_mutex_unlock(&serial_lock_);

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

    ok = enterConfigMode();
    if (!ok) return false;

    // send AT0 to acquire configuration.
    vector<uchar> at0(3);
    at0[0] = 'A';
    at0[1] = 'T';
    at0[2] = '0';

    serial_->send(at0);
    usleep(1000*100);
    serial_->receive(&config_);

    verbose("(wmb13u) config: link mode %02x (%s)\n", config_[0x01], lmname(config_[0x01]));
    verbose("(wmb13u) config: data frame format %02x\n", config_[0x35]);

    ok = exitConfigMode();
    if (!ok) return false;

    return true;
}

bool detectWMB13U(string device, SerialCommunicationManager *manager)
{
    // Talk to the device and expect a very specific answer.
    auto serial = manager->createSerialDeviceTTY(device.c_str(), 19200);
    bool ok = serial->open(false);
    if (!ok) return false;

    verbose("(wmb13u) are you there?\n");

    vector<uchar> data;

    // send 0xFF to wake up, if sleeping, just to be sure.
    vector<uchar> wakeup(1);
    wakeup[0] = 0xff;

    serial->send(wakeup);
    usleep(1000*100);
    serial->receive(&data);

    if (!startsWith("OK", data)) return false;

    // send AT to enter configuration mode.
    vector<uchar> at(2);
    at[0] = 'A';
    at[1] = 'T';

    serial->send(at);
    usleep(1000*100);
    serial->receive(&data);

    if (!startsWith("OK", data)) return false;

    return true;

    serial->close();
    return ok;
}
