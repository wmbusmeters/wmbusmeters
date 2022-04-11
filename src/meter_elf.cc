/*
 Copyright (C) 2021 Fredrik Öhrström (gpl-3.0-or-later)

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
#include"util.h"

struct MeterElf : public virtual MeterCommonImplementation {
    MeterElf(MeterInfo &mi);

    double totalEnergyConsumption(Unit u);
    double targetEnergyConsumption(Unit u);
    double currentPowerConsumption(Unit u);
    string status();
    double totalVolume(Unit u);
    double targetVolume(Unit u);

private:
    void processContent(Telegram *t);

    string meter_date_ {};
    uint32_t info_codes_ {};
    double total_energy_kwh_ {};
    double target_energy_kwh_ {};
    double current_power_kw_ {};
    double total_volume_m3_ {};
    double flow_temperature_c_ { 127 };
    double return_temperature_c_ { 127 };
    double external_temperature_c_ { 127 };
    uint16_t operating_time_days_ {};
    string version_;
    double battery_v_ {};
};

MeterElf::MeterElf(MeterInfo &mi) :
    MeterCommonImplementation(mi, "elf")
{
    setMeterType(MeterType::HeatMeter);

    setExpectedELLSecurityMode(ELLSecurityMode::AES_CTR);

    addLinkMode(LinkMode::C1);

    addPrint("meter_date", Quantity::Text,
             [&](){ return meter_date_; },
             "Date when measurement was recorded.",
             PrintProperty::JSON);

    addPrint("total_energy_consumption", Quantity::Energy,
             [&](Unit u){ return totalEnergyConsumption(u); },
             "The total energy consumption recorded by this meter.",
             PrintProperty::FIELD | PrintProperty::JSON);

    addPrint("current_power_consumption", Quantity::Power,
             [&](Unit u){ return currentPowerConsumption(u); },
             "Current power consumption.",
             PrintProperty::FIELD | PrintProperty::JSON);

    addPrint("total_volume", Quantity::Volume,
             [&](Unit u){ return totalVolume(u); },
             "Total volume of heat media.",
             PrintProperty::FIELD | PrintProperty::JSON);

    addPrint("total_energy_consumption_at_date", Quantity::Energy,
             [&](Unit u){ return targetEnergyConsumption(u); },
             "The total energy consumption recorded at the target date.",
             PrintProperty::JSON);

    addPrint("flow_temperature", Quantity::Temperature,
             [&](Unit u){ assertQuantity(u, Quantity::Temperature); return convert(flow_temperature_c_, Unit::C, u); },
             "The water temperature.",
             PrintProperty::FIELD | PrintProperty::JSON);

    addPrint("return_temperature", Quantity::Temperature,
             [&](Unit u){ assertQuantity(u, Quantity::Temperature); return convert(return_temperature_c_, Unit::C, u); },
             "The return temperature.",
             PrintProperty::FIELD | PrintProperty::JSON);

    addPrint("external_temperature", Quantity::Temperature,
             [&](Unit u){ assertQuantity(u, Quantity::Temperature); return convert(external_temperature_c_, Unit::C, u); },
             "The external temperature.",
             PrintProperty::FIELD | PrintProperty::JSON);

    addPrint("status", Quantity::Text,
             [&](){ return status(); },
             "Status of meter.",
             PrintProperty::FIELD | PrintProperty::JSON);

    addPrint("operating_time", Quantity::Time,
             [&](Unit u){ assertQuantity(u, Quantity::Time); return convert(operating_time_days_, Unit::Day, u); },
             "Operating time.",
             PrintProperty::JSON);

    addPrint("version", Quantity::Text,
             [&](){ return version_; },
             "Version number.",
             PrintProperty::JSON);

    addPrint("battery", Quantity::Voltage,
             [&](Unit u){ assertQuantity(u, Quantity::Voltage); return convert(battery_v_, Unit::Volt, u); },
             "Battery voltage. Not yet implemented.",
             PrintProperty::JSON);

}

shared_ptr<Meter> createElf(MeterInfo &mi) {
    return shared_ptr<Meter>(new MeterElf(mi));
}

double MeterElf::totalEnergyConsumption(Unit u)
{
    assertQuantity(u, Quantity::Energy);
    return convert(total_energy_kwh_, Unit::KWH, u);
}

double MeterElf::targetEnergyConsumption(Unit u)
{
    assertQuantity(u, Quantity::Energy);
    return convert(target_energy_kwh_, Unit::KWH, u);
}

double MeterElf::totalVolume(Unit u)
{
    assertQuantity(u, Quantity::Volume);
    return convert(total_volume_m3_, Unit::M3, u);
}

double MeterElf::currentPowerConsumption(Unit u)
{
    assertQuantity(u, Quantity::Power);
    return convert(current_power_kw_, Unit::KW, u);
}

void MeterElf::processContent(Telegram *t)
{
    /*
      (elf) 17: 02 dif (16 Bit Integer/Binary Instantaneous value)
      (elf) 18: 6C vif (Date type G)
      (elf) 19: * A922 meter date (2021-02-09)
      (elf) 1b: 0E dif (12 digit BCD Instantaneous value)
      (elf) 1c: 01 vif (Energy 10⁻² Wh)
      (elf) 1d: * 779924110300 total energy consumption (3112.499770 kWh)
      (elf) 23: 0C dif (8 digit BCD Instantaneous value)
      (elf) 24: 13 vif (Volume l)
      (elf) 25: * 64132000 total volume (201.364000 m3)
      (elf) 29: 0A dif (4 digit BCD Instantaneous value)
      (elf) 2a: 2D vif (Power 10² W)
      (elf) 2b: * 0000 current power consumption (0.000000 kW)
      (elf) 2d: 0A dif (4 digit BCD Instantaneous value)
      (elf) 2e: 5A vif (Flow temperature 10⁻¹ °C)
      (elf) 2f: * 9006 flow temperature (69.000000 °C)
      (elf) 31: 0A dif (4 digit BCD Instantaneous value)
      (elf) 32: 5E vif (Return temperature 10⁻¹ °C)
      (elf) 33: * 8005 return temperature (58.000000 °C)
      (elf) 35: 44 dif (32 Bit Integer/Binary Instantaneous value storagenr=1)
      (elf) 36: 05 vif (Energy 10² Wh)
      (elf) 37: * 0E770000 target energy consumption (3047.800000 kWh)
      (elf) 3b: 01 dif (8 Bit Integer/Binary Instantaneous value)
      (elf) 3c: FD vif (Second extension FD of VIF-codes)
      (elf) 3d: 0C vife (Model/Version)
      (elf) 3e: * 01 version (1)
      (elf) 3f: 0A dif (4 digit BCD Instantaneous value)
      (elf) 40: 65 vif (External temperature 10⁻² °C)
      (elf) 41: * 6437 external temperature (37.640000 °C)
      (elf) 43: 0A dif (4 digit BCD Instantaneous value)
      (elf) 44: FD vif (Second extension FD of VIF-codes)
      (elf) 45: 47 vife (10^-2 Volts)
      (elf) 46: 3103
      (elf) 48: 0A dif (4 digit BCD Instantaneous value)
      (elf) 49: 27 vif (Operating time days)
      (elf) 4a: * 4907 operating time days (1865)
      (elf) 4c: 04 dif (32 Bit Integer/Binary Instantaneous value)
      (elf) 4d: 7F vif (Manufacturer specific)
      (elf) 4e: * 00000002 info codes (200000)
        */
    int offset;
    string key;

    if (findKey(MeasurementType::Instantaneous, ValueInformation::Date, 0, 0, &key, &t->values)) {
        struct tm date;
        extractDVdate(&t->values, key, &offset, &date);
        meter_date_ = strdate(&date);
        t->addMoreExplanation(offset, " meter date (%s)", meter_date_.c_str());
    }

    if (extractDVuint32(&t->values, "047F", &offset, &info_codes_))
    {
        t->addMoreExplanation(offset, " info codes (%s)", status().c_str());
    }

    if (extractDVuint16(&t->values, "0A27", &offset, &operating_time_days_))
    {
        t->addMoreExplanation(offset, " operating time days (%d)", operating_time_days_);
    }

    uchar ver;
    if (extractDVuint8(&t->values, "01FD0C", &offset, &ver))
    {
        version_ = tostrprintf("%u", ver);
        t->addMoreExplanation(offset, " version (%s)", version_.c_str());
    }

    if(findKey(MeasurementType::Instantaneous, ValueInformation::EnergyWh, 0, 0, &key, &t->values)) {
        extractDVdouble(&t->values, key, &offset, &total_energy_kwh_);
        t->addMoreExplanation(offset, " total energy consumption (%f kWh)", total_energy_kwh_);
    }

    if(findKey(MeasurementType::Instantaneous, ValueInformation::Volume, 0, 0, &key, &t->values)) {
        extractDVdouble(&t->values, key, &offset, &total_volume_m3_);
        t->addMoreExplanation(offset, " total volume (%f m3)", total_volume_m3_);
    }

    if(findKey(MeasurementType::Instantaneous, ValueInformation::EnergyWh, 1, 0, &key, &t->values)) {
        extractDVdouble(&t->values, key, &offset, &target_energy_kwh_);
        t->addMoreExplanation(offset, " target energy consumption (%f kWh)", target_energy_kwh_);
    }

    if(findKey(MeasurementType::Instantaneous, ValueInformation::PowerW, 0, 0, &key, &t->values)) {
        extractDVdouble(&t->values, key, &offset, &current_power_kw_);
        t->addMoreExplanation(offset, " current power consumption (%f kW)", current_power_kw_);
    }

    if(findKey(MeasurementType::Instantaneous, ValueInformation::FlowTemperature, 0, 0, &key, &t->values)) {
        extractDVdouble(&t->values, key, &offset, &flow_temperature_c_);
        t->addMoreExplanation(offset, " flow temperature (%f °C)", flow_temperature_c_);
    }

    if(findKey(MeasurementType::Instantaneous, ValueInformation::ExternalTemperature, 0, 0, &key, &t->values)) {
        extractDVdouble(&t->values, key, &offset, &external_temperature_c_);
        t->addMoreExplanation(offset, " external temperature (%f °C)", external_temperature_c_);
    }

    if(findKey(MeasurementType::Instantaneous, ValueInformation::ReturnTemperature, 0, 0, &key, &t->values)) {
        extractDVdouble(&t->values, key, &offset, &return_temperature_c_);
        t->addMoreExplanation(offset, " return temperature (%f °C)", return_temperature_c_);
    }


}

string MeterElf::status()
{
    string s;

    if (info_codes_ == 0) return "OK";

    s = tostrprintf("%x", info_codes_);
    if (s.length() > 0) {
        s.pop_back(); // Remove final space
        return s;
    }
    return s;
}
