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

#include"meters.h"
#include"meters_common_implementation.h"
#include"dvparser.h"
#include"wmbus.h"
#include"wmbus_utils.h"
#include"util.h"

struct MeterQWater : public virtual MeterCommonImplementation {
    MeterQWater(MeterInfo &mi);

    double totalWaterConsumption(Unit u);
    string status();

private:
    void processContent(Telegram *t);

    double total_water_consumption_m3_ {};

    // This is the measurement at the end of last year. Stored in storage 1.
    string last_year_date_ {};
    double last_year_water_m3_ {};

    // For some reason the last month is stored in storage nr 17....woot?
    string last_month_date_ {};
    double last_month_water_m3_ {};

    string device_date_time_ {};

    string device_error_date_ {};
};

MeterQWater::MeterQWater(MeterInfo &mi) :
    MeterCommonImplementation(mi, MeterDriver::QWATER)
{
    setMeterType(MeterType::WaterMeter);

    setExpectedELLSecurityMode(ELLSecurityMode::AES_CTR);

    addLinkMode(LinkMode::C1);

    addPrint("total_water_consumption", Quantity::Volume,
             [&](Unit u){ return totalWaterConsumption(u); },
             "The total water consumption recorded by this meter.",
             true, true);

    addPrint("last_month_date", Quantity::Text,
             [&](){ return last_month_date_; },
             "Last day previous month when total water consumption was recorded.",
             true, true);

    addPrint("last_month_water_consumption", Quantity::Volume,
             [&](Unit u){ assertQuantity(u, Quantity::Volume); return convert(last_month_water_m3_, Unit::M3, u); },
             "The total water consumption recorded at the last day of the previous month.",
             true, true);

    addPrint("last_year_date", Quantity::Text,
             [&](){ return last_year_date_; },
             "Last day previous year when total energy consumption was recorded.",
             false, true);

    addPrint("last_year_water_consumption", Quantity::Volume,
             [&](Unit u){ assertQuantity(u, Quantity::Volume); return convert(last_year_water_m3_, Unit::M3, u); },
             "The total water consumption recorded at the last day of the previous year.",
             false, true);

    addPrint("device_date_time", Quantity::Text,
             [&](){ return device_date_time_; },
             "Device date time.",
             false, true);

    addPrint("device_error_date", Quantity::Text,
             [&](){ return device_error_date_; },
             "Device error date.",
             false, true);

}

shared_ptr<Meter> createQWater(MeterInfo &mi) {
    return shared_ptr<Meter>(new MeterQWater(mi));
}

double MeterQWater::totalWaterConsumption(Unit u)
{
    assertQuantity(u, Quantity::Volume);
    return convert(total_water_consumption_m3_, Unit::M3, u);
}

void MeterQWater::processContent(Telegram *t)
{
    /*
    (wmbus) 015   : 0C dif (8 digit BCD Instantaneous value)
    (wmbus) 016   : 13 vif (Volume l)
    (wmbus) 017 C?: 78550200

    (wmbus) 021   : 4C dif (8 digit BCD Instantaneous value storagenr=1)
    (wmbus) 022   : 13 vif (Volume l)
    (wmbus) 023 C?: 39220200

    (wmbus) 027   : 42 dif (16 Bit Integer/Binary Instantaneous value storagenr=1)
    (wmbus) 028   : 6C vif (Date type G)
    (wmbus) 029 C?: BF2A

    (wmbus) 031   : CC dif (8 digit BCD Instantaneous value storagenr=1)
    (wmbus) 032   : 08 dife (subunit=0 tariff=0 storagenr=17)
    (wmbus) 033   : 13 vif (Volume l)
    (wmbus) 034 C?: 30420200

    (wmbus) 038   : C2 dif (16 Bit Integer/Binary Instantaneous value storagenr=1)
    (wmbus) 039   : 08 dife (subunit=0 tariff=0 storagenr=17)
    (wmbus) 040   : 6C vif (Date type G)
    (wmbus) 041 C?: BE2B

    (wmbus) 043   : 02 dif (16 Bit Integer/Binary Instantaneous value)
    (wmbus) 044   : BB vif (Volume flow l/h)
    (wmbus) 045   : 56 vife (duration of limit exceed last lower  is 2)
    (wmbus) 046 C?: 0000

    (wmbus) 048   : 32 dif (16 Bit Integer/Binary Value during error state)
    (wmbus) 049   : 6C vif (Date type G)
    (wmbus) 050 C?: FFFF

    (wmbus) 052   : 04 dif (32 Bit Integer/Binary Instantaneous value)
    (wmbus) 053   : 6D vif (Date and time type)
    (wmbus) 054 C?: 0A00B62C
    */

    int offset;
    string key;

    if(findKey(MeasurementType::Instantaneous, ValueInformation::Volume, 0, 0, &key, &t->values)) {
        extractDVdouble(&t->values, key, &offset, &total_water_consumption_m3_);
        t->addMoreExplanation(offset, " total water consumption (%f m3)", total_water_consumption_m3_);
    }

    if(findKey(MeasurementType::Instantaneous, ValueInformation::Volume, 1, 0, &key, &t->values)) {
        extractDVdouble(&t->values, key, &offset, &last_year_water_m3_);
        t->addMoreExplanation(offset, " last year water consumption (%f m3)", last_year_water_m3_);
    }

    if (findKey(MeasurementType::Unknown, ValueInformation::Date, 1, 0, &key, &t->values)) {
        struct tm datetime;
        extractDVdate(&t->values, key, &offset, &datetime);
        last_year_date_ = strdatetime(&datetime);
        t->addMoreExplanation(offset, " last year date (%s)", last_year_date_.c_str());
    }

    if(findKey(MeasurementType::Instantaneous, ValueInformation::Volume, 17, 0, &key, &t->values)) {
        extractDVdouble(&t->values, key, &offset, &last_month_water_m3_);
        t->addMoreExplanation(offset, " last month water consumption (%f m3)", last_month_water_m3_);
    }

    if (findKey(MeasurementType::Unknown, ValueInformation::Date, 17, 0, &key, &t->values)) {
        struct tm datetime;
        extractDVdate(&t->values, key, &offset, &datetime);
        last_month_date_ = strdatetime(&datetime);
        t->addMoreExplanation(offset, " last month date (%s)", last_month_date_.c_str());
    }

    if (findKey(MeasurementType::AtError, ValueInformation::Date, 0, 0, &key, &t->values)) {
        struct tm datetime;
        extractDVdate(&t->values, key, &offset, &datetime);
        device_error_date_ = strdatetime(&datetime);
        t->addMoreExplanation(offset, " device error date (%s)", device_error_date_.c_str());
    }

    if (findKey(MeasurementType::Unknown, ValueInformation::DateTime, 0, 0, &key, &t->values)) {
            struct tm datetime;
        extractDVdate(&t->values, key, &offset, &datetime);
        device_date_time_ = strdatetime(&datetime);
        t->addMoreExplanation(offset, " device datetime (%s)", device_date_time_.c_str());
    }

}

string MeterQWater::status()
{
    return "";
}
