/*
 Copyright (C) 2020-2021 Fredrik Öhrström (gpl-3.0-or-later)

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

#ifndef WMBUS_COMMON_H
#define WMBUS_COMMON_H

#include "util.h"
#include "threads.h"
#include "wmbus.h"

struct BusDeviceCommonImplementation : public virtual BusDevice
{
    BusDeviceCommonImplementation(string bus_alias,
                                  BusDeviceType t,
                                  shared_ptr<SerialCommunicationManager> manager,
                                  shared_ptr<SerialDevice> serial_override,
                                  bool is_serial);
    ~BusDeviceCommonImplementation();

    string busAlias();
    string hr();
    bool isSerial();
    BusDeviceType type();
    void onTelegram(function<bool(AboutTelegram&,vector<uchar>)> cb);
    bool sendTelegram(LinkMode link_mode, TelegramFormat format, vector<uchar> &content);
    bool handleTelegram(AboutTelegram &about, vector<uchar> frame);
    void checkStatus();
    bool isWorking();
    string dongleId();
    DeviceMode deviceMode();
    void setTimeout(int seconds, std::string expected_activity);
    void setResetInterval(int seconds);
    void setLinkModes(LinkModeSet lms);
    void setDeviceMode(DeviceMode mode);
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
    void setDetected(Detected detected) { detected_ = detected; }
    Detected *getDetected() { return &detected_; }
    void markAsNoLongerSerial();

    protected:

    shared_ptr<SerialCommunicationManager> manager_;
    void protocolErrorDetected();
    void resetProtocolErrorCount();
    bool areLinkModesConfigured();
    // Device specific set link modes implementation.
    void retrySetLinkModes(LinkModeSet lms);

    virtual void deviceSetDeviceMode(DeviceMode mode);
    virtual bool deviceSetLinkModes(LinkModeSet lms) = 0;
    // Device specific reset code, apart from serial->open and setLinkModes.
    virtual void deviceReset() = 0;
    virtual void deviceClose();

    LinkModeSet protectedGetLinkModes(); // Used to read private link_modes_ in subclass.

    private:

    // Bus alias.
    string bus_alias_;
    // Uses a serial tty?
    bool is_serial_ {};
    bool is_working_ {};
    vector<function<bool(AboutTelegram&,vector<uchar>)>> telegram_listeners_;
    BusDeviceType type_ {};
    int protocol_error_count_ {};
    time_t timeout_ {}; // If longer silence than timeout, then reset dongle! It might have hanged!
    string expected_activity_ {}; // During which times should we care about timeouts?
    time_t last_received_ {}; // When as the last telegram reception?
    time_t last_reset_ {}; // When did we last attempt a reset of the dongle?
    int reset_timeout_ {}; // When set to 23*3600 reset the device once every 23 hours.
    bool link_modes_configured_ {};
    DeviceMode device_mode_ { DeviceMode::OTHER }; // Other or meter.
    LinkModeSet link_modes_ {};
    Detected detected_ {}; // Used to remember how this device was setup.

    shared_ptr<SerialDevice> serial_;

protected:

    // When a wmbus dongle transmits a telegram, then it will use this id.
    // It can often be changed by configuring the wmbud dongle.
    string cached_device_id_;
    // Generated human readable name: eg
    // * /dev/ttyUSB0:im871a[12345678]
    // * rtlmbus[longantenna]
    string cached_hr_;
    // Some dongles have a unique id (that cannot be changed) in addition to the transmit id.
    string cached_device_unique_id_;

    // Lock this mutex when you sent a request to the wmbus device
    // Unlock when you received the response or it timedout.
    RecursiveMutex command_mutex_;
#define LOCK_WMBUS_EXECUTING_COMMAND(where) WITH(command_mutex_, command_mutex, where)

    // Use waitForRespones/notifyReponseIsHere to wait for a response
    // while the command_mutex_ is taken.
    int waiting_for_response_id_ {};
    Semaphore waiting_for_response_sem_;
    bool serial_override_ {};

    // Lock this mutex when you want to append to, truncate or clear the receiving buffer.
    RecursiveMutex receiving_buffer_mutex_;
#define LOCK_WMBUS_RECEIVING_BUFFER(where) WITH(receiving_buffer_mutex_, receiving_buffer_mutex, where)

};

#endif
