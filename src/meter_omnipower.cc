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
#include <iostream>

struct MeterOmnipower : public virtual ElectricityMeter, public virtual MeterCommonImplementation {
    MeterOmnipower(WMBus *bus, MeterInfo &mi);

    double totalEnergyConsumption(Unit u);

    double unknownField1(Unit u);
    double unknownField2(Unit u);
    double unknownField3(Unit u);
    double unknownField4(Unit u);

private:

    void processContent(Telegram *t);

    double total_energy_kwh_{};
    double field_1_ {};
    double field_2_ {};
    double field_3_ {};
    double field_4_ {};
};

unique_ptr<ElectricityMeter> createOmnipower(WMBus *bus, MeterInfo &mi)
{
    return unique_ptr<ElectricityMeter>(new MeterOmnipower(bus, mi));
}

MeterOmnipower::MeterOmnipower(WMBus *bus, MeterInfo &mi) :
    MeterCommonImplementation(bus, mi, MeterType::OMNIPOWER)
{
    setExpectedTPLSecurityMode(TPLSecurityMode::AES_CBC_IV);

    addLinkMode(LinkMode::C1);

    addPrint("total_energy_consumption", Quantity::Energy,
             [&](Unit u){ return totalEnergyConsumption(u); },
             "The total energy consumption recorded by this meter.",
             true, true);
}

double MeterOmnipower::totalEnergyConsumption(Unit u)
{
    assertQuantity(u, Quantity::Energy);
    return convert(total_energy_kwh_, Unit::KWH, u);
}

double MeterOmnipower::unknownField1(Unit u)
{
    assertQuantity(u, Quantity::Energy);
    return convert(field_1_, Unit::KWH, u);
}

double MeterOmnipower::unknownField2(Unit u)
{
    assertQuantity(u, Quantity::Energy);
    return convert(field_2_, Unit::KWH, u);
}

double MeterOmnipower::unknownField3(Unit u)
{
    assertQuantity(u, Quantity::Energy);
    return convert(field_3_, Unit::KWH, u);
}

double MeterOmnipower::unknownField4(Unit u)
{
    assertQuantity(u, Quantity::Energy);
    return convert(field_4_, Unit::KWH, u);
}


void MeterOmnipower::processContent(Telegram *t)
{
    // Meter record:
    // 04 dif (32 Bit Integer/Binary Instantaneous value)
    // 83 vif (Energy Wh)
    // 3b vife (Forward flow contribution only)
    // xx xx xx xx (total energy)

    int offset;
    extractDVdouble(&t->values, "04833B", &offset, &total_energy_kwh_);
    t->addMoreExplanation(offset, " total power (%f kwh)", total_energy_kwh_);

    extractDVdouble(&t->values, "04833B", &offset, &total_energy_kwh_);
    t->addMoreExplanation(offset, " total power (%f kwh)", total_energy_kwh_);

    //std::cout << "Offset = " << offset << std::endl; // this is always 0

    for (auto k: t->values) {
        std::cout << k.first << ": " << k.second.first << ": " << "DVEntry here" << std::endl;
    }
}
