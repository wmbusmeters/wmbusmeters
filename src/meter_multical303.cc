/*
 Copyright (C) 2018-2020 Fredrik Öhrström (gpl-3.0-or-later)
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
#define INFO_CODE_SENSOR_T1_ABOVE_MEASURING_RANGE 8
#define INFO_CODE_SENSOR_T2_ABOVE_MEASURING_RANGE 16
#define INFO_CODE_SENSOR_T1_BELOW_MEASURING_RANGE 32
#define INFO_CODE_SENSOR_T2_BELOW_MEASURING_RANGE 64
#define INFO_CODE_TEMP_DIFF_WRONG_POLARITY 128
#define INFO_CODE_FLOW_SENSOR_WEAK_OR_AIR 256
#define INFO_CODE_WRONG_FLOW_DIRECTION 512
#define INFO_CODE_UNKNOWN 1024
#define INFO_CODE_FLOW_INCREASED 2048


struct MeterMultical303 : public virtual MeterCommonImplementation {
    MeterMultical303(MeterInfo &mi);

    double totalEnergyConsumption(Unit u);
    string status();
    double totalVolume(Unit u);
    double volumeFlow(Unit u);

    // Water temperatures
    double t1Temperature(Unit u);
    bool  hasT1Temperature(); // possible not a value in telegramm
    double t2Temperature(Unit u);
    bool  hasT2Temperature(); // possible not a value in telegramm
    // Target
    double targetEnergyConsumption(Unit u);
    double targetVolume(Unit u);
   
private:
    void processContent(Telegram *t);

    uchar info_codes_ {};
    double total_energy_kwh_ {};
    double total_volume_m3_ {};
    double volume_flow_m3h_ {};
    double t1_temperature_c_ { 127 };
    bool has_t1_temperature_ {}; // possible not a value in telegramm
    double t2_temperature_c_ { 127 };
    bool has_t2_temperature_ {}; // possible not a value in telegramm
    string target_date_ {};  // Byte 078 holds another target date, don´t know how to solve ??

    uint32_t energy_forward_kwh_ {};
    uint32_t energy_returned_kwh_ {};
    double target_energy_kwh_ {};
    double target_volume_m3_ {};
};

MeterMultical303::MeterMultical303(MeterInfo &mi) :
    MeterCommonImplementation(mi, "multical303")
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
     
    addPrint("total_energy_consumption_at_date", Quantity::Energy,
        [&](Unit u){ return targetEnergyConsumption(u); },
        "The total energy consumption recorded at the target date.",
        PrintProperty::JSON);
}

shared_ptr<Meter> createMultical303(MeterInfo &mi) {
    return shared_ptr<Meter>(new MeterMultical303(mi));
}

double MeterMultical303::totalEnergyConsumption(Unit u)
{
    assertQuantity(u, Quantity::Energy);
    return convert(total_energy_kwh_, Unit::KWH, u);
}

double MeterMultical303::totalVolume(Unit u)
{
    assertQuantity(u, Quantity::Volume);
    return convert(total_volume_m3_, Unit::M3, u);
}

double MeterMultical303::t1Temperature(Unit u)
{
    assertQuantity(u, Quantity::Temperature);
    return convert(t1_temperature_c_, Unit::C, u);
}

bool MeterMultical303::hasT1Temperature()
{
    return has_t1_temperature_;
}

double MeterMultical303::t2Temperature(Unit u)
{
    assertQuantity(u, Quantity::Temperature);
    return convert(t2_temperature_c_, Unit::C, u);
}

bool MeterMultical303::hasT2Temperature()
{
    return has_t2_temperature_;
}

double MeterMultical303::volumeFlow(Unit u)
{
    assertQuantity(u, Quantity::Flow);
    return convert(volume_flow_m3h_, Unit::M3H, u);
}

double MeterMultical302::targetEnergyConsumption(Unit u)
{
    assertQuantity(u, Quantity::Energy);
    return convert(target_energy_kwh_, Unit::KWH, u);
}

void MeterMultical303::processContent(Telegram *t)
{
    /*
      +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
      ++++++++++++++++++++ Will be updated if the driver is working +++++++++++++
      +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
      (multical303) 13: 78 tpl-ci-field (EN 13757-3 Application Layer (no tplh))
      (multical303) 14: 04 dif (32 Bit Integer/Binary Instantaneous value)
      (multical303) 15: 06 vif (Energy kWh)
      (multical303) 16: * A5000000 total energy consumption (165.000000 kWh)
      (multical303) 1a: 04 dif (32 Bit Integer/Binary Instantaneous value)
      (multical303) 1b: FF vif (Vendor extension)
      (multical303) 1c: 07 vife (?)
      (multical303) 1d: 2B010000
      (multical303) 21: 04 dif (32 Bit Integer/Binary Instantaneous value)
      (multical303) 22: FF vif (Vendor extension)
      (multical303) 23: 08 vife (?)
      (multical303) 24: 9C000000
      (multical303) 28: 04 dif (32 Bit Integer/Binary Instantaneous value)
      (multical303) 29: 14 vif (Volume 10⁻² m³)
      (multical303) 2a: * 21020000 total volume (5.450000 m3)
      (multical303) 2e: 04 dif (32 Bit Integer/Binary Instantaneous value)
      (multical303) 2f: 3B vif (Volume flow l/h)
      (multical303) 30: * 12000000 volume flow (0.018000 m3/h)
      (multical303) 34: 02 dif (16 Bit Integer/Binary Instantaneous value)
      (multical303) 35: 59 vif (Flow temperature 10⁻² °C)
      (multical303) 36: * D014 T1 flow temperature (53.280000 °C)
      (multical303) 38: 02 dif (16 Bit Integer/Binary Instantaneous value)
      (multical303) 39: 5D vif (Return temperature 10⁻² °C)
      (multical303) 3a: * 0009 T2 flow temperature (23.040000 °C)
      (multical303) 3c: 04 dif (32 Bit Integer/Binary Instantaneous value)
      (multical303) 3d: FF vif (Vendor extension)
      (multical303) 3e: 22 vife (per hour)
      (multical303) 3f: * 00000000 info codes ()
*/
    int offset;
    string key;

    extractDVuint8(&t->dv_entries, "04FF22", &offset, &info_codes_);
    t->addMoreExplanation(offset, " info codes (%s)", status().c_str());

    extractDVuint32(&t->dv_entries, "04FF07", &offset, &energy_forward_kwh_);
    t->addMoreExplanation(offset, " energy forward kwh (%zu)", energy_forward_kwh_);

    extractDVuint32(&t->dv_entries, "04FF08", &offset, &energy_returned_kwh_);
    t->addMoreExplanation(offset, " energy returned kwh (%zu)", energy_returned_kwh_);

    if(findKey(MeasurementType::Instantaneous, VIFRange::EnergyWh, 0, 0, &key, &t->dv_entries)) {
        extractDVdouble(&t->dv_entries, key, &offset, &total_energy_kwh_);
        t->addMoreExplanation(offset, " total energy consumption (%f kWh)", total_energy_kwh_);
    }

    if(findKey(MeasurementType::Instantaneous, VIFRange::Volume, 0, 0, &key, &t->dv_entries)) {
        extractDVdouble(&t->dv_entries, key, &offset, &total_volume_m3_);
        t->addMoreExplanation(offset, " total volume (%f m3)", total_volume_m3_);
    }

    if(findKey(MeasurementType::Instantaneous, VIFRange::VolumeFlow, 0, 0, &key, &t->dv_entries)) {
        extractDVdouble(&t->dv_entries, key, &offset, &volume_flow_m3h_);
        t->addMoreExplanation(offset, " volume flow (%f m3/h)", volume_flow_m3h_);
    }

    if(findKey(MeasurementType::Instantaneous, VIFRange::FlowTemperature, 0, 0, &key, &t->dv_entries)) {
        has_t1_temperature_ = extractDVdouble(&t->dv_entries, key, &offset, &t1_temperature_c_);
        t->addMoreExplanation(offset, " T1 flow temperature (%f °C)", t1_temperature_c_);
    }

    if(findKey(MeasurementType::Instantaneous, VIFRange::ReturnTemperature, 0, 0, &key, &t->dv_entries)) {
        has_t2_temperature_ = extractDVdouble(&t->dv_entries, key, &offset, &t2_temperature_c_);
        t->addMoreExplanation(offset, " T2 flow temperature (%f °C)", t2_temperature_c_);
    }

    if(findKey(MeasurementType::Instantaneous, VIFRange::Date, 0, 0, &key, &t->dv_entries)) {
        struct tm datetime;
        extractDVdate(&t->dv_entries, key, &offset, &datetime);
        target_date_ = strdatetime(&datetime);
        t->addMoreExplanation(offset, " target date (%s)", target_date_.c_str());
    }
 
    if(findKey(MeasurementType::Instantaneous, VIFRange::EnergyWh, 1, 0, &key, &t->dv_entries)) {
        extractDVdouble(&t->dv_entries, key, &offset, &target_energy_kwh_);
        t->addMoreExplanation(offset, " target energy consumption (%f kWh)", target_energy_kwh_);
    } else if(findKey(MeasurementType::Instantaneous, VIFRange::EnergyMJ, 1, 0, &key, &t->dv_entries)){
        double mj;
        extractDVdouble(&t->dv_entries, key, &offset, &mj);
        target_energy_kwh_ = convert(mj, Unit::MJ, Unit::KWH);
        t->addMoreExplanation(offset, " target energy consumption (%f kWh)", target_energy_kwh_);
    }
}

string MeterMultical303::status()
{
    string s;
    if (info_codes_ & INFO_CODE_VOLTAGE_INTERRUPTED) s.append("VOLTAGE_INTERRUPTED ");
    if (info_codes_ & INFO_CODE_LOW_BATTERY_LEVEL) s.append("LOW_BATTERY_LEVEL ");
    if (info_codes_ & INFO_CODE_SENSOR_T1_ABOVE_MEASURING_RANGE) s.append("SENSOR_T1_ABOVE_MEASURING_RANGE ");
    if (info_codes_ & INFO_CODE_SENSOR_T2_ABOVE_MEASURING_RANGE) s.append("SENSOR_T2_ABOVE_MEASURING_RANGE ");
    if (info_codes_ & INFO_CODE_SENSOR_T1_BELOW_MEASURING_RANGE) s.append("SENSOR_T1_BELOW_MEASURING_RANGE ");
    if (info_codes_ & INFO_CODE_SENSOR_T2_BELOW_MEASURING_RANGE) s.append("SENSOR_T2_BELOW_MEASURING_RANGE ");
    if (info_codes_ & INFO_CODE_TEMP_DIFF_WRONG_POLARITY) s.append("TEMP_DIFF_WRONG_POLARITY ");
    if (info_codes_ & INFO_CODE_FLOW_SENSOR_WEAK_OR_AIR) s.append("FLOW_SENSOR_WEAK_OR_AIR ");
    if (info_codes_ & INFO_CODE_WRONG_FLOW_DIRECTION) s.append("WRONG_FLOW_DIRECTION ");
    if (info_codes_ & 64) s.append("UNKNOWN_64 ");
    if (info_codes_ & INFO_CODE_FLOW_INCREASED) s.append("FLOW_INCREASED "); 
    if (s.length() > 0) {
        s.pop_back(); // Remove final space
        return s;
    }
    return s;
}
