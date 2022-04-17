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

struct MKRadio3 : public virtual MeterCommonImplementation
{
    MKRadio3(MeterInfo &mi);

    double totalWaterConsumption(Unit u);
    bool  hasTotalWaterConsumption();
    double targetWaterConsumption(Unit u);
    bool  hasTargetWaterConsumption();
    string currentDate();
    string previousDate();

private:
    void processContent(Telegram *t);

    double total_water_consumption_m3_ {};
    double target_water_consumption_m3_ {};
    string current_date_ {};
    string previous_date_ {};
};

MKRadio3::MKRadio3(MeterInfo &mi) :
    MeterCommonImplementation(mi, "mkradio3")
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

    addPrint("current_date", Quantity::Text,
             [&](){ return currentDate(); },
             "Date of current billing period.",
             PrintProperty::FIELD | PrintProperty::JSON);

    addPrint("prev_date", Quantity::Text,
             [&](){ return previousDate(); },
             "Date of previous billing period.",
             PrintProperty::FIELD | PrintProperty::JSON);
}

shared_ptr<Meter> createMKRadio3(MeterInfo &mi)
{
    return shared_ptr<Meter>(new MKRadio3(mi));
}

void MKRadio3::processContent(Telegram *t)
{
    // Unfortunately, the MK Radio 3 is mostly a proprieatary protocol
    // simple wrapped inside a wmbus telegram since the ci-field is 0xa2.
    // Which means that the entire payload is manufacturer specific.

    map<string,pair<int,DVEntry>> vendor_values;
    vector<uchar> content;

    t->extractPayload(&content);

    // Previous date
    uint16_t prev_date = (content[2] << 8) | content[1];
    uint prev_date_day = (prev_date >> 0) & 0x1F;
    uint prev_date_month = (prev_date >> 5) & 0x0F;
    uint prev_date_year = (prev_date >> 9) & 0x3F;
    strprintf(previous_date_, "%d-%02d-%02dT02:00:00Z", prev_date_year+2000, prev_date_month, prev_date_day);

    string prev_date_str;
    strprintf(prev_date_str, "%04x", prev_date);
    uint offset = t->parsed.size() + 1;
    vendor_values["0215"] = { offset, DVEntry(offset, DifVifKey("0215"), MeasurementType::Instantaneous, 0x6c, 0, 0, 0, prev_date_str) };
    t->explanations.push_back(Explanation(offset, 1, prev_date_str, KindOfData::CONTENT, Understanding::FULL));
    t->addMoreExplanation(offset, " previous date (%s)", previous_date_.c_str());

    // Previous consumption
    uchar prev_lo = content[3];
    uchar prev_hi = content[4];
    double prev = (256.0*prev_hi+prev_lo)/10.0;

    string prevs;
    strprintf(prevs, "%02x%02x", prev_lo, prev_hi);
    offset = t->parsed.size()+3;
    vendor_values["0215"] = { offset, DVEntry(offset, DifVifKey("0215"), MeasurementType::Instantaneous, 0x15, 0, 0, 0, prevs) };
    t->explanations.push_back(Explanation(offset, 2, prevs, KindOfData::CONTENT, Understanding::FULL));
    t->addMoreExplanation(offset, " prev consumption (%f m3)", prev);

    // Current date
    uint16_t current_date = (content[6] << 8) | content[5];
    uint current_date_day = (current_date >> 4) & 0x1F;
    uint current_date_month = (current_date >> 9) & 0x0F;
    strprintf(current_date_, "%s-%02d-%02dT02:00:00Z", currentYear().c_str(), current_date_month, current_date_day);

    string current_date_str;
    strprintf(current_date_str, "%04x", current_date);
    offset = t->parsed.size() + 5;
    vendor_values["0215"] = { offset, DVEntry(offset, DifVifKey("0215"), MeasurementType::Instantaneous, 0x6c, 0, 0, 0, current_date_str) };
    t->explanations.push_back(Explanation(offset, 1, current_date_str, KindOfData::CONTENT, Understanding::FULL));
    t->addMoreExplanation(offset, " current date (%s)", current_date_.c_str());

    // Current consumption
    uchar curr_lo = content[7];
    uchar curr_hi = content[8];
    double curr = (256.0*curr_hi+curr_lo)/10.0;

    string currs;
    strprintf(currs, "%02x%02x", curr_lo, curr_hi);
    offset = t->parsed.size()+7;
    vendor_values["0215"] = { offset, DVEntry(offset, DifVifKey("0215"), MeasurementType::Instantaneous, 0x15, 0, 0, 0, currs) };
    t->explanations.push_back(Explanation(offset, 2, currs, KindOfData::CONTENT, Understanding::FULL));
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

string MKRadio3::currentDate() {
    return current_date_;
}

string MKRadio3::previousDate() {
    return previous_date_;
}
