/*
 Copyright (C) 2021 Fredrik Öhrström

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

#define INFO_CODE_SMOKE 0x0001

struct MeterQSmoke : public virtual SmokeDetector, public virtual MeterCommonImplementation
{
    MeterQSmoke(MeterInfo &mi);

    string status();
    bool smokeDetected();

private:

    void processContent(Telegram *t);

    private:

    uchar counter_ {};

    uint16_t info_codes_ {};
    bool error_ {};
    string device_date_time_ {};
};

MeterQSmoke::MeterQSmoke(MeterInfo &mi) :
    MeterCommonImplementation(mi, MeterDriver::QSMOKE)
{
    setExpectedTPLSecurityMode(TPLSecurityMode::AES_CBC_IV);

    addLinkMode(LinkMode::T1);

    addPrint("status", Quantity::Text,
             [&](){ return status(); },
             "The current status: OK, SMOKE or ERROR.",
             true, true);

    addPrint("counter", Quantity::Counter,
             [&](Unit u){ return counter_; },
             "Transmission counter.",
             false, true);

    addPrint("device_date_time", Quantity::Text,
             [&](){ return device_date_time_; },
             "Device date time.",
             false, true);

}

shared_ptr<Meter> createQSmoke(MeterInfo &mi)
{
    return shared_ptr<SmokeDetector>(new MeterQSmoke(mi));
}

bool MeterQSmoke::smokeDetected()
{
    return info_codes_ & INFO_CODE_SMOKE;
}

/*
(wmbus) 0b: 01 dif (8 Bit Integer/Binary Instantaneous value)
(wmbus) 0c: FD vif (Second extension FD of VIF-codes)
(wmbus) 0d: 08 vife (Access Number (transmission count))
(wmbus) 0e: F0
(wmbus) 0f: 81 dif (8 Bit Integer/Binary Instantaneous value)
(wmbus) 10: 02 dife (subunit=0 tariff=0 storagenr=4)
(wmbus) 11: 7C vif (VIF in following string (length in first byte))
(wmbus) 12: 03 viflen (3)
(wmbus) 13: 49 vif (I)
(wmbus) 14: 55 vif (U)
(wmbus) 15: 23 vif (#)
(wmbus) 16: 00
(wmbus) 17: 82 dif (16 Bit Integer/Binary Instantaneous value)
(wmbus) 18: 02 dife (subunit=0 tariff=0 storagenr=4)
(wmbus) 19: 6C vif (Date type G)
(wmbus) 1a: FFFF
(wmbus) 1c: 81 dif (8 Bit Integer/Binary Instantaneous value)
(wmbus) 1d: 03 dife (subunit=0 tariff=0 storagenr=6)
(wmbus) 1e: 7C vif (VIF in following string (length in first byte))
(wmbus) 1f: 03 viflen (3)
(wmbus) 20: 4C vif (L)
(wmbus) 21: 41 vif (A)
(wmbus) 22: 23 vif (#)
(wmbus) 23: 00
(wmbus) 24: 82 dif (16 Bit Integer/Binary Instantaneous value)
(wmbus) 25: 03 dife (subunit=0 tariff=0 storagenr=6)
(wmbus) 26: 6C vif (Date type G)
(wmbus) 27: FFFF
(wmbus) 29: 03 dif (24 Bit Integer/Binary Instantaneous value)
(wmbus) 2a: FD vif (Second extension FD of VIF-codes)
(wmbus) 2b: 17 vife (Error flags (binary))
(wmbus) 2c: 000000
(wmbus) 2f: 32 dif (16 Bit Integer/Binary Value during error state)
(wmbus) 30: 6C vif (Date type G)
(wmbus) 31: FFFF
(wmbus) 33: 04 dif (32 Bit Integer/Binary Instantaneous value)
(wmbus) 34: 6D vif (Date and time type)
(wmbus) 35: 0F0ABC2B
(wmbus) 39: 02 dif (16 Bit Integer/Binary Instantaneous value)
(wmbus) 3a: FD vif (Second extension FD of VIF-codes)
(wmbus) 3b: AC vife (Duration since last readout [second(s)])
(wmbus) 3c: 7E vife (Reserved)
(wmbus) 3d: 1100

Another version 0x23

qsmoke) 0f: 81 dif (8 Bit Integer/Binary Instantaneous value)
(qsmoke) 10: 02 dife (subunit=0 tariff=0 storagenr=4)
(qsmoke) 11: 7C vif (VIF in following string (length in first byte))
(qsmoke) 12: 03 viflen (3)
(qsmoke) 13: 49 vif (I)
(qsmoke) 14: 55 vif (U)
(qsmoke) 15: 23 vif (#)
(qsmoke) 16: 00
(qsmoke) 17: 82 dif (16 Bit Integer/Binary Instantaneous value)
(qsmoke) 18: 02 dife (subunit=0 tariff=0 storagenr=4)
(qsmoke) 19: 6C vif (Date type G)
(qsmoke) 1a: FFFF
(qsmoke) 1c: 81 dif (8 Bit Integer/Binary Instantaneous value)
(qsmoke) 1d: 03 dife (subunit=0 tariff=0 storagenr=6)
(qsmoke) 1e: 7C vif (VIF in following string (length in first byte))
(qsmoke) 1f: 03 viflen (3)
(qsmoke) 20: 4C vif (L)
(qsmoke) 21: 41 vif (A)
(qsmoke) 22: 23 vif (#)
(qsmoke) 23: 00
(qsmoke) 24: 82 dif (16 Bit Integer/Binary Instantaneous value)
(qsmoke) 25: 03 dife (subunit=0 tariff=0 storagenr=6)
(qsmoke) 26: 6C vif (Date type G)
(qsmoke) 27: FFFF
(qsmoke) 29: 02 dif (16 Bit Integer/Binary Instantaneous value)
(qsmoke) 2a: FD vif (Second extension FD of VIF-codes)
(qsmoke) 2b: 17 vife (Error flags (binary))
(qsmoke) 2c: 0000
(qsmoke) 2e: 32 dif (16 Bit Integer/Binary Value during error state)
(qsmoke) 2f: 6C vif (Date type G)
(qsmoke) 30: FFFF
(qsmoke) 32: 04 dif (32 Bit Integer/Binary Instantaneous value)
(qsmoke) 33: 6D vif (Date and time type)
(qsmoke) 34: * 2514BC2B device datetime (2021-11-28 20:37)

*/
void MeterQSmoke::processContent(Telegram *t)
{
    int offset;
    string key;

    if (findKey(MeasurementType::Unknown, ValueInformation::DateTime, 0, 0, &key, &t->values)) {
        struct tm datetime;
        extractDVdate(&t->values, key, &offset, &datetime);
        device_date_time_ = strdatetime(&datetime);
        t->addMoreExplanation(offset, " device datetime (%s)", device_date_time_.c_str());
    }
}

string MeterQSmoke::status()
{
    // We do not yet know how to understand the fields!
    return "WOOT";
    /*
    string s;
    bool smoke = 0;
    if (smoke)
    {
        s.append("SMOKE ");
    }

    if (error_)
    {
        s.append("ERROR ");
    }

    if (s.length() > 0)
    {
        s.pop_back();
        return s;
    }
    */
}
