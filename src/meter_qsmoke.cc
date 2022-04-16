/*
 Copyright (C) 2021 Fredrik Öhrström (gpl-3.0-or-later)

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

struct MeterQSmoke : public virtual MeterCommonImplementation
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
    uint8_t ui_event_count_ {};
    string ui_event_date_ {};
    uint8_t al_event_count_ {};
    string al_event_date_ {};
    uint32_t error_flags_ {};
    string error_date_ {};
    string device_date_time_ {};
    int duration_since_readout_s_ {};
};

MeterQSmoke::MeterQSmoke(MeterInfo &mi) :
    MeterCommonImplementation(mi, "qsmoke")
{
    setMeterType(MeterType::SmokeDetector);

    setExpectedTPLSecurityMode(TPLSecurityMode::AES_CBC_IV);

    addLinkMode(LinkMode::C1);

    addPrint("status", Quantity::Text,
             [&](){ return status(); },
             "The current status: OK, SMOKE or ERROR.",
             PrintProperty::FIELD | PrintProperty::JSON);

    addPrint("counter", Quantity::Counter,
             [&](Unit u){ return counter_; },
             "Transmission counter.",
             PrintProperty::JSON);

    addPrint("device_date_time", Quantity::Text,
             [&](){ return device_date_time_; },
             "Device date time.",
             PrintProperty::JSON);

}

shared_ptr<Meter> createQSmoke(MeterInfo &mi)
{
    return shared_ptr<Meter>(new MeterQSmoke(mi));
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

(qsmoke) 0f: 81 dif (8 Bit Integer/Binary Instantaneous value)
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

Telegram with #UI set
#UI is increased after removing the smoke setector from the mounting plate.
If that triggers the dismantling alarm or the environmental monitoring is not known yet.

telegram=|3E44934480570147231A78#01FD089E81027C034955230282026CBB2C81037C034C41230082036CFFFF03FD17100010326CFFFF046D060FBB2C02FDAC7E8000|
(qsmoke) 000   : 3e length (62 bytes)
(qsmoke) 001   : 44 dll-c (from meter SND_NR)
(qsmoke) 002   : 9344 dll-mfct (QDS)
(qsmoke) 004   : 80570147 dll-id (47015780)
(qsmoke) 008   : 23 dll-version
(qsmoke) 009   : 1a dll-type (Smoke detector)
(qsmoke) 010   : 78 tpl-ci-field (EN 13757-3 Application Layer (no tplh))
(qsmoke) 011   : 01 dif (8 Bit Integer/Binary Instantaneous value)
(qsmoke) 012   : FD vif (Second extension FD of VIF-codes)
(qsmoke) 013   : 08 vife (Access Number (transmission count))
(qsmoke) 014 C?: 9E
(qsmoke) 015   : 81 dif (8 Bit Integer/Binary Instantaneous value)
(qsmoke) 016   : 02 dife (subunit=0 tariff=0 storagenr=4)
(qsmoke) 017   : 7C vif (VIF in following string (length in first byte))
(qsmoke) 018   : 03 viflen (3)
(qsmoke) 019   : 49 vif (I)
(qsmoke) 020   : 55 vif (U)
(qsmoke) 021   : 23 vif (#)
(qsmoke) 022 C!: 02 UI event count (2)
(qsmoke) 023   : 82 dif (16 Bit Integer/Binary Instantaneous value)
(qsmoke) 024   : 02 dife (subunit=0 tariff=0 storagenr=4)
(qsmoke) 025   : 6C vif (Date type G)
(qsmoke) 026 C!: BB2C UI event date (2021-12-27)
(qsmoke) 028   : 81 dif (8 Bit Integer/Binary Instantaneous value)
(qsmoke) 029   : 03 dife (subunit=0 tariff=0 storagenr=6)
(qsmoke) 030   : 7C vif (VIF in following string (length in first byte))
(qsmoke) 031   : 03 viflen (3)
(qsmoke) 032   : 4C vif (L)
(qsmoke) 033   : 41 vif (A)
(qsmoke) 034   : 23 vif (#)
(qsmoke) 035 C!: 00 AL event count (0)
(qsmoke) 036   : 82 dif (16 Bit Integer/Binary Instantaneous value)
(qsmoke) 037   : 03 dife (subunit=0 tariff=0 storagenr=6)
(qsmoke) 038   : 6C vif (Date type G)
(qsmoke) 039 C!: FFFF AL event date (2127-15-31)
(qsmoke) 041   : 03 dif (24 Bit Integer/Binary Instantaneous value)
(qsmoke) 042   : FD vif (Second extension FD of VIF-codes)
(qsmoke) 043   : 17 vife (Error flags (binary))
(qsmoke) 044 C!: 100010 error flags (100010)
(qsmoke) 047   : 32 dif (16 Bit Integer/Binary Value during error state)
(qsmoke) 048   : 6C vif (Date type G)
(qsmoke) 049 C!: FFFF error date (2127-15-31)
(qsmoke) 051   : 04 dif (32 Bit Integer/Binary Instantaneous value)
(qsmoke) 052   : 6D vif (Date and time type)
(qsmoke) 053 C!: 060FBB2C device datetime (2021-12-27 15:06)
(qsmoke) 057   : 02 dif (16 Bit Integer/Binary Instantaneous value)
(qsmoke) 058   : FD vif (Second extension FD of VIF-codes)
(qsmoke) 059   : AC vife (Duration since last readout [second(s)])
(qsmoke) 060   : 7E vife (Reserved)
(qsmoke) 061 C!: 8000 duration (128 s)

Telegram with #AL set

telegram=|3744934471478946231A7A6B100020#81027C034955230082026CFFFF81037C034C41230182036CB92902FD170400326CFFFF046D1B12AC2C|
(qsmoke) 000   : 37 length (55 bytes)
(qsmoke) 001   : 44 dll-c (from meter SND_NR)
(qsmoke) 002   : 9344 dll-mfct (QDS)
(qsmoke) 004   : 71478946 dll-id (46894771)
(qsmoke) 008   : 23 dll-version
(qsmoke) 009   : 1a dll-type (Smoke detector)
(qsmoke) 010   : 7a tpl-ci-field (EN 13757-3 Application Layer (short tplh))
(qsmoke) 011   : 6b tpl-acc-field
(qsmoke) 012   : 10 tpl-sts-field (TEMPORARY_ERROR)
(qsmoke) 013   : 0020 tpl-cfg 2000 (synchronous )
(qsmoke) 015   : 81 dif (8 Bit Integer/Binary Instantaneous value)
(qsmoke) 016   : 02 dife (subunit=0 tariff=0 storagenr=4)
(qsmoke) 017   : 7C vif (VIF in following string (length in first byte))
(qsmoke) 018   : 03 viflen (3)
(qsmoke) 019   : 49 vif (I)
(qsmoke) 020   : 55 vif (U)
(qsmoke) 021   : 23 vif (#)
(qsmoke) 022 C!: 00 UI event count (0)
(qsmoke) 023   : 82 dif (16 Bit Integer/Binary Instantaneous value)
(qsmoke) 024   : 02 dife (subunit=0 tariff=0 storagenr=4)
(qsmoke) 025   : 6C vif (Date type G)
(qsmoke) 026 C!: FFFF UI event date (2127-15-31)
(qsmoke) 028   : 81 dif (8 Bit Integer/Binary Instantaneous value)
(qsmoke) 029   : 03 dife (subunit=0 tariff=0 storagenr=6)
(qsmoke) 030   : 7C vif (VIF in following string (length in first byte))
(qsmoke) 031   : 03 viflen (3)
(qsmoke) 032   : 4C vif (L)
(qsmoke) 033   : 41 vif (A)
(qsmoke) 034   : 23 vif (#)
(qsmoke) 035 C!: 01 AL event count (1)
(qsmoke) 036   : 82 dif (16 Bit Integer/Binary Instantaneous value)
(qsmoke) 037   : 03 dife (subunit=0 tariff=0 storagenr=6)
(qsmoke) 038   : 6C vif (Date type G)
(qsmoke) 039 C!: B929 AL event date (2021-09-25)
(qsmoke) 041   : 02 dif (16 Bit Integer/Binary Instantaneous value)
(qsmoke) 042   : FD vif (Second extension FD of VIF-codes)
(qsmoke) 043   : 17 vife (Error flags (binary))
(qsmoke) 044 C!: 0400 error flags (0004)
(qsmoke) 046   : 32 dif (16 Bit Integer/Binary Value during error state)
(qsmoke) 047   : 6C vif (Date type G)
(qsmoke) 048 C!: FFFF error date (2127-15-31)
(qsmoke) 050   : 04 dif (32 Bit Integer/Binary Instantaneous value)
(qsmoke) 051   : 6D vif (Date and time type)
(qsmoke) 052 C!: 1B12AC2C device datetime (2021-12-12 18:27)
*/
void MeterQSmoke::processContent(Telegram *t)
{
    int offset;
    string key;

    key = "81027C495523";
    if (hasKey(&t->dv_entries, key)) {
        if (extractDVuint8(&t->dv_entries, key, &offset, &ui_event_count_)) {
            t->addMoreExplanation(offset, " UI event count (%d)", ui_event_count_);
        }
    }

    if (findKey(MeasurementType::Instantaneous, VIFRange::Date, 4, 0, &key, &t->dv_entries)) {
        struct tm date;
        extractDVdate(&t->dv_entries, key, &offset, &date);
        ui_event_date_ = strdate(&date);
        t->addMoreExplanation(offset, " UI event date (%s)", ui_event_date_.c_str());
    }

    key = "81037C4C4123";
    if (hasKey(&t->dv_entries, key)) {
        if (extractDVuint8(&t->dv_entries, key, &offset, &al_event_count_)) {
            t->addMoreExplanation(offset, " AL event count (%d)", al_event_count_);
        }
    }

    if (findKey(MeasurementType::Instantaneous, VIFRange::Date, 6, 0, &key, &t->dv_entries)) {
        struct tm date;
        extractDVdate(&t->dv_entries, key, &offset, &date);
        al_event_date_ = strdate(&date);
        t->addMoreExplanation(offset, " AL event date (%s)", al_event_date_.c_str());
    }

    key = "02FD17";
    if (hasKey(&t->dv_entries, key)) {
        if (extractDVuint16(&t->dv_entries, key, &offset, (uint16_t*) &error_flags_)) {
            t->addMoreExplanation(offset, " error flags (%04X)", error_flags_);
        }
    } else {
        key = "03FD17";
        if (hasKey(&t->dv_entries, key)) {
            if (extractDVuint24(&t->dv_entries, key, &offset, &error_flags_)) {
                t->addMoreExplanation(offset, " error flags (%06X)", error_flags_);
            }
        }
    }

    if (findKey(MeasurementType::AtError, VIFRange::Date, 0, 0, &key, &t->dv_entries)) {
        struct tm date;
        extractDVdate(&t->dv_entries, key, &offset, &date);
        error_date_ = strdate(&date);
        t->addMoreExplanation(offset, " error date (%s)", error_date_.c_str());
    }

    if (findKey(MeasurementType::Unknown, VIFRange::DateTime, 0, 0, &key, &t->dv_entries)) {
        struct tm datetime;
        extractDVdate(&t->dv_entries, key, &offset, &datetime);
        device_date_time_ = strdatetime(&datetime);
        t->addMoreExplanation(offset, " device datetime (%s)", device_date_time_.c_str());
    }

    key = "02FDAC7E";
    if (hasKey(&t->dv_entries, key)) {
        uint64_t seconds;
        if (extractDVlong(&t->dv_entries, key, &offset, &seconds))
        {
            duration_since_readout_s_ = (int)seconds;
            t->addMoreExplanation(offset, " duration (%d s)", duration_since_readout_s_);
        }
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
