/*
 Copyright (C) 2018-2019 Fredrik Öhrström

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

#include<map>
#include<memory.h>
#include<stdio.h>
#include<string>
#include<time.h>
#include<vector>

struct MeterOmnipower : public virtual ElectricityMeter, public virtual MeterCommonImplementation {
    MeterOmnipower(WMBus *bus, string& name, string& id, string& key);

    double totalEnergyConsumption(Unit u);

private:

    void processContent(Telegram *t);

    double total_energy_kwh_ {};
};

unique_ptr<ElectricityMeter> createOmnipower(WMBus *bus, string& name, string& id, string& key)
{
    return unique_ptr<ElectricityMeter>(new MeterOmnipower(bus,name,id,key));
}

MeterOmnipower::MeterOmnipower(WMBus *bus, string& name, string& id, string& key) :
    MeterCommonImplementation(bus, name, id, key, MeterType::OMNIPOWER, MANUFACTURER_KAM, LinkMode::C1)
{
    setEncryptionMode(EncryptionMode::AES_CBC);

    addMedia(0x02);

    setExpectedVersion(0x01);

    addPrint("total_energy_consumption", Quantity::Energy,
             [&](Unit u){ return totalEnergyConsumption(u); },
             "The total energy consumption recorded by this meter.",
             true, true);

    MeterCommonImplementation::bus()->onTelegram(calll(this,handleTelegram,Telegram*));
}

double MeterOmnipower::totalEnergyConsumption(Unit u)
{
    assertQuantity(u, Quantity::Energy);
    return convert(total_energy_kwh_, Unit::KWH, u);
}

void MeterOmnipower::processContent(Telegram *t)
{
    // Meter record:
    // 04 dif (32 Bit Integer/Binary Instantaneous value)
    // 83 vif (Energy Wh)
    // 3b vife (Forward flow contribution only)
    // xx xx xx xx (total energy)

    map<string,pair<int,DVEntry>> values;
    parseDV(t, t->content, t->content.begin(), t->content.size(), &values);

    int offset;
    extractDVdouble(&values, "04833B", &offset, &total_energy_kwh_);
    t->addMoreExplanation(offset, " total power (%f kwh)", total_energy_kwh_);
}
