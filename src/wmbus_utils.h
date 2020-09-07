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
#include "wmbus.h"

bool decrypt_ELL_AES_CTR(Telegram *t, vector<uchar> &frame, vector<uchar>::iterator &pos, vector<uchar> &aeskey);
bool decrypt_TPL_AES_CBC_IV(Telegram *t, vector<uchar> &frame, vector<uchar>::iterator &pos, vector<uchar> &aeskey);
bool decrypt_TPL_AES_CBC_NO_IV(Telegram *t, vector<uchar> &frame, vector<uchar>::iterator &pos, vector<uchar> &aeskey);
string frameTypeKamstrupC1(int ft);

struct WMBusCommonImplementation : public virtual WMBus
{
    WMBusCommonImplementation(WMBusDeviceType t, SerialCommunicationManager *manager, unique_ptr<SerialDevice> serial_override);
    ~WMBusCommonImplementation();

    WMBusDeviceType type();
    void onTelegram(function<bool(vector<uchar>)> cb);
    bool handleTelegram(vector<uchar> frame);
    void checkStatus();
    bool isWorking();
    void setTimeout(int seconds, std::string expected_activity);
    void setLinkModes(LinkModeSet lms);
    void disconnectedFromDevice();
    bool reset();
    SerialDevice *serial() { if (serial_) return serial_.get(); else return NULL; }
    string device() { if (serial_) return serial_->device(); else return "?"; }

    protected:

    SerialCommunicationManager *manager_ {};
    void protocolErrorDetected();
    void resetProtocolErrorCount();
    bool areLinkModesConfigured();
    // Device specific set link modes implementation.
    virtual void deviceSetLinkModes(LinkModeSet lms) = 0;
    // Device specific reset code, apart from serial->open and setLinkModes.
    virtual void deviceReset() = 0;
    LinkModeSet protectedGetLinkModes(); // Used to read private link_modes_ in subclass.

    private:

    bool is_working_ {};
    vector<function<bool(vector<uchar>)>> telegram_listeners_;
//    vector<unique_ptr<Meter>> *meters_;
    WMBusDeviceType type_ {};
    int protocol_error_count_ {};
    time_t timeout_ {}; // If longer silence than timeout, then reset dongle! It might have hanged!
    string expected_activity_ {}; // During which times should we care about timeouts?
    time_t last_received_ {}; // When as the last telegram reception?
    time_t last_reset_ {}; // When did we last attempt a reset of the dongle?

    bool link_modes_configured_ {};
    LinkModeSet link_modes_ {};

    int regular_cb_id_;

    unique_ptr<SerialDevice> serial_;
};

#endif
