/*
 Copyright (C) 2019 Fredrik Öhrström

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

struct MKRadio3 : public virtual WaterMeter, public virtual MeterCommonImplementation
{
    MKRadio3(WMBus *bus, string& name, string& id, string& key);

    double totalWaterConsumption(Unit u);
    bool  hasTotalWaterConsumption();
    double targetWaterConsumption(Unit u);
    bool  hasTargetWaterConsumption();

private:
    void processContent(Telegram *t);

    double total_water_consumption_m3_ {};
    double target_water_consumption_m3_ {};
};

MKRadio3::MKRadio3(WMBus *bus, string& name, string& id, string& key) :
    MeterCommonImplementation(bus, name, id, key, MeterType::MKRADIO3, MANUFACTURER_TCH, LinkMode::T1)
{
    addMedia(0x62);
    addMedia(0x72);

    addPrint("total", Quantity::Volume,
             [&](Unit u){ return totalWaterConsumption(u); },
             "The total water consumption recorded by this meter.",
             true);

    addPrint("target", Quantity::Volume,
             [&](Unit u){ return targetWaterConsumption(u); },
             "The total water consumption recorded at the beginning of this month.",
             true);

    MeterCommonImplementation::bus()->onTelegram(calll(this,handleTelegram,Telegram*));
}

unique_ptr<WaterMeter> createMKRadio3(WMBus *bus, string& name, string& id, string& key)
{
    return unique_ptr<WaterMeter>(new MKRadio3(bus,name,id,key));
}

void MKRadio3::processContent(Telegram *t)
{
    // Meter record:
    map<string,pair<int,DVEntry>> vendor_values;

    // Unfortunately, the MK Radio 3 is mostly a proprieatary protocol
    // simple wrapped inside a wmbus telegram since the ci-field is 0xa2.
    // Which means that the entire payload is manufacturer specific.

    uchar prev_lo = t->content[3];
    uchar prev_hi = t->content[4];
    double prev = (256.0*prev_hi+prev_lo)/10.0;

    string prevs;
    strprintf(prevs, "%02x%02x", prev_lo, prev_hi);
    int offset = t->parsed.size()+3;
    vendor_values["0215"] = { offset, DVEntry(0x15, 0, 0, 0, prevs) };
    t->explanations.push_back({ offset, prevs });
    t->addMoreExplanation(offset, " prev consumption (%f m3)", prev);

    uchar curr_lo = t->content[7];
    uchar curr_hi = t->content[8];
    double curr = (256.0*curr_hi+curr_lo)/10.0;

    string currs;
    strprintf(currs, "%02x%02x", curr_lo, curr_hi);
    offset = t->parsed.size()+7;
    vendor_values["0215"] = { offset, DVEntry(0x15, 0, 0, 0, currs) };
    t->explanations.push_back({ offset, currs });
    t->addMoreExplanation(offset, " curr consumption (%f m3)", curr);

    total_water_consumption_m3_ = prev+curr;
    target_water_consumption_m3_ = prev;
}

double MKRadio3::totalWaterConsumption(Unit u)
{
    assertQuantity(u, Quantity::Volume);
    return convert(total_water_consumption_m3_, Unit::M3, u);
}

bool MKRadio3::hasTotalWaterConsumption()
{
    return true;
}

double MKRadio3::targetWaterConsumption(Unit u)
{
    return target_water_consumption_m3_;
}

bool MKRadio3::hasTargetWaterConsumption()
{
    return true;
}
