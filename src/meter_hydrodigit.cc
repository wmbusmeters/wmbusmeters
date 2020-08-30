/*
 Copyright (C) 2019-2020 Fredrik Öhrström

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

struct MeterHydrodigit : public virtual WaterMeter, public virtual MeterCommonImplementation {
    MeterHydrodigit(MeterInfo &mi);

    // Total water counted through the meter
    double totalWaterConsumption(Unit u);
    bool  hasTotalWaterConsumption();

private:
    void processContent(Telegram *t);

    double total_water_consumption_m3_ {};
    string meter_datetime_;
};

unique_ptr<WaterMeter> createHydrodigit(MeterInfo &mi)
{
    return unique_ptr<WaterMeter>(new MeterHydrodigit(mi));
}

MeterHydrodigit::MeterHydrodigit(MeterInfo &mi) :
    MeterCommonImplementation(mi, MeterType::HYDRODIGIT, MANUFACTURER_BMT)
{
    setExpectedTPLSecurityMode(TPLSecurityMode::AES_CBC_IV);

    addMedia(0x07);
    addExpectedVersion(0x13);
    addLinkMode(LinkMode::T1);

    addPrint("total", Quantity::Volume,
             [&](Unit u){ return totalWaterConsumption(u); },
             "The total water consumption recorded by this meter.",
             true, true);

    addPrint("meter_datetime", Quantity::Text,
             [&](){ return meter_datetime_; },
             "Meter timestamp for measurement.",
             true, true);
}

void MeterHydrodigit::processContent(Telegram *t)
{
    int offset;
    string key;

    if(findKey(MeasurementType::Unknown, ValueInformation::Volume, 0, 0, &key, &t->values)) {
        extractDVdouble(&t->values, key, &offset, &total_water_consumption_m3_);
        t->addMoreExplanation(offset, " total consumption (%f m3)", total_water_consumption_m3_);
    }

    if (findKey(MeasurementType::Unknown, ValueInformation::DateTime, 0, 0, &key, &t->values)) {
        struct tm datetime;
        extractDVdate(&t->values, key, &offset, &datetime);
        meter_datetime_ = strdatetime(&datetime);
        t->addMoreExplanation(offset, " meter_datetime (%s)", meter_datetime_.c_str());
    }

    vector<uchar> data;
    t->extractMfctData(&data);
}

double MeterHydrodigit::totalWaterConsumption(Unit u)
{
    assertQuantity(u, Quantity::Volume);
    return convert(total_water_consumption_m3_, Unit::M3, u);
}

bool MeterHydrodigit::hasTotalWaterConsumption()
{
    return true;
}
