/*
 Copyright (C) 2017-2019 Fredrik Öhrström
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
#include"util.h"

#include<assert.h>
#include<map>
#include<memory.h>
#include<stdio.h>
#include<string>
#include<time.h>
#include<vector>

using namespace std;

struct MeterIperl : public virtual WaterMeter, public virtual MeterCommonImplementation {
    MeterIperl(WMBus *bus, string& name, string& id, string& key);

    // Total water counted through the meter
    double totalWaterConsumption(Unit u);
    bool  hasTotalWaterConsumption();
    double maxFlow(Unit u);
    bool  hasMaxFlow();

private:
    void processContent(Telegram *t);

    double total_water_consumption_m3_ {};
    double max_flow_m3h_ {};
};

MeterIperl::MeterIperl(WMBus *bus, string& name, string& id, string& key) :
    MeterCommonImplementation(bus, name, id, key, MeterType::IPERL, MANUFACTURER_SEN, LinkMode::T1)
{
    setEncryptionMode(EncryptionMode::AES_CBC);

    addMedia(0x06);
    addMedia(0x07);

    setExpectedVersion(0x68);

    addPrint("total", Quantity::Volume,
             [&](Unit u){ return totalWaterConsumption(u); },
             "The total water consumption recorded by this meter.",
             true, true);

    addPrint("max_flow", Quantity::Flow,
             [&](Unit u){ return maxFlow(u); },
             "The maxium flow recorded during previous period.",
             true, true);

    MeterCommonImplementation::bus()->onTelegram(calll(this,handleTelegram,Telegram*));
}

unique_ptr<WaterMeter> createIperl(WMBus *bus, string& name, string& id, string& key)
{
    return unique_ptr<WaterMeter>(new MeterIperl(bus,name,id,key));
}

void MeterIperl::processContent(Telegram *t)
{
    map<string,pair<int,DVEntry>> values;
    parseDV(t, t->content, t->content.begin(), t->content.size(), &values);

    int offset;
    string key;

    if(findKey(ValueInformation::Volume, 0, &key, &values)) {
        extractDVdouble(&values, key, &offset, &total_water_consumption_m3_);
        t->addMoreExplanation(offset, " total consumption (%f m3)", total_water_consumption_m3_);
    }

    if(findKey(ValueInformation::VolumeFlow, ANY_STORAGENR, &key, &values)) {
        extractDVdouble(&values, key, &offset, &max_flow_m3h_);
        t->addMoreExplanation(offset, " max flow (%f m3/h)", max_flow_m3h_);
    }
}

double MeterIperl::totalWaterConsumption(Unit u)
{
    assertQuantity(u, Quantity::Volume);
    return convert(total_water_consumption_m3_, Unit::M3, u);
}

bool MeterIperl::hasTotalWaterConsumption()
{
    return true;
}

double MeterIperl::maxFlow(Unit u)
{
    assertQuantity(u, Quantity::Flow);
    return convert(max_flow_m3h_, Unit::M3H, u);
}

bool MeterIperl::hasMaxFlow()
{
    return true;
}
