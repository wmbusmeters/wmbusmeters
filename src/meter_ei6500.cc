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

#include"dvparser.h"
#include"meters.h"
#include"meters_common_implementation.h"
#include"wmbus.h"
#include"wmbus_utils.h"
#include"string.h"

#define INFO_INSTALLATION_COMPLETED 0x0001
#define INFO_ENVIRONMENT_CHANGED    0x0002
#define INFO_REMOVED                0x0040
#define INFO_LOW_BATTERY            0x0080
#define INFO_OBSTACLE_DETECTED      0x0100
#define INFO_COVERING_DETECTED      0x0200

struct MeterEI6500 : public virtual MeterCommonImplementation
{
    MeterEI6500(MeterInfo &mi);

    string status();
    bool smokeDetected();
    string messageDate();
    string commissionDate();
    string lastSounderTestDate();
    string lastAlarmDate();
    string smokeAlarmCounter();
    string removedCounter();
    string totalRemoveDuration();
    string lastRemoveDate();
    string testButtonCounter();
    string testButtonLastDate();

private:

    void processContent(Telegram *t);

    private:

    string software_version_;
    string message_datetime_ {};
    uint8_t tpl_sts_ {};
    uint16_t info_codes_ {};

    uint16_t smoke_alarm_counter_ {};
    string last_alarm_date_ {};
    uint32_t total_remove_duration_ {};
    string last_remove_date_ {};
    string test_button_last_date_ {};
    uint16_t removed_counter_ {};
    uint16_t test_button_counter_ {};

    map<int,string> error_codes_;
};

MeterEI6500::MeterEI6500(MeterInfo &mi) :
    MeterCommonImplementation(mi, "ei6500")
{
    setMeterType(MeterType::SmokeDetector);

    setExpectedTPLSecurityMode(TPLSecurityMode::AES_CBC_IV);

    addLinkMode(LinkMode::C1);

    // Vendor specific errors sent through the tpl status byte.
    /*
    error_codes_ = { { 0x10, "XXX" },
                     { 0x20, "YYY" },
                     { 0x30, "ZZZ" },
                     { 0xf0, "WWW" } };*/

    addPrint("software_version", Quantity::Text,
             [&](){ return software_version_; },
             "Software version.",
             false, true);
    addPrint("message_datetime", Quantity::Text,
             [&](){ return messageDate(); },
             "Date of message.",
             false, true);
    addPrint("last_alarm_date", Quantity::Text,
             [&](){ return lastAlarmDate(); },
             "Date of last alarm.",
             true, true);
    addPrint("smoke_alarm_counter", Quantity::Text,
             [&](){ return smokeAlarmCounter(); },
             "Number of times smoke alarm was triggered.",
             true, true);
    addPrint("total_remove_duration", Quantity::Text,
             [&](){ return totalRemoveDuration(); },
             "Number of times it was removed.",
             true, true);
    addPrint("last_remove_date", Quantity::Text,
             [&](){ return lastRemoveDate(); },
             "Date of last removal.",
             true, true);
    addPrint("removed_counter", Quantity::Text,
             [&](){ return removedCounter(); },
             "removed counter",
             true, true);
    addPrint("test_button_last_date", Quantity::Text,
             [&](){ return testButtonLastDate(); },
             "Date of last test button press.",
             true, true);
    addPrint("test_button_counter", Quantity::Text,
             [&](){ return testButtonCounter(); },
             "test button counter",
             true, true);
    addPrint("status", Quantity::Text,
             [&](){ return status(); },
             "Status of smoke detector.",
             true, true);
}

shared_ptr<Meter> createEI6500(MeterInfo &mi)
{
    return shared_ptr<Meter>(new MeterEI6500(mi));
}

bool MeterEI6500::smokeDetected()
{
    return 0;
}

void MeterEI6500::processContent(Telegram *t)
{
    /*
      (ei6500) 11: 0B dif (6 digit BCD Instantaneous value)
      (ei6500) 12: FD vif (Second extension FD of VIF-codes)
      (ei6500) 13: 0F vife (Software version #)
      (ei6500) 14: * 060101 software version (1.1.6)
      (ei6500) 17: 04 dif (32 Bit Integer/Binary Instantaneous value)
      (ei6500) 18: 6D vif (Date and time type)
      (ei6500) 19: * 300CAB22 message datetime (2021-02-11 12:48)
      (ei6500) 1d: 02 dif (16 Bit Integer/Binary Instantaneous value)
      (ei6500) 1e: FD vif (Second extension FD of VIF-codes)
      (ei6500) 1f: 17 vife (Error flags (binary))
      (ei6500) 20: * 0000 info codes (148ce5d8)
      (ei6500) 22: 82 dif (16 Bit Integer/Binary Instantaneous value)
      (ei6500) 23: 20 dife (subunit=0 tariff=2 storagenr=0)
      (ei6500) 24: 6C vif (Date type G)
      (ei6500) 25: AB22
      (ei6500) 27: 42 dif (16 Bit Integer/Binary Instantaneous value storagenr=1)
      (ei6500) 28: 6C vif (Date type G)
      (ei6500) 29: 0101
      (ei6500) 2b: 84 dif (32 Bit Integer/Binary Instantaneous value)
      (ei6500) 2c: 40 dife (subunit=1 tariff=0 storagenr=0)
      (ei6500) 2d: FF vif (Vendor extension)
      (ei6500) 2e: 2C vife (per litre)
      (ei6500) 2f: 000F1100
      (ei6500) 33: 82 dif (16 Bit Integer/Binary Instantaneous value)
      (ei6500) 34: 50 dife (subunit=1 tariff=1 storagenr=0)
      (ei6500) 35: FD vif (Second extension FD of VIF-codes)
      (ei6500) 36: 61 vife (Cumulation counter)
      (ei6500) 37: * 0000 smoke alarm counter (0)
      (ei6500) 39: 82 dif (16 Bit Integer/Binary Instantaneous value)
      (ei6500) 3a: 50 dife (subunit=1 tariff=1 storagenr=0)
      (ei6500) 3b: 6C vif (Date type G)
      (ei6500) 3c: * 0101 last alarm date (2000-01-01)
      (ei6500) 3e: 82 dif (16 Bit Integer/Binary Instantaneous value)
      (ei6500) 3f: 60 dife (subunit=1 tariff=2 storagenr=0)
      (ei6500) 40: FD vif (Second extension FD of VIF-codes)
      (ei6500) 41: 61 vife (Cumulation counter)
      (ei6500) 42: * 0000 removed counter (0)
      (ei6500) 44: 83 dif (24 Bit Integer/Binary Instantaneous value)
      (ei6500) 45: 60 dife (subunit=1 tariff=2 storagenr=0)
      (ei6500) 46: FD vif (Second extension FD of VIF-codes)
      (ei6500) 47: 31 vife (Duration of tariff [minute(s)])
      (ei6500) 48: * 000000 total remove duration (0)
      (ei6500) 4b: 82 dif (16 Bit Integer/Binary Instantaneous value)
      (ei6500) 4c: 60 dife (subunit=1 tariff=2 storagenr=0)
      (ei6500) 4d: 6C vif (Date type G)
      (ei6500) 4e: * 0101 last remove date (2000-01-01)
      (ei6500) 50: 82 dif (16 Bit Integer/Binary Instantaneous value)
      (ei6500) 51: 70 dife (subunit=1 tariff=3 storagenr=0)
      (ei6500) 52: FD vif (Second extension FD of VIF-codes)
      (ei6500) 53: 61 vife (Cumulation counter)
      (ei6500) 54: * 0100 test button counter (1)
      (ei6500) 56: 82 dif (16 Bit Integer/Binary Instantaneous value)
      (ei6500) 57: 70 dife (subunit=1 tariff=3 storagenr=0)
      (ei6500) 58: 6C vif (Date type G)
      (ei6500) 59: * AB22 test button last date (2021-02-11)
    */
    int offset;

    tpl_sts_ = t->tpl_sts;

    uint64_t serial;
    if (extractDVlong(&t->values, "0BFD0F", &offset, &serial))
    {
        // 060101 --> 01.01.06
        software_version_ =
            to_string((serial/10000)%100)+"."+
            to_string((serial/100)%100)+"."+
            to_string((serial)%100);
        t->addMoreExplanation(offset, " software version (%s)", software_version_.c_str());
    }

    struct tm datetime;
    if (extractDVdate(&t->values, "046D", &offset, &datetime))
    {
        message_datetime_ = strdatetime(&datetime);
        t->addMoreExplanation(offset, " message datetime (%s)", message_datetime_.c_str());
    }

    if (extractDVuint16(&t->values, "02FD17", &offset, &info_codes_))
    {
        string s = status();
        t->addMoreExplanation(offset, " info codes (%x)", s.c_str());
    }

    extractDVdate(&t->values, "82506C", &offset, &datetime);
    last_alarm_date_ = strdate(&datetime);
    t->addMoreExplanation(offset, " last alarm date (%s)", last_alarm_date_.c_str());

    extractDVuint16(&t->values, "8250FD61", &offset, &smoke_alarm_counter_);
    t->addMoreExplanation(offset, " smoke alarm counter (%zu)", smoke_alarm_counter_);

    extractDVuint16(&t->values, "8260FD61", &offset, &removed_counter_);
    t->addMoreExplanation(offset, " removed counter (%zu)", removed_counter_);

    extractDVuint16(&t->values, "8270FD61", &offset, &test_button_counter_);
    t->addMoreExplanation(offset, " test button counter (%zu)", test_button_counter_);

    extractDVuint24(&t->values, "8360FD31", &offset, &total_remove_duration_);
    t->addMoreExplanation(offset, " total remove duration (%zu)", total_remove_duration_);

    extractDVdate(&t->values, "82606C", &offset, &datetime);
    last_remove_date_ = strdate(&datetime);
    t->addMoreExplanation(offset, " last remove date (%s)", last_remove_date_.c_str());

    extractDVdate(&t->values, "82706C", &offset, &datetime);
    test_button_last_date_ = strdate(&datetime);
    t->addMoreExplanation(offset, " test button last date (%s)", test_button_last_date_.c_str());
}

string MeterEI6500::status()
{
    string s = decodeTPLStatusByte(tpl_sts_, &error_codes_);

    if (s == "OK") s = ""; else s += " ";

    if ((info_codes_ & INFO_INSTALLATION_COMPLETED) == 0) s.append("NOT_INSTALLED ");
    if (info_codes_ & INFO_ENVIRONMENT_CHANGED) s.append("ENVIRONMENT_CHANGED ");
    if (info_codes_ & INFO_REMOVED) s.append("REMOVED ");
    if (info_codes_ & INFO_OBSTACLE_DETECTED) s.append("OBSTACLE_DETECTED ");
    if (info_codes_ & INFO_COVERING_DETECTED) s.append("COVERING_DETECTED ");

    if (s.length() > 0)
    {
        // There is something to report!
        s.pop_back(); // Remove final space
        return s;
    }
    return "OK";
}

string MeterEI6500::messageDate()
{
	return message_datetime_;
}

string MeterEI6500::lastAlarmDate()
{
	return last_alarm_date_;
}

string MeterEI6500::totalRemoveDuration()
{
	return to_string(total_remove_duration_) + " minutes";
}

string MeterEI6500::smokeAlarmCounter()
{
	return to_string(smoke_alarm_counter_);
}

string MeterEI6500::testButtonCounter()
{
	return to_string(test_button_counter_);
}

string MeterEI6500::removedCounter()
{
	return to_string(removed_counter_);
}

string MeterEI6500::lastRemoveDate()
{
	return last_remove_date_;
}

string MeterEI6500::testButtonLastDate()
{
	return test_button_last_date_;
}
