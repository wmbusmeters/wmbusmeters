/*
 Copyright (C) 2018-2021 Fredrik Öhrström (gpl-3.0-or-later)
 Copyright (C)      2020 Eric Bus (gpl-3.0-or-later)

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

#define INFO_CODE_VOLTAGE_INTERRUPTED 1
#define INFO_CODE_LOW_BATTERY_LEVEL 2
#define INFO_CODE_EXTERNAL_ALARM 4
#define INFO_CODE_SENSOR_T1_ABOVE_MEASURING_RANGE 8
#define INFO_CODE_SENSOR_T2_ABOVE_MEASURING_RANGE 16
#define INFO_CODE_SENSOR_T1_BELOW_MEASURING_RANGE 32
#define INFO_CODE_SENSOR_T2_BELOW_MEASURING_RANGE 64
#define INFO_CODE_TEMP_DIFF_WRONG_POLARITY 128

struct MeterMultical602 : public virtual MeterCommonImplementation {
    MeterMultical602(MeterInfo &mi);

    double totalEnergyConsumption(Unit u);
    string status();
    double totalVolume(Unit u);
    double volumeFlow(Unit u);

    // Water temperatures
    double t1Temperature(Unit u);
    bool  hasT1Temperature();
    double t2Temperature(Unit u);
    bool  hasT2Temperature();

private:
    void processContent(Telegram *t);

    uchar info_codes_ {};
    double total_energy_kwh_ {};
    double total_volume_m3_ {};
    double volume_flow_m3h_ {};
    double t1_temperature_c_ { 127 };
    bool has_t1_temperature_ {};
    double t2_temperature_c_ { 127 };
    bool has_t2_temperature_ {};
    string target_date_ {};

    uint32_t energy_forward_kwh_ {};
    uint32_t energy_returned_kwh_ {};
};

MeterMultical602::MeterMultical602(MeterInfo &mi) :
    MeterCommonImplementation(mi, "multical602")
{
    setMeterType(MeterType::HeatMeter);

    setExpectedELLSecurityMode(ELLSecurityMode::AES_CTR);

    addLinkMode(LinkMode::C1);

    addPrint("total_energy_consumption", Quantity::Energy,
             [&](Unit u){ return totalEnergyConsumption(u); },
             "The total energy consumption recorded by this meter.",
             PrintProperty::FIELD | PrintProperty::JSON);

    addPrint("total_volume", Quantity::Volume,
             [&](Unit u){ return totalVolume(u); },
             "Total volume of media.",
             PrintProperty::FIELD | PrintProperty::JSON);

    addPrint("volume_flow", Quantity::Flow,
             [&](Unit u){ return volumeFlow(u); },
             "The current flow.",
             PrintProperty::FIELD | PrintProperty::JSON);

    addPrint("t1_temperature", Quantity::Temperature,
             [&](Unit u){ return t1Temperature(u); },
             "The T1 temperature.",
             PrintProperty::FIELD | PrintProperty::JSON);

    addPrint("t2_temperature", Quantity::Temperature,
             [&](Unit u){ return t2Temperature(u); },
             "The T2 temperature.",
             PrintProperty::FIELD | PrintProperty::JSON);

    addPrint("at_date", Quantity::Text,
             [&](){ return target_date_; },
             "Date when total energy consumption was recorded.",
             PrintProperty::JSON);

    addPrint("current_status", Quantity::Text,
             [&](){ return status(); },
             "Status of meter.",
             PrintProperty::FIELD | PrintProperty::JSON);

    addPrint("energy_forward", Quantity::Energy,
             [&](Unit u){ assertQuantity(u, Quantity::Energy); return convert(energy_forward_kwh_, Unit::KWH, u); },
             "Energy forward.",
             PrintProperty::JSON);

    addPrint("energy_returned", Quantity::Energy,
             [&](Unit u){ assertQuantity(u, Quantity::Energy); return convert(energy_returned_kwh_, Unit::KWH, u); },
             "Energy returned.",
             PrintProperty::JSON);
}

shared_ptr<Meter> createMultical602(MeterInfo &mi) {
    return shared_ptr<Meter>(new MeterMultical602(mi));
}

double MeterMultical602::totalEnergyConsumption(Unit u)
{
    assertQuantity(u, Quantity::Energy);
    return convert(total_energy_kwh_, Unit::KWH, u);
}

double MeterMultical602::totalVolume(Unit u)
{
    assertQuantity(u, Quantity::Volume);
    return convert(total_volume_m3_, Unit::M3, u);
}

double MeterMultical602::t1Temperature(Unit u)
{
    assertQuantity(u, Quantity::Temperature);
    return convert(t1_temperature_c_, Unit::C, u);
}

bool MeterMultical602::hasT1Temperature()
{
    return has_t1_temperature_;
}

double MeterMultical602::t2Temperature(Unit u)
{
    assertQuantity(u, Quantity::Temperature);
    return convert(t2_temperature_c_, Unit::C, u);
}

bool MeterMultical602::hasT2Temperature()
{
    return has_t2_temperature_;
}

double MeterMultical602::volumeFlow(Unit u)
{
    assertQuantity(u, Quantity::Flow);
    return convert(volume_flow_m3h_, Unit::M3H, u);
}

void MeterMultical602::processContent(Telegram *t)
{
    /*
      (multical602) 14: 02 dif (16 Bit Integer/Binary Instantaneous value)
      (multical602) 15: F9 vif (Enhanced identification)
      (multical602) 16: FF vife (additive correction constant: unit of VIF * 10^0)
      (multical602) 17: 15 vife (?)
      (multical602) 18: 1113
      (multical602) 1a: 04 dif (32 Bit Integer/Binary Instantaneous value)
      (multical602) 1b: 06 vif (Energy kWh)
      (multical602) 1c: * 690B0100 total energy consumption (68457.000000 kWh)
      (multical602) 20: 04 dif (32 Bit Integer/Binary Instantaneous value)
      (multical602) 21: EE vif (Units for H.C.A.)
      (multical602) 22: FF vife (additive correction constant: unit of VIF * 10^0)
      (multical602) 23: 07 vife (?)
      (multical602) 24: C1BC0200
      (multical602) 28: 04 dif (32 Bit Integer/Binary Instantaneous value)
      (multical602) 29: EE vif (Units for H.C.A.)
      (multical602) 2a: FF vife (additive correction constant: unit of VIF * 10^0)
      (multical602) 2b: 08 vife (?)
      (multical602) 2c: 90D40100
      (multical602) 30: 04 dif (32 Bit Integer/Binary Instantaneous value)
      (multical602) 31: 14 vif (Volume 10⁻² m³)
      (multical602) 32: * A9250400 total volume (2717.850000 m3)
      (multical602) 36: 84 dif (32 Bit Integer/Binary Instantaneous value)
      (multical602) 37: 40 dife (subunit=1 tariff=0 storagenr=0)
      (multical602) 38: 14 vif (Volume 10⁻² m³)
      (multical602) 39: 00000000
      (multical602) 3d: 84 dif (32 Bit Integer/Binary Instantaneous value)
      (multical602) 3e: 80 dife (subunit=0 tariff=0 storagenr=0)
      (multical602) 3f: 40 dife (subunit=2 tariff=0 storagenr=0)
      (multical602) 40: 14 vif (Volume 10⁻² m³)
      (multical602) 41: 00000000
      (multical602) 45: 02 dif (16 Bit Integer/Binary Instantaneous value)
      (multical602) 46: FD vif (Second extension FD of VIF-codes)
      (multical602) 47: 17 vife (Error flags (binary))
      (multical602) 48: 0000
      (multical602) 4a: 02 dif (16 Bit Integer/Binary Instantaneous value)
      (multical602) 4b: 6C vif (Date type G)
      (multical602) 4c: * B929 target date (2021-09-25 00:00)
      (multical602) 4e: 42 dif (16 Bit Integer/Binary Instantaneous value storagenr=1)
      (multical602) 4f: 6C vif (Date type G)
      (multical602) 50: BF28
      (multical602) 52: 44 dif (32 Bit Integer/Binary Instantaneous value storagenr=1)
      (multical602) 53: 06 vif (Energy kWh)
      (multical602) 54: 100A0100
      (multical602) 58: 44 dif (32 Bit Integer/Binary Instantaneous value storagenr=1)
      (multical602) 59: 14 vif (Volume 10⁻² m³)
      (multical602) 5a: D81A0400
      (multical602) 5e: C4 dif (32 Bit Integer/Binary Instantaneous value storagenr=1)
      (multical602) 5f: 40 dife (subunit=1 tariff=0 storagenr=1)
      (multical602) 60: 14 vif (Volume 10⁻² m³)
      (multical602) 61: 00000000
      (multical602) 65: C4 dif (32 Bit Integer/Binary Instantaneous value storagenr=1)
      (multical602) 66: 80 dife (subunit=0 tariff=0 storagenr=1)
      (multical602) 67: 40 dife (subunit=2 tariff=0 storagenr=1)
      (multical602) 68: 14 vif (Volume 10⁻² m³)
      (multical602) 69: 00000000
      (multical602) 6d: 04 dif (32 Bit Integer/Binary Instantaneous value)
      (multical602) 6e: 3B vif (Volume flow l/h)
      (multical602) 6f: * 39000000 volume flow (0.057000 m3/h)
      (multical602) 73: 02 dif (16 Bit Integer/Binary Instantaneous value)
      (multical602) 74: 59 vif (Flow temperature 10⁻² °C)
      (multical602) 75: * 2A17 T1 flow temperature (59.300000 °C)
      (multical602) 77: 02 dif (16 Bit Integer/Binary Instantaneous value)
      (multical602) 78: 5D vif (Return temperature 10⁻² °C)
      (multical602) 79: * 2912 T2 flow temperature (46.490000 °C)
    */
    int offset;
    string key;

    if(findKey(MeasurementType::Instantaneous, VIFRange::EnergyWh, 0, 0, &key, &t->values)) {
        extractDVdouble(&t->values, key, &offset, &total_energy_kwh_);
        t->addMoreExplanation(offset, " total energy consumption (%f kWh)", total_energy_kwh_);
    }

    if(findKey(MeasurementType::Instantaneous, VIFRange::Volume, 0, 0, &key, &t->values)) {
        extractDVdouble(&t->values, key, &offset, &total_volume_m3_);
        t->addMoreExplanation(offset, " total volume (%f m3)", total_volume_m3_);
    }

    if(findKey(MeasurementType::Unknown, VIFRange::VolumeFlow, 0, 0, &key, &t->values)) {
        extractDVdouble(&t->values, key, &offset, &volume_flow_m3h_);
        t->addMoreExplanation(offset, " volume flow (%f m3/h)", volume_flow_m3h_);
    }

    if(findKey(MeasurementType::Instantaneous, VIFRange::FlowTemperature, 0, 0, &key, &t->values)) {
        has_t1_temperature_ = extractDVdouble(&t->values, key, &offset, &t1_temperature_c_);
        t->addMoreExplanation(offset, " T1 flow temperature (%f °C)", t1_temperature_c_);
    }

    if(findKey(MeasurementType::Instantaneous, VIFRange::ReturnTemperature, 0, 0, &key, &t->values)) {
        has_t2_temperature_ = extractDVdouble(&t->values, key, &offset, &t2_temperature_c_);
        t->addMoreExplanation(offset, " T2 flow temperature (%f °C)", t2_temperature_c_);
    }

    if (findKey(MeasurementType::Unknown, VIFRange::Date, 0, 0, &key, &t->values)) {
        struct tm datetime;
        extractDVdate(&t->values, key, &offset, &datetime);
        target_date_ = strdatetime(&datetime);
        t->addMoreExplanation(offset, " target date (%s)", target_date_.c_str());
    }
}

string MeterMultical602::status()
{
    string s;
    if (info_codes_ & INFO_CODE_VOLTAGE_INTERRUPTED) s.append("VOLTAGE_INTERRUPTED ");
    if (info_codes_ & INFO_CODE_LOW_BATTERY_LEVEL) s.append("LOW_BATTERY_LEVEL ");
    if (info_codes_ & INFO_CODE_EXTERNAL_ALARM) s.append("EXTERNAL_ALARM ");
    if (info_codes_ & INFO_CODE_SENSOR_T1_ABOVE_MEASURING_RANGE) s.append("SENSOR_T1_ABOVE_MEASURING_RANGE ");
    if (info_codes_ & INFO_CODE_SENSOR_T2_ABOVE_MEASURING_RANGE) s.append("SENSOR_T2_ABOVE_MEASURING_RANGE ");
    if (info_codes_ & INFO_CODE_SENSOR_T1_BELOW_MEASURING_RANGE) s.append("SENSOR_T1_BELOW_MEASURING_RANGE ");
    if (info_codes_ & INFO_CODE_SENSOR_T2_BELOW_MEASURING_RANGE) s.append("SENSOR_T2_BELOW_MEASURING_RANGE ");
    if (info_codes_ & INFO_CODE_TEMP_DIFF_WRONG_POLARITY) s.append("TEMP_DIFF_WRONG_POLARITY ");
    if (s.length() > 0) {
        s.pop_back(); // Remove final space
        return s;
    }
    return s;
}
