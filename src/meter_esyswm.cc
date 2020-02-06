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

struct MeterESYSWM : public virtual ElectricityMeter, public virtual MeterCommonImplementation
{
    MeterESYSWM(WMBus *bus, MeterInfo &mi);

    double totalEnergyConsumption(Unit u);
    double totalEnergyConsumptionTariff1(Unit u);
    double totalEnergyConsumptionTariff2(Unit u);
    double currentPowerConsumption(Unit u);
    double currentPowerConsumptionPhase1(Unit u);
    double currentPowerConsumptionPhase2(Unit u);
    double currentPowerConsumptionPhase3(Unit u);
    double totalEnergyProduction(Unit u);
    double currentPowerProduction(Unit u);

private:

    void processContent(Telegram *t);

    double total_energy_kwh_ {};
    double total_energy_tariff1_kwh_ {};
    double total_energy_tariff2_kwh_ {};
    double current_power_kw_ {};
    double current_power_phase1_kw_ {};
    double current_power_phase2_kw_ {};
    double current_power_phase3_kw_ {};
    double total_energy_returned_kwh_ {};
    double current_power_returned_kw_ {};
    string device_date_time_;

    // Information sent more rarely and is static.

    string version_;
    string enhanced_id_;
    string location_hex_;
    string fabrication_no_;
};

MeterESYSWM::MeterESYSWM(WMBus *bus, MeterInfo &mi) :
    MeterCommonImplementation(bus, mi, MeterType::ESYSWM, MANUFACTURER_ESY)
{
    setExpectedTPLSecurityMode(TPLSecurityMode::AES_CBC_NO_IV);

    // The ESYS-WM-20 and ESYS-WM15 are addons to the electricity meters.
    addMedia(0x37); // Radio converter (meter side)

    addLinkMode(LinkMode::T1);

    setExpectedVersion(0x30);

    addPrint("total_energy_consumption", Quantity::Energy,
             [&](Unit u){ return totalEnergyConsumption(u); },
             "The total energy consumption recorded by this meter.",
             true, true);

    addPrint("current_power_consumption", Quantity::Power,
             [&](Unit u){ return currentPowerConsumption(u); },
             "Current power consumption.",
             true, true);

    addPrint("total_energy_production", Quantity::Energy,
             [&](Unit u){ return totalEnergyProduction(u); },
             "The total energy production recorded by this meter.",
             true, true);

    addPrint("total_energy_consumption_tariff1", Quantity::Energy,
             [&](Unit u){ return totalEnergyConsumptionTariff1(u); },
             "The total energy consumption recorded by this meter on tariff 1.",
             true, true);

    addPrint("total_energy_consumption_tariff2", Quantity::Energy,
             [&](Unit u){ return totalEnergyConsumptionTariff2(u); },
             "The total energy consumption recorded by this meter on tariff 2.",
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

    addPrint("enhanced_id", Quantity::Text,
             [&](){ return enhanced_id_; },
             "Static enhanced id information.",
             true, true);

    addPrint("version", Quantity::Text,
             [&](){ return version_; },
             "Static version information.",
             false, true);

    addPrint("location_hex", Quantity::Text,
             [&](){ return location_hex_; },
             "Static location information.",
             false, true);

    addPrint("fabrication_no", Quantity::Text,
             [&](){ return fabrication_no_; },
             "Static fabrication no information.",
             false, true);
}

unique_ptr<ElectricityMeter> createESYSWM(WMBus *bus, MeterInfo &mi)
{
    return unique_ptr<ElectricityMeter>(new MeterESYSWM(bus, mi));
}

double MeterESYSWM::totalEnergyConsumption(Unit u)
{
    assertQuantity(u, Quantity::Energy);
    return convert(total_energy_kwh_, Unit::KWH, u);
}

double MeterESYSWM::totalEnergyConsumptionTariff1(Unit u)
{
    assertQuantity(u, Quantity::Energy);
    return convert(total_energy_tariff1_kwh_, Unit::KWH, u);
}

double MeterESYSWM::totalEnergyConsumptionTariff2(Unit u)
{
    assertQuantity(u, Quantity::Energy);
    return convert(total_energy_tariff2_kwh_, Unit::KWH, u);
}

double MeterESYSWM::currentPowerConsumption(Unit u)
{
    assertQuantity(u, Quantity::Power);
    return convert(current_power_kw_, Unit::KW, u);
}

double MeterESYSWM::currentPowerConsumptionPhase1(Unit u)
{
    assertQuantity(u, Quantity::Power);
    return convert(current_power_phase1_kw_, Unit::KW, u);
}

double MeterESYSWM::currentPowerConsumptionPhase2(Unit u)
{
    assertQuantity(u, Quantity::Power);
    return convert(current_power_phase2_kw_, Unit::KW, u);
}

double MeterESYSWM::currentPowerConsumptionPhase3(Unit u)
{
    assertQuantity(u, Quantity::Power);
    return convert(current_power_phase3_kw_, Unit::KW, u);
}

double MeterESYSWM::totalEnergyProduction(Unit u)
{
    assertQuantity(u, Quantity::Energy);
    return convert(total_energy_returned_kwh_, Unit::KWH, u);
}

double MeterESYSWM::currentPowerProduction(Unit u)
{
    assertQuantity(u, Quantity::Power);
    return convert(current_power_returned_kw_, Unit::KW, u);
}

void MeterESYSWM::processContent(Telegram *t)
{
    /*
      (esyswm) 2e: 07 dif (64 Bit Integer/Binary Instantaneous value)
      (esyswm) 2f: 02 vif (Energy 10⁻¹ Wh)
      (esyswm) 30: * F5C3FA0000000000 total energy (1643.416500 kwh)
      (esyswm) 38: 07 dif (64 Bit Integer/Binary Instantaneous value)
      (esyswm) 39: 82 vif (Energy 10⁻¹ Wh)
      (esyswm) 3a: 3C vife (backward flow)
      (esyswm) 3b: * 5407000000000000 total energy returned (0.187600 kwh)
      (esyswm) 43: 84 dif (32 Bit Integer/Binary Instantaneous value)
      (esyswm) 44: 10 dife (subunit=0 tariff=1 storagenr=0)
      (esyswm) 45: 04 vif (Energy 10¹ Wh)
      (esyswm) 46: * E0810200 total energy tariff 1 (1643.200000 kwh)
      (esyswm) 4a: 84 dif (32 Bit Integer/Binary Instantaneous value)
      (esyswm) 4b: 20 dife (subunit=0 tariff=2 storagenr=0)
      (esyswm) 4c: 04 vif (Energy 10¹ Wh)
      (esyswm) 4d: * 15000000 total energy tariff 2 (0.210000 kwh)
      (esyswm) 51: 04 dif (32 Bit Integer/Binary Instantaneous value)
      (esyswm) 52: 29 vif (Power 10⁻² W)
      (esyswm) 53: * 38AB0000 current power (0.438320 kw)
      (esyswm) 57: 04 dif (32 Bit Integer/Binary Instantaneous value)
      (esyswm) 58: A9 vif (Power 10⁻² W)
      (esyswm) 59: FF vife (additive correction constant: unit of VIF * 10^0)
      (esyswm) 5a: 01 vife (?)
      (esyswm) 5b: * FA0A0000 current power phase 1 (0.028100 kwh)
      (esyswm) 5f: 04 dif (32 Bit Integer/Binary Instantaneous value)
      (esyswm) 60: A9 vif (Power 10⁻² W)
      (esyswm) 61: FF vife (additive correction constant: unit of VIF * 10^0)
      (esyswm) 62: 02 vife (?)
      (esyswm) 63: * 050A0000 current power phase 2 (0.000000 kwh)
      (esyswm) 67: 04 dif (32 Bit Integer/Binary Instantaneous value)
      (esyswm) 68: A9 vif (Power 10⁻² W)
      (esyswm) 69: FF vife (additive correction constant: unit of VIF * 10^0)
      (esyswm) 6a: 03 vife (?)
      (esyswm) 6b: * 38960000 current power phase 3 (0.000000 kwh)
    */

    /*
      (esyswm) 2e: 0D dif (variable length Instantaneous value)
      (esyswm) 2f: FD vif (Second extension of VIF-codes)
      (esyswm) 30: 09 vife (Medium (as in fixed header))
      (esyswm) 31: 0F varlen=15
      (esyswm) 32:
      (esyswm) 41: 0D dif (variable length Instantaneous value)
      (esyswm) 42: 79 vif (Enhanced identification)
      (esyswm) 43: 0E varlen=14
      (esyswm) 44:
      (esyswm) 52: 0D dif (variable length Instantaneous value)
      (esyswm) 53: FD vif (Second extension of VIF-codes)
      (esyswm) 54: 10 vife (Customer location)
      (esyswm) 55: 0A varlen=10
      (esyswm) 56:
      (esyswm) 60: 0D dif (variable length Instantaneous value)
      (esyswm) 61: 78 vif (Fabrication no)
      (esyswm) 62: 0E varlen=14
      (esyswm) 63:
    */

    int offset;
    string key;

    if (findKey(MeasurementType::Unknown, ValueInformation::EnergyWh, 0, 0, &key, &t->values)) {
        extractDVdouble(&t->values, key, &offset, &total_energy_kwh_);
        t->addMoreExplanation(offset, " total energy (%f kwh)", total_energy_kwh_);
    }

    if (findKey(MeasurementType::Unknown, ValueInformation::EnergyWh, 0, 1, &key, &t->values)) {
        extractDVdouble(&t->values, key, &offset, &total_energy_tariff1_kwh_);
        t->addMoreExplanation(offset, " total energy tariff 1 (%f kwh)", total_energy_tariff1_kwh_);
    }

    if (findKey(MeasurementType::Unknown, ValueInformation::EnergyWh, 0, 2, &key, &t->values)) {
        extractDVdouble(&t->values, key, &offset, &total_energy_tariff2_kwh_);
        t->addMoreExplanation(offset, " total energy tariff 2 (%f kwh)", total_energy_tariff2_kwh_);
    }

    if (findKey(MeasurementType::Unknown, ValueInformation::PowerW, 0, 0, &key, &t->values)) {
        extractDVdouble(&t->values, key, &offset, &current_power_kw_);
        t->addMoreExplanation(offset, " current power (%f kw)", current_power_kw_);
    }

    extractDVdouble(&t->values, "07823C", &offset, &total_energy_returned_kwh_);
    t->addMoreExplanation(offset, " total energy returned (%f kwh)", total_energy_returned_kwh_);

    extractDVdouble(&t->values, "04A9FF01", &offset, &current_power_phase1_kw_);
    t->addMoreExplanation(offset, " current power phase 1 (%f kwh)", current_power_phase1_kw_);

    extractDVdouble(&t->values, "04A9FF02", &offset, &current_power_phase1_kw_);
    t->addMoreExplanation(offset, " current power phase 2 (%f kwh)", current_power_phase2_kw_);

    extractDVdouble(&t->values, "04A9FF03", &offset, &current_power_phase1_kw_);
    t->addMoreExplanation(offset, " current power phase 3 (%f kwh)", current_power_phase3_kw_);

    string tmp;
    extractDVstring(&t->values, "0DFD09", &offset, &tmp);
    if (tmp.length() > 0) {
        vector<uchar> bin;
        hex2bin(tmp, &bin);
        version_ = safeString(bin);
    }
    t->addMoreExplanation(offset, " version (%s)", version_.c_str());

    extractDVstring(&t->values, "0D79", &offset, &tmp);
    if (tmp.length() > 0) {
        vector<uchar> bin;
        hex2bin(tmp, &bin);
        enhanced_id_ = safeString(bin);
    }
    t->addMoreExplanation(offset, " enhanced id (%s)", enhanced_id_.c_str());

    extractDVstring(&t->values, "0DFD10", &offset, &tmp);
    if (tmp.length() > 0) {
        location_hex_ = tmp;
    }
    t->addMoreExplanation(offset, " location (%s)", location_hex_.c_str());

    extractDVstring(&t->values, "0D78", &offset, &tmp);
    if (tmp.length() > 0) {
        vector<uchar> bin;
        hex2bin(tmp, &bin);
        fabrication_no_ = safeString(bin);
    }
    t->addMoreExplanation(offset, " fabrication no (%s)", fabrication_no_.c_str());

}
