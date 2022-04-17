/*
 Copyright (C) 2019-2020 Fredrik Öhrström (gpl-3.0-or-later)

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

struct MKRadio4 : public virtual MeterCommonImplementation
{
    MKRadio4(MeterInfo &mi);

    double totalWaterConsumption(Unit u);
    bool  hasTotalWaterConsumption();
    double targetWaterConsumption(Unit u);
    bool  hasTargetWaterConsumption();

private:
    void processContent(Telegram *t);

    double total_water_consumption_m3_ {};
    double target_water_consumption_m3_ {};
};

MKRadio4::MKRadio4(MeterInfo &mi) :
    MeterCommonImplementation(mi, "mkradio4")
{
    setMeterType(MeterType::WaterMeter);

    setExpectedTPLSecurityMode(TPLSecurityMode::AES_CBC_IV);

    addLinkMode(LinkMode::T1);

    addPrint("total", Quantity::Volume,
             [&](Unit u){ return totalWaterConsumption(u); },
             "The total water consumption recorded by this meter.",
             PrintProperty::FIELD | PrintProperty::JSON);

    addPrint("target", Quantity::Volume,
             [&](Unit u){ return targetWaterConsumption(u); },
             "The total water consumption recorded at the beginning of this month.",
             PrintProperty::FIELD | PrintProperty::JSON);
}

shared_ptr<Meter> createMKRadio4(MeterInfo &mi)
{
    return shared_ptr<Meter>(new MKRadio4(mi));
}

void MKRadio4::processContent(Telegram *t)
{
    // Unfortunately, the MK Radio 4 is mostly a proprieatary protocol
    // simple wrapped inside a wmbus telegram since the ci-field is 0xa2.
    // Which means that the entire payload is manufacturer specific.

    map<string,pair<int,DVEntry>> vendor_values;
    vector<uchar> content;

    t->extractPayload(&content);

    uchar prev_lo = content[3];
    uchar prev_hi = content[4];
    double prev = (256.0*prev_hi+prev_lo)/10.0;

    string prevs;
    strprintf(prevs, "%02x%02x", prev_lo, prev_hi);
    int offset = t->parsed.size()+3;
    vendor_values["0215"] = { offset, DVEntry(DifVifKey("0215"), MeasurementType::Instantaneous, 0x15, 0, 0, 0, prevs) };
    t->explanations.push_back(Explanation(offset, 2, prevs, KindOfData::CONTENT, Understanding::FULL));
    t->addMoreExplanation(offset, " prev consumption (%f m3)", prev);

    uchar curr_lo = content[7];
    uchar curr_hi = content[8];
    double curr = (256.0*curr_hi+curr_lo)/10.0;

    string currs;
    strprintf(currs, "%02x%02x", curr_lo, curr_hi);
    offset = t->parsed.size()+7;
    vendor_values["0215"] = { offset, DVEntry(DifVifKey("0215"), MeasurementType::Instantaneous, 0x15, 0, 0, 0, currs) };
    t->explanations.push_back(Explanation(offset, 2, currs, KindOfData::CONTENT, Understanding::FULL));
    t->addMoreExplanation(offset, " curr consumption (%f m3)", curr);

    total_water_consumption_m3_ = prev+curr;
    target_water_consumption_m3_ = prev;
}

double MKRadio4::totalWaterConsumption(Unit u)
{
    assertQuantity(u, Quantity::Volume);
    return convert(total_water_consumption_m3_, Unit::M3, u);
}

bool MKRadio4::hasTotalWaterConsumption()
{
    return true;
}

double MKRadio4::targetWaterConsumption(Unit u)
{
    return target_water_consumption_m3_;
}

bool MKRadio4::hasTargetWaterConsumption()
{
    return true;
}
