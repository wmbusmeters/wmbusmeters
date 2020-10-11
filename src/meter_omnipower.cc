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
    double totalEnergyBackward(Unit u);
    double powerConsumption(Unit u);
    double powerBackward(Unit u);

private:

    void processContent(Telegram *t);

    double total_energy_kwh_{};
    double total_energy_backward_kwh_{};
    double power_kw_{};
    double power_backward_kw_{};

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

    addPrint("total_energy_backward", Quantity::Energy,
             [&](Unit u){ return totalEnergyBackward(u); },
             "The total energy backward recorded by this meter.",
             true, true);

    addPrint("power_consumption", Quantity::Power,
             [&](Unit u){ return powerConsumption(u); },
             "The current power consumption on this meter.",
             true, true);

    addPrint("power_backward", Quantity::Power,
             [&](Unit u){ return powerBackward(u); },
             "The current power backward on this meter.",
             true, true);
}

double MeterOmnipower::totalEnergyConsumption(Unit u)
{
    assertQuantity(u, Quantity::Energy);
    return convert(total_energy_kwh_, Unit::KWH, u);
}

double MeterOmnipower::totalEnergyBackward(Unit u)
{
    assertQuantity(u, Quantity::Energy);
    return convert(total_energy_backward_kwh_, Unit::KWH, u);
}

double MeterOmnipower::powerConsumption(Unit u)
{
    assertQuantity(u, Quantity::Energy);
    return convert(power_kw_, Unit::KW, u);
}

double MeterOmnipower::powerBackward(Unit u)
{
    assertQuantity(u, Quantity::Energy);
    return convert(power_backward_kw_, Unit::KW, u);
}


void MeterOmnipower::processContent(Telegram *t)
{
    // Meter record:
    // 04 dif (32 Bit Integer/Binary Instantaneous value)
    // 83 vif (Energy Wh)
    // 3b vife (Forward flow contribution only)
    // xx xx xx xx (total energy)

    //"040404843C042B04AB3C"

    int offset;
    extractDVdouble(&t->values, "0404", &offset, &total_energy_kwh_);
    t->addMoreExplanation(offset, " total energy (%f kwh)", total_energy_kwh_);

    extractDVdouble(&t->values, "04843C", &offset, &total_energy_backward_kwh_);
    t->addMoreExplanation(offset, " total energy backward (%f kwh)", total_energy_backward_kwh_);

    extractDVdouble(&t->values, "042B", &offset, &power_kw_);
    t->addMoreExplanation(offset, " current power (%f kw)", power_kw_);

    extractDVdouble(&t->values, "042B", &offset, &power_backward_kw_);
    t->addMoreExplanation(offset, " current power (%f kw)", power_backward_kw_);

    //std::cout << "Offset = " << offset << std::endl; // this is always 0

//    for (auto k: t->values) {
//        std::cout << k.first << ": " << k.second.first << ": " << "DVEntry here" << std::endl;
//    }
}
