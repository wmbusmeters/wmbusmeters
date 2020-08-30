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

#include"dvparser.h"
#include"meters.h"
#include"meters_common_implementation.h"
#include"wmbus.h"
#include"wmbus_utils.h"
#include"util.h"

using namespace std;

struct MeterQ400 : public virtual WaterMeter, public virtual MeterCommonImplementation {
    MeterQ400(MeterInfo &mi);

    // Total water counted through the meter
    double totalWaterConsumption(Unit u);
    bool  hasTotalWaterConsumption();

    string setDate();
    double consumptionAtSetDate(Unit u);

private:
    void processContent(Telegram *t);

    double total_water_consumption_m3_ {};
    string set_date_;
    double consumption_at_set_date_m3_ {};
};

unique_ptr<WaterMeter> createQ400(MeterInfo &mi)
{
    return unique_ptr<WaterMeter>(new MeterQ400(mi));
}

MeterQ400::MeterQ400(MeterInfo &mi) :
    MeterCommonImplementation(mi, MeterType::Q400, MANUFACTURER_AXI)
{
    setExpectedTPLSecurityMode(TPLSecurityMode::AES_CBC_IV);

    addMedia(0x07);

    addLinkMode(LinkMode::T1);

    addExpectedVersion(0x10);

    addPrint("total", Quantity::Volume,
             [&](Unit u){ return totalWaterConsumption(u); },
             "The total water consumption recorded by this meter.",
             true, true);

    addPrint("set_date", Quantity::Text,
             [&](){ return setDate(); },
             "The most recent billing period date.",
             false, true);

    addPrint("consumption_at_set_date", Quantity::Volume,
             [&](Unit u){ return consumptionAtSetDate(u); },
             "The total water consumption at the most recent billing period date.",
             false, true);
}

void MeterQ400::processContent(Telegram *t)
{
    /*
      (q400) 0f: 2f2f decrypt check bytes
      (q400) 11: 04 dif (32 Bit Integer/Binary Instantaneous value)
      (q400) 12: 6D vif (Date and time type)
      (q400) 13: 040D742C
      (q400) 17: 04 dif (32 Bit Integer/Binary Instantaneous value)
      (q400) 18: 13 vif (Volume l)
      (q400) 19: * 00000000 total consumption (0.000000 m3)
      (q400) 1d: 44 dif (32 Bit Integer/Binary Instantaneous value storagenr=1)
      (q400) 1e: 6D vif (Date and time type)
      (q400) 1f: * 0000612C set date (2019-12-01)
      (q400) 23: 44 dif (32 Bit Integer/Binary Instantaneous value storagenr=1)
      (q400) 24: 13 vif (Volume l)
      (q400) 25: * 00000000 consumption at set date (0.000000 m3)
      (q400) 29: 2F skip
      (q400) 2a: 2F skip
      (q400) 2b: 2F skip
      (q400) 2c: 2F skip
      (q400) 2d: 2F skip
      (q400) 2e: 2F skip
    */
    int offset;
    string key;

    if(findKey(MeasurementType::Unknown, ValueInformation::Volume, 0, 0, &key, &t->values)) {
        extractDVdouble(&t->values, key, &offset, &total_water_consumption_m3_);
        t->addMoreExplanation(offset, " total consumption (%f m3)", total_water_consumption_m3_);
    }

    if(findKey(MeasurementType::Unknown, ValueInformation::Volume, 1, 0, &key, &t->values)) {
        extractDVdouble(&t->values, key, &offset, &consumption_at_set_date_m3_);
        t->addMoreExplanation(offset, " consumption at set date (%f m3)", consumption_at_set_date_m3_);
    }

    if (findKey(MeasurementType::Unknown, ValueInformation::DateTime, 1, 0, &key, &t->values)) {
        struct tm date;
        extractDVdate(&t->values, key, &offset, &date);
        set_date_ = strdate(&date);
        t->addMoreExplanation(offset, " set date (%s)", set_date_.c_str());
    }
}

double MeterQ400::totalWaterConsumption(Unit u)
{
    assertQuantity(u, Quantity::Volume);
    return convert(total_water_consumption_m3_, Unit::M3, u);
}

bool MeterQ400::hasTotalWaterConsumption()
{
    return true;
}

string MeterQ400::setDate()
{
    return set_date_;
}

double MeterQ400::consumptionAtSetDate(Unit u)
{
    assertQuantity(u, Quantity::Volume);
    return convert(consumption_at_set_date_m3_, Unit::M3, u);
}
