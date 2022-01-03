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
#include"util.h"

using namespace std;

struct MeterDME_07 : public virtual MeterCommonImplementation {
    MeterDME_07(MeterInfo &mi);

    // Total water counted through the meter
    double totalWaterConsumption(Unit u);
    bool  hasTotalWaterConsumption();

private:
    void processContent(Telegram *t);
    string status();

    double total_water_consumption_m3_ {};
    uint16_t error_codes_ {};
};

shared_ptr<Meter> createDME_07(MeterInfo &mi)
{
    return shared_ptr<Meter>(new MeterDME_07(mi));
}

MeterDME_07::MeterDME_07(MeterInfo &mi) :
    MeterCommonImplementation(mi, "dme_07")
{
    setMeterType(MeterType::WaterMeter);

    setExpectedTPLSecurityMode(TPLSecurityMode::AES_CBC_IV);

    addLinkMode(LinkMode::T1);

    addPrint("total", Quantity::Volume,
             [&](Unit u){ return totalWaterConsumption(u); },
             "The total water consumption recorded by this meter.",
             true, true);

    addPrint("status", Quantity::Text,
             [&](){ return status(); },
             "Status of meter.",
             true, true);
}

void MeterDME_07::processContent(Telegram *t)
{
    int offset;
    string key;

    if(findKey(MeasurementType::Instantaneous, ValueInformation::Volume, 0, 0, &key, &t->values)) {
        extractDVdouble(&t->values, key, &offset, &total_water_consumption_m3_);
        t->addMoreExplanation(offset, " total consumption (%f m3)", total_water_consumption_m3_);
    }

    extractDVuint16(&t->values, "02FD17", &offset, &error_codes_);
    t->addMoreExplanation(offset, " error codes (%s)", status().c_str());
}

double MeterDME_07::totalWaterConsumption(Unit u)
{
    assertQuantity(u, Quantity::Volume);
    return convert(total_water_consumption_m3_, Unit::M3, u);
}

bool MeterDME_07::hasTotalWaterConsumption()
{
    return true;
}

string MeterDME_07::status()
{
    if (error_codes_ == 0) return "OK";
    // Can we decode the error bits more?
    return tostrprintf("ERR %04x", error_codes_);
}
