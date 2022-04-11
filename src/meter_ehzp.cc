/*
 Copyright (C) 2020 Fredrik Öhrström (gpl-3.0-or-later)

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

struct MeterEHZP : public virtual MeterCommonImplementation
{
    MeterEHZP(MeterInfo &mi);

    double totalEnergyConsumption(Unit u);
    double currentPowerConsumption(Unit u);
    double totalEnergyProduction(Unit u);
    double currentPowerProduction(Unit u);

private:

    void processContent(Telegram *t);

    double total_energy_kwh_ {};
    double current_power_kw_ {};
    double total_energy_returned_kwh_ {};
    double current_power_returned_kw_ {};
    double on_time_h_ {};
};

MeterEHZP::MeterEHZP(MeterInfo &mi) :
    MeterCommonImplementation(mi, "ehzp")
{
    setMeterType(MeterType::ElectricityMeter);

    setExpectedTPLSecurityMode(TPLSecurityMode::AES_CBC_NO_IV);

    addLinkMode(LinkMode::T1);

    addPrint("total_energy_consumption", Quantity::Energy,
             [&](Unit u){ return totalEnergyConsumption(u); },
             "The total energy consumption recorded by this meter.",
             PrintProperty::FIELD | PrintProperty::JSON);

    addPrint("current_power_consumption", Quantity::Power,
             [&](Unit u){ return currentPowerConsumption(u); },
             "Current power consumption.",
             PrintProperty::FIELD | PrintProperty::JSON);

    addPrint("total_energy_production", Quantity::Energy,
             [&](Unit u){ return totalEnergyProduction(u); },
             "The total energy production recorded by this meter.",
             PrintProperty::FIELD | PrintProperty::JSON);

    addPrint("on_time", Quantity::Time,
             [&](Unit u){ assertQuantity(u, Quantity::Time);
                 return convert(on_time_h_, Unit::Hour, u); },
             "Device on time.",
             PrintProperty::JSON);
}

shared_ptr<Meter> createEHZP(MeterInfo &mi)
{
    return shared_ptr<Meter>(new MeterEHZP(mi));
}

double MeterEHZP::totalEnergyConsumption(Unit u)
{
    assertQuantity(u, Quantity::Energy);
    return convert(total_energy_kwh_, Unit::KWH, u);
}

double MeterEHZP::currentPowerConsumption(Unit u)
{
    assertQuantity(u, Quantity::Power);
    return convert(current_power_kw_, Unit::KW, u);
}

double MeterEHZP::totalEnergyProduction(Unit u)
{
    assertQuantity(u, Quantity::Energy);
    return convert(total_energy_returned_kwh_, Unit::KWH, u);
}

double MeterEHZP::currentPowerProduction(Unit u)
{
    assertQuantity(u, Quantity::Power);
    return convert(current_power_returned_kw_, Unit::KW, u);
}

void MeterEHZP::processContent(Telegram *t)
{
    /*
    (ehzp) 26: 07 dif (64 Bit Integer/Binary Instantaneous value)
    (ehzp) 27: 00 vif (Energy mWh)
    (ehzp) 28: * 583B740200000000 total energy (41.171800 kwh)
    (ehzp) 30: 07 dif (64 Bit Integer/Binary Instantaneous value)
    (ehzp) 31: 80 vif (Energy mWh)
    (ehzp) 32: 3C vife (backward flow)
    (ehzp) 33: * BCD7020000000000 total energy returned (0.186300 kwh)
    (ehzp) 3b: 07 dif (64 Bit Integer/Binary Instantaneous value)
    (ehzp) 3c: 28 vif (Power mW)
    (ehzp) 3d: * B070200000000000 current power (2.126000 kw)
    (ehzp) 45: 04 dif (32 Bit Integer/Binary Instantaneous value)
    (ehzp) 46: 20 vif (On time seconds)
    (ehzp) 47: * 92A40600 on time (120.929444 h)
    */
    int offset;
    string key;

    if (findKey(MeasurementType::Unknown, ValueInformation::EnergyWh, 0, 0, &key, &t->values))
    {
        extractDVdouble(&t->values, key, &offset, &total_energy_kwh_);
        t->addMoreExplanation(offset, " total energy (%f kwh)", total_energy_kwh_);
    }

    if (findKey(MeasurementType::Unknown, ValueInformation::PowerW, 0, 0, &key, &t->values))
    {
        extractDVdouble(&t->values, key, &offset, &current_power_kw_);
        t->addMoreExplanation(offset, " current power (%f kw)", current_power_kw_);
    }

    extractDVdouble(&t->values, "07803C", &offset, &total_energy_returned_kwh_);
    t->addMoreExplanation(offset, " total energy returned (%f kwh)", total_energy_returned_kwh_);

    extractDVdouble(&t->values, "0420", &offset, &on_time_h_);
    t->addMoreExplanation(offset, " on time (%f h)", on_time_h_);

}
