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

#ifndef WMBUS_UTILS_H
#define WMBUS_UTILS_H

#include "util.h"
#include "threads.h"
#include "wmbus.h"

bool decrypt_ELL_AES_CTR(Telegram *t, vector<uchar> &frame, vector<uchar>::iterator &pos, vector<uchar> &aeskey);
bool decrypt_TPL_AES_CBC_IV(Telegram *t, vector<uchar> &frame, vector<uchar>::iterator &pos, vector<uchar> &aeskey);
bool decrypt_TPL_AES_CBC_NO_IV(Telegram *t, vector<uchar> &frame, vector<uchar>::iterator &pos, vector<uchar> &aeskey);
string frameTypeKamstrupC1(int ft);

struct WMBusCommonImplementation : public virtual WMBus
{
    WMBusCommonImplementation(WMBusDeviceType t, shared_ptr<SerialCommunicationManager> manager, shared_ptr<SerialDevice> serial_override);
    ~WMBusCommonImplementation();

    WMBusDeviceType type();
    void onTelegram(function<bool(vector<uchar>)> cb);
    bool handleTelegram(vector<uchar> frame);
    void checkStatus();
    bool isWorking();
    string dongleId();
    void setTimeout(int seconds, std::string expected_activity);
    void setResetInterval(int seconds);
    void setLinkModes(LinkModeSet lms);
    virtual void processSerialData() = 0;
    void disconnectedFromDevice();
    bool reset();
    SerialDevice *serial() { if (serial_) return serial_.get(); else return NULL; }
    bool serialOverride() { return serial_override_; }
    void markSerialAsOverriden() { serial_override_ = true; }

    string device() { if (serial_) return serial_->device(); else return "?"; }
    // Wait for a response to arrive from the device.
    bool waitForResponse(int id);
    // Notify the waiter that the response has arrived.
    bool notifyResponseIsHere(int id);
    void close();

    protected:

    shared_ptr<SerialCommunicationManager> manager_;
    void protocolErrorDetected();
    void resetProtocolErrorCount();
    bool areLinkModesConfigured();
    // Device specific set link modes implementation.
    virtual void deviceSetLinkModes(LinkModeSet lms) = 0;
    // Device specific reset code, apart from serial->open and setLinkModes.
    virtual void deviceReset() = 0;
    virtual void deviceClose();
    LinkModeSet protectedGetLinkModes(); // Used to read private link_modes_ in subclass.

    private:

    bool is_working_ {};
    vector<function<bool(vector<uchar>)>> telegram_listeners_;
    WMBusDeviceType type_ {};
    int protocol_error_count_ {};
    time_t timeout_ {}; // If longer silence than timeout, then reset dongle! It might have hanged!
    string expected_activity_ {}; // During which times should we care about timeouts?
    time_t last_received_ {}; // When as the last telegram reception?
    time_t last_reset_ {}; // When did we last attempt a reset of the dongle?
    int reset_timeout_ {}; // When set to 24*3600 reset the device once every 24 hours.
    bool link_modes_configured_ {};
    LinkModeSet link_modes_ {};

    shared_ptr<SerialDevice> serial_;

protected:

    // When a wmbus dongle transmits a telegram, then it will use this id.
    string cached_device_id_;

    // Lock this mutex when you sent a request to the wmbus device
    // Unlock when you received the response or it timedout.
    RecursiveMutex command_mutex_;
#define LOCK_WMBUS_EXECUTING_COMMAND(where) WITH(command_mutex_, where)

    // Use waitForRespones/notifyReponseIsHere to wait for a response
    // while the command_mutex_ is taken.
    int waiting_for_response_id_ {};
    Semaphore waiting_for_response_sem_;
    bool serial_override_ {};
};

#endif
