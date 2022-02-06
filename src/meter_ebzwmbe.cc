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
#include"util.h"

struct MeterEBZWMBE : public virtual MeterCommonImplementation
{
    MeterEBZWMBE(MeterInfo &mi);

    double totalEnergyConsumption(Unit u);
    double currentPowerConsumption(Unit u);
    double currentPowerConsumptionPhase1(Unit u);
    double currentPowerConsumptionPhase2(Unit u);
    double currentPowerConsumptionPhase3(Unit u);

private:

    void processContent(Telegram *t);

    double total_energy_kwh_ {};
    double current_power_kw_ {};
    double current_power_phase1_kw_ {};
    double current_power_phase2_kw_ {};
    double current_power_phase3_kw_ {};
    string customer_;
};

MeterEBZWMBE::MeterEBZWMBE(MeterInfo &mi) :
    MeterCommonImplementation(mi, "ebzwmbe")
{
    setMeterType(MeterType::ElectricityMeter);

    setExpectedTPLSecurityMode(TPLSecurityMode::AES_CBC_NO_IV);

    // The eBZ wWMB E01 is an addons to the electricity meters
    // media 0x37 Radio converter (meter side)

    addLinkMode(LinkMode::T1);

    addPrint("total_energy_consumption", Quantity::Energy,
             [&](Unit u){ return totalEnergyConsumption(u); },
             "The total energy consumption recorded by this meter.",
             true, true);

    addPrint("current_power_consumption", Quantity::Power,
             [&](Unit u){ return currentPowerConsumption(u); },
             "Current power consumption.",
             true, true);

    addPrint("current_power_consumption_phase1", Quantity::Power,
             [&](Unit u){ return currentPowerConsumptionPhase1(u); },
             "Current power consumption phase 1.",
             true, true);

    addPrint("current_power_consumption_phase2", Quantity::Power,
             [&](Unit u){ return currentPowerConsumptionPhase2(u); },
             "Current power consumption phase 2.",
             true, true);

    addPrint("current_power_consumption_phase3", Quantity::Power,
             [&](Unit u){ return currentPowerConsumptionPhase3(u); },
             "Current power consumption phase 3.",
             true, true);

    addPrint("customer", Quantity::Text,
             [&](){ return customer_; },
             "Customer name.",
             false, true);
}

shared_ptr<Meter> createEBZWMBE(MeterInfo &mi)
{
    return shared_ptr<Meter>(new MeterEBZWMBE(mi));
}

double MeterEBZWMBE::totalEnergyConsumption(Unit u)
{
    assertQuantity(u, Quantity::Energy);
    return convert(total_energy_kwh_, Unit::KWH, u);
}

double MeterEBZWMBE::currentPowerConsumption(Unit u)
{
    assertQuantity(u, Quantity::Power);
    return convert(current_power_kw_, Unit::KW, u);
}

double MeterEBZWMBE::currentPowerConsumptionPhase1(Unit u)
{
    assertQuantity(u, Quantity::Power);
    return convert(current_power_phase1_kw_, Unit::KW, u);
}

double MeterEBZWMBE::currentPowerConsumptionPhase2(Unit u)
{
    assertQuantity(u, Quantity::Power);
    return convert(current_power_phase2_kw_, Unit::KW, u);
}

double MeterEBZWMBE::currentPowerConsumptionPhase3(Unit u)
{
    assertQuantity(u, Quantity::Power);
    return convert(current_power_phase3_kw_, Unit::KW, u);
}

void MeterEBZWMBE::processContent(Telegram *t)
{
    /*
    (ebzwmbe) 2e: 04 dif (32 Bit Integer/Binary Instantaneous value)
    (ebzwmbe) 2f: 03 vif (Energy Wh)
    (ebzwmbe) 30: * 30F92A00 total energy (2816.304000 kwh)
    (ebzwmbe) 34: 04 dif (32 Bit Integer/Binary Instantaneous value)
    (ebzwmbe) 35: A9 vif (Power 10⁻² W)
    (ebzwmbe) 36: FF vife (additive correction constant: unit of VIF * 10^0)
    (ebzwmbe) 37: 01 vife (?)
    (ebzwmbe) 38: * FF240000 current power phase 1 (0.094710 kwh)
    (ebzwmbe) 3c: 04 dif (32 Bit Integer/Binary Instantaneous value)
    (ebzwmbe) 3d: A9 vif (Power 10⁻² W)
    (ebzwmbe) 3e: FF vife (additive correction constant: unit of VIF * 10^0)
    (ebzwmbe) 3f: 02 vife (?)
    (ebzwmbe) 40: * 6A290000 current power phase 2 (0.000000 kwh)
    (ebzwmbe) 44: 04 dif (32 Bit Integer/Binary Instantaneous value)
    (ebzwmbe) 45: A9 vif (Power 10⁻² W)
    (ebzwmbe) 46: FF vife (additive correction constant: unit of VIF * 10^0)
    (ebzwmbe) 47: 03 vife (?)
    (ebzwmbe) 48: * 46060000 current power phase 3 (0.000000 kwh)
    (ebzwmbe) 4c: 0D dif (variable length Instantaneous value)
    (ebzwmbe) 4d: FD vif (Second extension of VIF-codes)
    (ebzwmbe) 4e: 11 vife (Customer)
    (ebzwmbe) 4f: 06 varlen=6
    (ebzwmbe) 50: * 313233343536 customer (123456)
    */
    int offset;
    string key;

    if (findKey(MeasurementType::Unknown, ValueInformation::EnergyWh, 0, 0, &key, &t->values)) {
        extractDVdouble(&t->values, key, &offset, &total_energy_kwh_);
        t->addMoreExplanation(offset, " total energy (%f kwh)", total_energy_kwh_);
    }

    extractDVdouble(&t->values, "04A9FF01", &offset, &current_power_phase1_kw_);
    t->addMoreExplanation(offset, " current power phase 1 (%f kwh)", current_power_phase1_kw_);

    extractDVdouble(&t->values, "04A9FF02", &offset, &current_power_phase2_kw_);
    t->addMoreExplanation(offset, " current power phase 2 (%f kwh)", current_power_phase2_kw_);

    extractDVdouble(&t->values, "04A9FF03", &offset, &current_power_phase3_kw_);
    t->addMoreExplanation(offset, " current power phase 3 (%f kwh)", current_power_phase3_kw_);

    current_power_kw_ = current_power_phase1_kw_ + current_power_phase2_kw_ + current_power_phase3_kw_;
    t->addMoreExplanation(offset, " current power (%f kw)", current_power_kw_);

    string tmp;
    extractDVHexString(&t->values, "0DFD11", &offset, &tmp);
    if (tmp.length() > 0) {
        vector<uchar> bin;
        hex2bin(tmp, &bin);
        customer_ = safeString(bin);
    }
    t->addMoreExplanation(offset, " customer (%s)", customer_.c_str());
}
