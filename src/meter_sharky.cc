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

struct MeterSharky : public virtual MeterCommonImplementation {
    MeterSharky(MeterInfo &mi);

    double totalEnergyConsumption(Unit u);
    double totalEnergyConsumptionTariff1(Unit u);
    double totalVolume(Unit u);
    double totalVolumeTariff2(Unit u);
    double volumeFlow(Unit u);
    double power(Unit u);
    double flowTemperature(Unit u);
    double returnTemperature(Unit u);
    double temperatureDifference(Unit u);

private:
    void processContent(Telegram *t);

    double total_energy_kwh_ {};
    double total_energy_tariff1_kwh_ {};
    double total_volume_m3_ {};
    double total_volume_tariff2_m3_ {};
    double volume_flow_m3h_ {};
    double power_w_ {};
    double flow_temperature_c_ {};
    double return_temperature_c_ {};
    double temperature_difference_c_ {};
};

MeterSharky::MeterSharky(MeterInfo &mi) :
    MeterCommonImplementation(mi, "sharky")
{
    setMeterType(MeterType::HeatMeter);

    addLinkMode(LinkMode::T1);

    addPrint("total_energy_consumption", Quantity::Energy,
             [&](Unit u){ return totalEnergyConsumption(u); },
             "The total energy consumption recorded by this meter.",
             true, true);

    addPrint("total_energy_consumption_tariff1", Quantity::Energy,
             [&](Unit u){ return totalEnergyConsumptionTariff1(u); },
             "The total energy consumption recorded by this meter on tariff 1.",
             true, true);

    addPrint("total_volume", Quantity::Volume,
             [&](Unit u){ return totalVolume(u); },
             "The total volume recorded by this meter.",
             true, true);

    addPrint("total_volume", Quantity::Volume,
             [&](Unit u){ return totalVolumeTariff2(u); },
             "The total volume recorded by this meter on tariff 2.",
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

shared_ptr<Meter> createSharky(MeterInfo &mi) {
    return shared_ptr<Meter>(new MeterSharky(mi));
}

double MeterSharky::totalEnergyConsumption(Unit u)
{
    assertQuantity(u, Quantity::Energy);
    return convert(total_energy_kwh_, Unit::KWH, u);
}

double MeterSharky::totalEnergyConsumptionTariff1(Unit u)
{
    assertQuantity(u, Quantity::Energy);
    return convert(total_energy_tariff1_kwh_, Unit::KWH, u);
}

double MeterSharky::totalVolume(Unit u)
{
    assertQuantity(u, Quantity::Volume);
    return convert(total_volume_m3_, Unit::M3, u);
}

double MeterSharky::totalVolumeTariff2(Unit u)
{
    assertQuantity(u, Quantity::Volume);
    return convert(total_volume_tariff2_m3_, Unit::M3, u);
}

double MeterSharky::volumeFlow(Unit u)
{
    assertQuantity(u, Quantity::Flow);
    return convert(volume_flow_m3h_, Unit::M3H, u);
}

double MeterSharky::power(Unit u)
{
    assertQuantity(u, Quantity::Power);
    return convert(power_w_, Unit::KW, u);
}

double MeterSharky::flowTemperature(Unit u)
{
    assertQuantity(u, Quantity::Temperature);
    return convert(flow_temperature_c_, Unit::C, u);
}

double MeterSharky::returnTemperature(Unit u)
{
    assertQuantity(u, Quantity::Temperature);
    return convert(return_temperature_c_, Unit::C, u);
}

double MeterSharky::temperatureDifference(Unit u)
{
    assertQuantity(u, Quantity::Temperature);
    return convert(temperature_difference_c_, Unit::C, u);
}

void MeterSharky::processContent(Telegram *t)
{
    /*
      (wmbus) 0f: 0C dif (8 digit BCD Instantaneous value)
      (wmbus) 10: 06 vif (Energy kWh)
      (wmbus) 11: 51260000
      (wmbus) 15: 8C dif (8 digit BCD Instantaneous value)
      (wmbus) 16: 10 dife (subunit=0 tariff=1 storagenr=0)
      (wmbus) 17: 06 vif (Energy kWh)
      (wmbus) 18: 00000000
      (wmbus) 1c: 0C dif (8 digit BCD Instantaneous value)
      (wmbus) 1d: 13 vif (Volume l)
      (wmbus) 1e: 47031500
      (wmbus) 22: 8C dif (8 digit BCD Instantaneous value)
      (wmbus) 23: 20 dife (subunit=0 tariff=2 storagenr=0)
      (wmbus) 24: 13 vif (Volume l)
      (wmbus) 25: 18000000
      (wmbus) 29: 8C dif (8 digit BCD Instantaneous value)
      (wmbus) 2a: 40 dife (subunit=1 tariff=0 storagenr=0)
      (wmbus) 2b: 13 vif (Volume l)
      (wmbus) 2c: 00000000
      (wmbus) 30: 8C dif (8 digit BCD Instantaneous value)
      (wmbus) 31: 80 dife (subunit=0 tariff=0 storagenr=0)
      (wmbus) 32: 40 dife (subunit=2 tariff=0 storagenr=0)
      (wmbus) 33: 13 vif (Volume l)
      (wmbus) 34: 00000000
      (wmbus) 38: 02 dif (16 Bit Integer/Binary Instantaneous value)
      (wmbus) 39: FD vif (Second extension FD of VIF-codes)
      (wmbus) 3a: 17 vife (Error flags (binary))
      (wmbus) 3b: 0000
      (wmbus) 3d: 0B dif (6 digit BCD Instantaneous value)
      (wmbus) 3e: 3B vif (Volume flow l/h)
      (wmbus) 3f: 000000
      (wmbus) 42: 0C dif (8 digit BCD Instantaneous value)
      (wmbus) 43: 2B vif (Power W)
      (wmbus) 44: 00000000
      (wmbus) 48: 0A dif (4 digit BCD Instantaneous value)
      (wmbus) 49: 5A vif (Flow temperature 10⁻¹ °C)
      (wmbus) 4a: 2304
      (wmbus) 4c: 0A dif (4 digit BCD Instantaneous value)
      (wmbus) 4d: 5E vif (Return temperature 10⁻¹ °C)
      (wmbus) 4e: 8102
      (wmbus) 50: 0A dif (4 digit BCD Instantaneous value)
      (wmbus) 51: 62 vif (Temperature difference 10⁻¹ K)
      (wmbus) 52: 4101
    */

    int offset;
    string key;

    if (findKey(MeasurementType::Instantaneous, ValueInformation::EnergyWh, 0, 0, &key, &t->values)) {
        extractDVdouble(&t->values, key, &offset, &total_energy_kwh_);
        t->addMoreExplanation(offset, " total energy consumption (%f kWh)", total_energy_kwh_);
    }

    if (findKey(MeasurementType::Instantaneous, ValueInformation::EnergyWh, 0, 1, &key, &t->values)) {
        extractDVdouble(&t->values, key, &offset, &total_energy_tariff1_kwh_);
        t->addMoreExplanation(offset, " total energy tariff 1 (%f kwh)", total_energy_tariff1_kwh_);
    }

    if (findKey(MeasurementType::Instantaneous, ValueInformation::Volume, 0, 0, &key, &t->values)) {
        extractDVdouble(&t->values, key, &offset, &total_volume_m3_);
        t->addMoreExplanation(offset, " total volume (%f ㎥)", total_volume_m3_);
    }

    if (findKey(MeasurementType::Instantaneous, ValueInformation::Volume, 0, 2, &key, &t->values)) {
        extractDVdouble(&t->values, key, &offset, &total_volume_tariff2_m3_);
        t->addMoreExplanation(offset, " total volume tariff 2 (%f ㎥)", total_volume_tariff2_m3_);
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

    if (findKey(MeasurementType::Instantaneous, ValueInformation::TemperatureDifference, 0, 0, &key, &t->values)) {
        extractDVdouble(&t->values, key, &offset, &temperature_difference_c_);
        t->addMoreExplanation(offset, " temperature difference (%f °C)", temperature_difference_c_);
    }
}
