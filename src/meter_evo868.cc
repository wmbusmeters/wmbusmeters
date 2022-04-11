/*
 Copyright (C) 2017-2020 Fredrik Öhrström (gpl-3.0-or-later)

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
#include"util.h"

#include <algorithm>

using namespace std;

struct MeterEvo868 : public virtual MeterCommonImplementation {
    MeterEvo868(MeterInfo &mi);

    // Total water counted through the meter
    double totalWaterConsumption(Unit u);
    bool  hasTotalWaterConsumption();
    uint32_t error_flags_ {};
    string fabrication_no_;
    double consumption_at_set_date_m3_ {};
    string set_date_;
    double consumption_at_set_date_2_m3_ {};
    string set_date_2_;
    double max_flow_since_datetime_m3h_ {};
    string max_flow_datetime_;

    double consumption_at_history_date_m3_[12];
    string history_date_[12];

    string status();

private:
    void processContent(Telegram *t);

    double total_water_consumption_m3_ {};
    string device_date_time_;
};

shared_ptr<Meter> createEVO868(MeterInfo &mi)
{
    return shared_ptr<Meter>(new MeterEvo868(mi));
}

MeterEvo868::MeterEvo868(MeterInfo &mi) :
    MeterCommonImplementation(mi, "evo868")
{
    setMeterType(MeterType::WaterMeter);

    setExpectedTPLSecurityMode(TPLSecurityMode::AES_CBC_IV);

    addLinkMode(LinkMode::T1);

    addPrint("total", Quantity::Volume,
             [&](Unit u){ return totalWaterConsumption(u); },
             "The total water consumption recorded by this meter.",
             PrintProperty::FIELD | PrintProperty::JSON);

    addPrint("device_date_time", Quantity::Text,
             [&](){ return device_date_time_; },
             "Device date time.",
             PrintProperty::JSON);

    addPrint("current_status", Quantity::Text,
             [&](){ return status(); },
             "Status of meter.",
             PrintProperty::FIELD | PrintProperty::JSON);

    addPrint("fabrication_no", Quantity::Text,
             [&](){ return fabrication_no_; },
             "Fabrication number.",
             PrintProperty::JSON);

    addPrint("consumption_at_set_date", Quantity::Volume,
             [&](Unit u){ assertQuantity(u, Quantity::Volume); return convert(consumption_at_set_date_m3_, Unit::M3, u); },
             "The total water consumption at the most recent billing period date.",
             PrintProperty::FIELD | PrintProperty::JSON);

    addPrint("set_date", Quantity::Text,
             [&](){ return set_date_; },
             "The most recent billing period date.",
             PrintProperty::FIELD | PrintProperty::JSON);

    addPrint("consumption_at_set_date_2", Quantity::Volume,
             [&](Unit u){ assertQuantity(u, Quantity::Volume); return convert(consumption_at_set_date_2_m3_, Unit::M3, u); },
             "The total water consumption at the second recent billing period date.",
             PrintProperty::JSON);

    addPrint("set_date_2", Quantity::Text,
             [&](){ return set_date_2_; },
             "The second recent billing period date.",
             PrintProperty::JSON);

    addPrint("max_flow_since_datetime", Quantity::Flow,
             [&](Unit u){ assertQuantity(u, Quantity::Flow); return convert(max_flow_since_datetime_m3h_, Unit::M3H, u); },
             "Maximum water flow since date time.",
             PrintProperty::JSON);

    addPrint("max_flow_datetime", Quantity::Text,
             [&](){ return max_flow_datetime_; },
             "The datetime to which maximum flow is measured.",
             PrintProperty::JSON);

    for (int i=1; i<=12; ++i)
    {
        string key = tostrprintf("consumption_at_history_%d", i);
        string epl = tostrprintf("The total water consumption at the history date %d.", i);

        addPrint(key, Quantity::Volume,
             [this,i](Unit u){ assertQuantity(u, Quantity::Volume); return convert(consumption_at_history_date_m3_[i-1], Unit::M3, u); },
             epl,
             PrintProperty::JSON);
        key = tostrprintf("history_%d_date", i);
        epl = tostrprintf("The history date %d.", i);

        addPrint(key, Quantity::Text,
                 [this,i](){ return history_date_[i-1]; },
                 epl,
                 PrintProperty::JSON);
    }

}

void MeterEvo868::processContent(Telegram *t)
{
    /*
(evo868) 11: 04 dif (32 Bit Integer/Binary Instantaneous value)
(evo868) 12: 13 vif (Volume l)
(evo868) 13: * 06070000 total consumption (1.798000 m3)

(evo868) 17: 04 dif (32 Bit Integer/Binary Instantaneous value)
(evo868) 18: 6D vif (Date and time type)
(evo868) 19: 1E31B121

(evo868) 1d: 04 dif (32 Bit Integer/Binary Instantaneous value)
(evo868) 1e: FD vif (Second extension of VIF-codes)
(evo868) 1f: 17 vife (Error flags (binary))
(evo868) 20: 00000000

(evo868) 24: 0E dif (12 digit BCD Instantaneous value)
(evo868) 25: 78 vif (Fabrication no)
(evo868) 26: 788004812000

(evo868) 2c: 44 dif (32 Bit Integer/Binary Instantaneous value storagenr=1)
(evo868) 2d: 13 vif (Volume l)
(evo868) 2e: C9040000

(evo868) 32: 42 dif (16 Bit Integer/Binary Instantaneous value storagenr=1)
(evo868) 33: 6C vif (Date type G)
(evo868) 34: 9F2C

(evo868) 36: 84 dif (32 Bit Integer/Binary Instantaneous value)
(evo868) 37: 01 dife (subunit=0 tariff=0 storagenr=2)
(evo868) 38: 13 vif (Volume l)
(evo868) 39: C9040000

(evo868) 3d: 82 dif (16 Bit Integer/Binary Instantaneous value)
(evo868) 3e: 01 dife (subunit=0 tariff=0 storagenr=2)
(evo868) 3f: 6C vif (Date type G)
(evo868) 40: 9F2C

(evo868) 42: D3 dif (24 Bit Integer/Binary Maximum value storagenr=1)
(evo868) 43: 01 dife (subunit=0 tariff=0 storagenr=3)
(evo868) 44: 3B vif (Volume flow l/h)
(evo868) 45: 9A0200

(evo868) 48: C4 dif (32 Bit Integer/Binary Instantaneous value storagenr=1)
(evo868) 49: 01 dife (subunit=0 tariff=0 storagenr=3)
(evo868) 4a: 6D vif (Date and time type)
(evo868) 4b: 0534A721

(evo868) 4f: 81 dif (8 Bit Integer/Binary Instantaneous value)
(evo868) 50: 04 dife (subunit=0 tariff=0 storagenr=8)
(evo868) 51: FD vif (Second extension of VIF-codes)
(evo868) 52: 28 vife (Storage interval month(s))
(evo868) 53: 01

(evo868) 54: 82 dif (16 Bit Integer/Binary Instantaneous value)
(evo868) 55: 04 dife (subunit=0 tariff=0 storagenr=8)
(evo868) 56: 6C vif (Date type G)
(evo868) 57: 9F2C

(evo868) 59: 84 dif (32 Bit Integer/Binary Instantaneous value)
(evo868) 5a: 04 dife (subunit=0 tariff=0 storagenr=8)
(evo868) 5b: 13 vif (Volume l)
(evo868) 5c: C9040000

(evo868) 60: C4 dif (32 Bit Integer/Binary Instantaneous value storagenr=1)
(evo868) 61: 04 dife (subunit=0 tariff=0 storagenr=9)
(evo868) 62: 13 vif (Volume l)
(evo868) 63: 1B000000

(evo868) 67: 84 dif (32 Bit Integer/Binary Instantaneous value)
(evo868) 68: 05 dife (subunit=0 tariff=0 storagenr=10)
(evo868) 69: 13 vif (Volume l)
(evo868) 6a: 00000000

(evo868) 6e: C4 dif (32 Bit Integer/Binary Instantaneous value storagenr=1)
(evo868) 6f: 05 dife (subunit=0 tariff=0 storagenr=11)
(evo868) 70: 13 vif (Volume l)
(evo868) 71: 00000000

(evo868) 75: 84 dif (32 Bit Integer/Binary Instantaneous value)
(evo868) 76: 06 dife (subunit=0 tariff=0 storagenr=12)
(evo868) 77: 13 vif (Volume l)
(evo868) 78: 00000000

(evo868) 7c: C4 dif (32 Bit Integer/Binary Instantaneous value storagenr=1)
(evo868) 7d: 06 dife (subunit=0 tariff=0 storagenr=13)
(evo868) 7e: 13 vif (Volume l)
(evo868) 7f: 00000000

(evo868) 83: 84 dif (32 Bit Integer/Binary Instantaneous value)
(evo868) 84: 07 dife (subunit=0 tariff=0 storagenr=14)
(evo868) 85: 13 vif (Volume l)
(evo868) 86: 00000000

(evo868) 8a: C4 dif (32 Bit Integer/Binary Instantaneous value storagenr=1)
(evo868) 8b: 07 dife (subunit=0 tariff=0 storagenr=15)
(evo868) 8c: 13 vif (Volume l)
(evo868) 8d: 00000000

(evo868) 91: 84 dif (32 Bit Integer/Binary Instantaneous value)
(evo868) 92: 08 dife (subunit=0 tariff=0 storagenr=16)
(evo868) 93: 13 vif (Volume l)
(evo868) 94: 00000000

(evo868) 98: C4 dif (32 Bit Integer/Binary Instantaneous value storagenr=1)
(evo868) 99: 08 dife (subunit=0 tariff=0 storagenr=17)
(evo868) 9a: 13 vif (Volume l)
(evo868) 9b: 00000000

(evo868) 9f: 84 dif (32 Bit Integer/Binary Instantaneous value)
(evo868) a0: 09 dife (subunit=0 tariff=0 storagenr=18)
(evo868) a1: 13 vif (Volume l)
(evo868) a2: 00000000

(evo868) a6: C4 dif (32 Bit Integer/Binary Instantaneous value storagenr=1)
(evo868) a7: 09 dife (subunit=0 tariff=0 storagenr=19)
(evo868) a8: 13 vif (Volume l)
(evo868) a9: 00000000
     */
    int offset;
    string key;

    if(findKey(MeasurementType::Instantaneous, ValueInformation::Volume, 0, 0, &key, &t->values)) {
        extractDVdouble(&t->values, key, &offset, &total_water_consumption_m3_);
        t->addMoreExplanation(offset, " total consumption (%f m3)", total_water_consumption_m3_);
    }

    if (findKey(MeasurementType::Instantaneous, ValueInformation::DateTime, 0, 0, &key, &t->values)) {
        struct tm datetime;
        extractDVdate(&t->values, key, &offset, &datetime);
        device_date_time_ = strdatetime(&datetime);
        t->addMoreExplanation(offset, " device datetime (%s)", device_date_time_.c_str());
    }

    extractDVuint32(&t->values, "04FD17", &offset, &error_flags_);
    t->addMoreExplanation(offset, " error flags (%s)", status().c_str());

    extractDVReadableString(&t->values, "0E78", &offset, &fabrication_no_);
    t->addMoreExplanation(offset, " fabrication no (%s)", fabrication_no_.c_str());

    if(findKey(MeasurementType::Instantaneous, ValueInformation::Volume, 1, 0, &key, &t->values)) {
        extractDVdouble(&t->values, key, &offset, &consumption_at_set_date_m3_);
        t->addMoreExplanation(offset, " consumption at set date (%f m3)", consumption_at_set_date_m3_);
    }

    if (findKey(MeasurementType::Instantaneous, ValueInformation::Date, 1, 0, &key, &t->values)) {
        struct tm date;
        extractDVdate(&t->values, key, &offset, &date);
        set_date_ = strdate(&date);
        t->addMoreExplanation(offset, " set date (%s)", set_date_.c_str());
    }

    if(findKey(MeasurementType::Instantaneous, ValueInformation::Volume, 2, 0, &key, &t->values)) {
        extractDVdouble(&t->values, key, &offset, &consumption_at_set_date_2_m3_);
        t->addMoreExplanation(offset, " consumption at set date 2 (%f m3)", consumption_at_set_date_2_m3_);
    }

    if (findKey(MeasurementType::Instantaneous, ValueInformation::Date, 2, 0, &key, &t->values)) {
        struct tm date;
        extractDVdate(&t->values, key, &offset, &date);
        set_date_2_ = strdate(&date);
        t->addMoreExplanation(offset, " set date 2 (%s)", set_date_2_.c_str());
    }

    if(findKey(MeasurementType::Maximum, ValueInformation::VolumeFlow, 3, 0, &key, &t->values)) {
        extractDVdouble(&t->values, key, &offset, &max_flow_since_datetime_m3h_);
        t->addMoreExplanation(offset, " max flow (%f m3/h)", max_flow_since_datetime_m3h_);
    }

    if (findKey(MeasurementType::Instantaneous, ValueInformation::DateTime, 3, 0, &key, &t->values)) {
        struct tm datetime;
        extractDVdate(&t->values, key, &offset, &datetime);
        max_flow_datetime_ = strdatetime(&datetime);
        t->addMoreExplanation(offset, " max flow since datetime (%s)", max_flow_datetime_.c_str());
    }

    uint8_t month_increment = 0;
    extractDVuint8(&t->values, "8104FD28", &offset, &month_increment);
    t->addMoreExplanation(offset, " month increment (%d)", month_increment);

    struct tm date;
    if (findKey(MeasurementType::Instantaneous, ValueInformation::Date, 8, 0, &key, &t->values)) {
        extractDVdate(&t->values, key, &offset, &date);
        string start = strdate(&date);
        t->addMoreExplanation(offset, " history starts with date (%s)", start.c_str());
    }

    // 12 months of historical data, starting in storage 8.
    for (int i=1; i<=12; ++i)
    {
        if(findKey(MeasurementType::Instantaneous, ValueInformation::Volume, i+7, 0, &key, &t->values)) {
            extractDVdouble(&t->values, key, &offset, &consumption_at_history_date_m3_[i-1]);
            t->addMoreExplanation(offset, " consumption at history %d (%f m3)", i, consumption_at_history_date_m3_[i-1]);
            struct tm d = date;
            if (i>1) addMonths(&d, 1-i);
            history_date_[i-1] = strdate(&d);
        }
    }
}

double MeterEvo868::totalWaterConsumption(Unit u)
{
    assertQuantity(u, Quantity::Volume);
    return convert(total_water_consumption_m3_, Unit::M3, u);
}

bool MeterEvo868::hasTotalWaterConsumption()
{
    return true;
}

string MeterEvo868::status()
{
    if (error_flags_ == 0)
    {
        return "OK";
    }

    /* Possible errors according to datasheet:
       overflow  (threshold configurable, must  be  activated)
       backflow  (threshold  set,  configurable)
       leak
       meter blocked
       non-used (days threshold set, configurable)
       magnetic and mechanical tampering (removal)
    */

    // How do we decode these?
    string info;
    strprintf(info, "ERROR bits %08x", error_flags_);
    return info;
}
