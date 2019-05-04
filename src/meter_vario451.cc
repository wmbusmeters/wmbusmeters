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
#include"units.h"
#include"util.h"

struct MeterVario451 : public virtual HeatMeter, public virtual MeterCommonImplementation
{
    MeterVario451(WMBus *bus, string& name, string& id, string& key);

    double totalEnergyConsumption(Unit u);
    double currentPeriodEnergyConsumption(Unit u);
    double previousPeriodEnergyConsumption(Unit u);

    private:

    void processContent(Telegram *t);

    double total_energy_gj_ {};
    double curr_energy_gj_ {};
    double prev_energy_gj_ {};
};

unique_ptr<HeatMeter> createVario451(WMBus *bus, string& name, string& id, string& key)
{
    return unique_ptr<HeatMeter>(new MeterVario451(bus,name,id,key));
}

MeterVario451::MeterVario451(WMBus *bus, string& name, string& id, string& key) :
    MeterCommonImplementation(bus, name, id, key, MeterType::VARIO451, MANUFACTURER_TCH, LinkMode::T1)
{
    setEncryptionMode(EncryptionMode::None);

    addMedia(0x04); // C telegrams
    addMedia(0xC3); // T telegrams

    addPrint("total", Quantity::Energy,
             [&](Unit u){ return totalEnergyConsumption(u); },
             "The total energy consumption recorded by this meter.",
             true, true);

    addPrint("current", Quantity::Energy,
             [&](Unit u){ return currentPeriodEnergyConsumption(u); },
             "Energy consumption so far in this billing period.",
             true, true);

    addPrint("previous", Quantity::Energy,
             [&](Unit u){ return previousPeriodEnergyConsumption(u); },
             "Energy consumption in previous billing period.",
             true, true);

    MeterCommonImplementation::bus()->onTelegram(calll(this,handleTelegram,Telegram*));
}

double MeterVario451::totalEnergyConsumption(Unit u)
{
    assertQuantity(u, Quantity::Energy);
    return convert(total_energy_gj_, Unit::GJ, u);
}

double MeterVario451::currentPeriodEnergyConsumption(Unit u)
{
    assertQuantity(u, Quantity::Energy);
    return convert(curr_energy_gj_, Unit::GJ, u);
}

double MeterVario451::previousPeriodEnergyConsumption(Unit u)
{
    assertQuantity(u, Quantity::Energy);
    return convert(prev_energy_gj_, Unit::GJ, u);
}

void MeterVario451::processContent(Telegram *t)
{
    map<string,pair<int,DVEntry>> vendor_values;

    // Unfortunately, the Techem Vario 4 Typ 4.5.1 is mostly a proprieatary protocol
    // simple wrapped inside a wmbus telegram since the ci-field is 0xa2.
    // Which means that the entire payload is manufacturer specific.

    uchar prev_lo = t->content[3];
    uchar prev_hi = t->content[4];
    double prev = (256.0*prev_hi+prev_lo)/1000;

    string prevs;
    strprintf(prevs, "%02x%02x", prev_lo, prev_hi);
    int offset = t->parsed.size()+3;
    vendor_values["0215"] = { offset, DVEntry(0x15, 0, 0, 0, prevs) };
    t->explanations.push_back({ offset, prevs });
    t->addMoreExplanation(offset, " energy used in previous billing period (%f GJ)", prev);

    uchar curr_lo = t->content[7];
    uchar curr_hi = t->content[8];
    double curr = (256.0*curr_hi+curr_lo)/1000;

    string currs;
    strprintf(currs, "%02x%02x", curr_lo, curr_hi);
    offset = t->parsed.size()+7;
    vendor_values["0215"] = { offset, DVEntry(0x15, 0, 0, 0, currs) };
    t->explanations.push_back({ offset, currs });
    t->addMoreExplanation(offset, " energy used in current billing period (%f GJ)", curr);

    total_energy_gj_ = prev+curr;
    curr_energy_gj_ = curr;
    prev_energy_gj_ = prev;
}
