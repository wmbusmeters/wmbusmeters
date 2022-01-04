/*
 Copyright (C) 2021 Vincent Privat

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

#include"meters.h"
#include"meters_common_implementation.h"
#include"dvparser.h"
#include"wmbus.h"
#include"wmbus_utils.h"

struct MeterSharky774 : public virtual MeterCommonImplementation {
    MeterSharky774(MeterInfo &mi);

    double totalEnergyConsumption(Unit u);
    double totalVolume(Unit u);
    double volumeFlow(Unit u);
    double power(Unit u);
    double flowTemperature(Unit u);
    double returnTemperature(Unit u);
    double temperatureDifference(Unit u);

private:
    void processContent(Telegram *t);

    double total_energy_mj_ {};
    double total_volume_m3_ {};
    double volume_flow_m3h_ {};
    double power_w_ {};
    double flow_temperature_c_ {};
    double return_temperature_c_ {};
};

MeterSharky774::MeterSharky774(MeterInfo &mi) :
    MeterCommonImplementation(mi, "sharky774")
{
    setMeterType(MeterType::HeatMeter);

    addLinkMode(LinkMode::T1);

    addPrint("total_energy_consumption", Quantity::Energy,
             [&](Unit u){ return totalEnergyConsumption(u); },
             "The total energy consumption recorded by this meter.",
             true, true);

    addPrint("total_volume", Quantity::Volume,
             [&](Unit u){ return totalVolume(u); },
             "The total volume recorded by this meter.",
             true, true);

    addPrint("volume_flow", Quantity::Flow,
             [&](Unit u){ return volumeFlow(u); },
             "The current flow.",
             true, true);

    addPrint("power", Quantity::Power,
             [&](Unit u){ return power(u); },
             "The power.",
             true, true);

    addPrint("flow_temperature", Quantity::Temperature,
             [&](Unit u){ return flowTemperature(u); },
             "The flow temperature.",
             true, true);

    addPrint("return_temperature", Quantity::Temperature,
             [&](Unit u){ return returnTemperature(u); },
             "The return temperature.",
             true, true);

    addPrint("temperature_difference", Quantity::Temperature,
             [&](Unit u){ return temperatureDifference(u); },
             "The temperature difference.",
             true, true);
}

shared_ptr<Meter> createSharky774(MeterInfo &mi) {
    return shared_ptr<Meter>(new MeterSharky774(mi));
}

double MeterSharky774::totalEnergyConsumption(Unit u)
{
    assertQuantity(u, Quantity::Energy);
    return convert(total_energy_mj_, Unit::MJ, u);
}

double MeterSharky774::totalVolume(Unit u)
{
    assertQuantity(u, Quantity::Volume);
    return convert(total_volume_m3_, Unit::M3, u);
}

double MeterSharky774::volumeFlow(Unit u)
{
    assertQuantity(u, Quantity::Flow);
    return convert(volume_flow_m3h_, Unit::M3H, u);
}

double MeterSharky774::power(Unit u)
{
    assertQuantity(u, Quantity::Power);
    return convert(power_w_, Unit::KW, u);
}

double MeterSharky774::flowTemperature(Unit u)
{
    assertQuantity(u, Quantity::Temperature);
    return convert(flow_temperature_c_, Unit::C, u);
}

double MeterSharky774::returnTemperature(Unit u)
{
    assertQuantity(u, Quantity::Temperature);
    return convert(return_temperature_c_, Unit::C, u);
}

double MeterSharky774::temperatureDifference(Unit u)
{
    assertQuantity(u, Quantity::Temperature);
    return convert(flow_temperature_c_ - return_temperature_c_, Unit::C, u);
}

void MeterSharky774::processContent(Telegram *t)
{
    /*
	(sharky) 017   : 0C dif (8 digit BCD Instantaneous value)
	(sharky) 018   : 0E vif (Energy MJ)
	(sharky) 019 C?: 00000000
	(sharky) 023   : 0C dif (8 digit BCD Instantaneous value)
	(sharky) 024   : 13 vif (Volume l)
	(sharky) 025 C!: 00000000 total volume (0.000000 ㎥)
	(sharky) 029   : 0B dif (6 digit BCD Instantaneous value)
	(sharky) 030   : 3B vif (Volume flow l/h)
	(sharky) 031 C!: 000000 volume flow (0.000000 ㎥/h)
	(sharky) 034   : 0C dif (8 digit BCD Instantaneous value)
	(sharky) 035   : 2B vif (Power W)
	(sharky) 036 C!: 00000000 power (0.000000 W)
	(sharky) 040   : 0A dif (4 digit BCD Instantaneous value)
	(sharky) 041   : 5A vif (Flow temperature 10⁻¹ °C)
	(sharky) 042 C!: 8504 flow temperature (48.500000 °C)
	(sharky) 044   : 0A dif (4 digit BCD Instantaneous value)
	(sharky) 045   : 5E vif (Return temperature 10⁻¹ °C)
	(sharky) 046 C!: 6604 return temperature (46.600000 °C)
	(sharky) 048   : 0B dif (6 digit BCD Instantaneous value)
	(sharky) 049   : 26 vif (Operating time hours)
	(sharky) 050 C?: 631800
	(sharky) 053   : 0A dif (4 digit BCD Instantaneous value)
	(sharky) 054   : A6 vif (Operating time hours)
	(sharky) 055   : 18 vife (?)
	(sharky) 056 C?: 0000
	(sharky) 058   : C2 dif (16 Bit Integer/Binary Instantaneous value storagenr=1)
	(sharky) 059   : 02 dife (subunit=0 tariff=0 storagenr=5)
	(sharky) 060   : 6C vif (Date type G)
	(sharky) 061 C?: BE2B
	(sharky) 063   : CC dif (8 digit BCD Instantaneous value storagenr=1)
	(sharky) 064   : 02 dife (subunit=0 tariff=0 storagenr=5)
	(sharky) 065   : 0E vif (Energy MJ)
	(sharky) 066 C?: 00000000
	(sharky) 070   : CC dif (8 digit BCD Instantaneous value storagenr=1)
	(sharky) 071   : 02 dife (subunit=0 tariff=0 storagenr=5)
	(sharky) 072   : 13 vif (Volume l)
	(sharky) 073 C?: 00000000
	(sharky) 077   : DB dif (6 digit BCD Maximum value storagenr=1)
	(sharky) 078   : 02 dife (subunit=0 tariff=0 storagenr=5)
	(sharky) 079   : 3B vif (Volume flow l/h)
	(sharky) 080 C?: 000000
	(sharky) 083   : DC dif (8 digit BCD Maximum value storagenr=1)
	(sharky) 084   : 02 dife (subunit=0 tariff=0 storagenr=5)
	(sharky) 085   : 2B vif (Power W)
	(sharky) 086 C?: 00000000
	(sharky) 090   : 2F skip
	(sharky) 091   : 2F skip
	(sharky) 092   : 2F skip
	(sharky) 093   : 2F skip
	(sharky) 094   : 2F skip
    */

    int offset;
    string key;
    
    if (findKey(MeasurementType::Instantaneous, ValueInformation::EnergyMJ, 0, 0, &key, &t->values)) {
        extractDVdouble(&t->values, key, &offset, &total_energy_mj_);
        t->addMoreExplanation(offset, " total energy consumption (%f MJ)", total_energy_mj_);
    }

    if (findKey(MeasurementType::Instantaneous, ValueInformation::Volume, 0, 0, &key, &t->values)) {
        extractDVdouble(&t->values, key, &offset, &total_volume_m3_);
        t->addMoreExplanation(offset, " total volume (%f ㎥)", total_volume_m3_);
    }

    if (findKey(MeasurementType::Instantaneous, ValueInformation::VolumeFlow, 0, 0, &key, &t->values)) {
        extractDVdouble(&t->values, key, &offset, &volume_flow_m3h_);
        t->addMoreExplanation(offset, " volume flow (%f ㎥/h)", volume_flow_m3h_);
    }

    if (findKey(MeasurementType::Instantaneous, ValueInformation::PowerW, 0, 0, &key, &t->values)) {
        extractDVdouble(&t->values, key, &offset, &power_w_);
        t->addMoreExplanation(offset, " power (%f W)", power_w_);
    }

    if (findKey(MeasurementType::Instantaneous, ValueInformation::FlowTemperature, 0, 0, &key, &t->values)) {
        extractDVdouble(&t->values, key, &offset, &flow_temperature_c_);
        t->addMoreExplanation(offset, " flow temperature (%f °C)", flow_temperature_c_);
    }

    if (findKey(MeasurementType::Instantaneous, ValueInformation::ReturnTemperature, 0, 0, &key, &t->values)) {
        extractDVdouble(&t->values, key, &offset, &return_temperature_c_);
        t->addMoreExplanation(offset, " return temperature (%f °C)", return_temperature_c_);
    }
}
