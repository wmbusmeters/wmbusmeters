/*
 Copyright (C) 2017-2020 Fredrik Öhrström
 Copyright (C) 2018 David Mallon

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

using namespace std;

struct MeterEV200 : public virtual WaterMeter, public virtual MeterCommonImplementation {
    MeterEV200(MeterInfo &mi);

    // Total water counted through the meter
    double totalWaterConsumption(Unit u);
    bool  hasTotalWaterConsumption();
    double targetWaterConsumption(Unit u);
    bool   hasTargetWaterConsumption();

private:
    void processContent(Telegram *t);

    double actual_total_water_consumption_m3_ {};
    double last_total_water_consumption_m3h_ {};
};

MeterEV200::MeterEV200(MeterInfo &mi) :
    MeterCommonImplementation(mi, MeterDriver::EV200)
{
    setExpectedTPLSecurityMode(TPLSecurityMode::AES_CBC_IV);

    addLinkMode(LinkMode::T1);

    // version 0x68
    // version 0x7c Sensus 640

    addPrint("total", Quantity::Volume,
             [&](Unit u){ return totalWaterConsumption(u); },
             "The total water consumption recorded by this meter.",
             true, true);

    addPrint("target", Quantity::Volume,
             [&](Unit u){ return targetWaterConsumption(u); },
             "The target water consumption recorded at previous period.",
             true, true);
}

shared_ptr<Meter> createEV200(MeterInfo &mi)
{
    return shared_ptr<WaterMeter>(new MeterEV200(mi));
}

void MeterEV200::processContent(Telegram *t)
{
    int offset;
    string key;

    if(findKey(MeasurementType::Unknown, ValueInformation::Volume, 0, 0, &key, &t->values)) {
        extractDVdouble(&t->values, key, &offset, &actual_total_water_consumption_m3_);
        t->addMoreExplanation(offset, " actual total consumption (%f m3)", actual_total_water_consumption_m3_);
    }

    if(findKey(MeasurementType::Unknown, ValueInformation::Volume, 1, 0, &key, &t->values)) {
        extractDVdouble(&t->values, key, &offset, &last_total_water_consumption_m3h_);
        t->addMoreExplanation(offset, " last total consumption (%f m3)", last_total_water_consumption_m3h_);
    }


}

double MeterEV200::totalWaterConsumption(Unit u)
{
    assertQuantity(u, Quantity::Volume);
    return convert(actual_total_water_consumption_m3_, Unit::M3, u);
}

bool MeterEV200::hasTotalWaterConsumption()
{
    return true;
}

double MeterEV200::targetWaterConsumption(Unit u)
{
    assertQuantity(u, Quantity::Volume);
    return convert(last_total_water_consumption_m3h_, Unit::M3, u);
}

bool MeterEV200::hasTargetWaterConsumption()
{
    return true;
}
