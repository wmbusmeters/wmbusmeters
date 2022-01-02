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

using namespace std;

struct MeterApator08 : public virtual MeterCommonImplementation
{
    MeterApator08(MeterInfo &mi);

    // Total water counted through the meter
    double totalWaterConsumption(Unit u);
    bool  hasTotalWaterConsumption();

private:

    void processContent(Telegram *t);

    double total_water_consumption_m3_ {};
};

shared_ptr<Meter> createApator08(MeterInfo &mi)
{
    return shared_ptr<Meter>(new MeterApator08(mi));
}

MeterApator08::MeterApator08(MeterInfo &mi) :
    MeterCommonImplementation(mi, "apator08")
{
    setMeterType(MeterType::WaterMeter);

    // manufacturer 0x8614 is not compliant with flags encoding.
    // forced decode will decode to APT.
    setExpectedTPLSecurityMode(TPLSecurityMode::AES_CBC_IV);

    addLinkMode(LinkMode::T1);

    addPrint("total", Quantity::Volume,
             [&](Unit u){ return totalWaterConsumption(u); },
             "The total water consumption recorded by this meter.",
             true, true);
}

double MeterApator08::totalWaterConsumption(Unit u)
{
    assertQuantity(u, Quantity::Volume);
    return convert(total_water_consumption_m3_, Unit::M3, u);
}

bool MeterApator08::hasTotalWaterConsumption()
{
    return true;
}

void MeterApator08::processContent(Telegram *t)
{
    // Unfortunately, the at-wmbus-08 is mostly a proprietary protocol
    // simple wrapped inside a wmbus telegram. Naughty!

    // The telegram says gas (0x03) but it is a water meter.... so fix this.
    t->dll_type = 0x07;

    vector<uchar> content;
    t->extractPayload(&content);

    map<string,pair<int,DVEntry>> vendor_values;

    string total;
    strprintf(total, "%02x%02x%02x%02x", content[0], content[1], content[2], content[3]);

    vendor_values["0413"] = { 25, DVEntry(MeasurementType::Instantaneous, 0x13, 0, 0, 0, total) };
    int offset;
    string key;
    if(findKey(MeasurementType::Unknown, ValueInformation::Volume, 0, 0, &key, &vendor_values))
    {
        extractDVdouble(&vendor_values, key, &offset, &total_water_consumption_m3_);
        // Now divide with 3! Is this the same for all apator08 meters? Time will tell.
        total_water_consumption_m3_ /= 3.0;

        //Adding explanation have to wait since it assumes that the dvparser could do something, but it could not here.
        //t->addMoreExplanation(offset, " total consumption (%f m3)", total_water_consumption_m3_);
    }
}
