/*
 Copyright (C) 2018-2020 Fredrik Öhrström
                    2020 Eric Bus
                    2020 Nikodem Jędrzejczak

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

struct MeterMultical803 : public virtual HeatMeter, public virtual MeterCommonImplementation {
    MeterMultical803(MeterInfo &mi);

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
    double total_energy_gj_ {};
    double total_volume_m3_ {};
    double volume_flow_m3h_ {};
    double t1_temperature_c_ { 127 };
    bool has_t1_temperature_ {};
    double t2_temperature_c_ { 127 };
    bool has_t2_temperature_ {};
    string target_date_ {};

    uint32_t energy_forward_gj_ {};
    uint32_t energy_returned_gj_ {};
};

MeterMultical803::MeterMultical803(MeterInfo &mi) :
    MeterCommonImplementation(mi, MeterType::MULTICAL803)
{
    setExpectedELLSecurityMode(ELLSecurityMode::AES_CTR);

    addLinkMode(LinkMode::C1);

    addPrint("total_energy_consumption", Quantity::Energy,
             [&](Unit u){ return totalEnergyConsumption(u); },
             "The total energy consumption recorded by this meter.",
             true, true);

    addPrint("total_volume", Quantity::Volume,
             [&](Unit u){ return totalVolume(u); },
             "Total volume of media.",
             true, true);

    addPrint("volume_flow", Quantity::Flow,
             [&](Unit u){ return volumeFlow(u); },
             "The current flow.",
             true, true);

    addPrint("t1_temperature", Quantity::Temperature,
             [&](Unit u){ return t1Temperature(u); },
             "The T1 temperature.",
             true, true);

    addPrint("t2_temperature", Quantity::Temperature,
             [&](Unit u){ return t2Temperature(u); },
             "The T2 temperature.",
             true, true);

    addPrint("at_date", Quantity::Text,
             [&](){ return target_date_; },
             "Date when total energy consumption was recorded.",
             false, true);

    addPrint("current_status", Quantity::Text,
             [&](){ return status(); },
             "Status of meter.",
             true, true);

    addPrint("energy_forward", Quantity::Energy,
             [&](Unit u){ assertQuantity(u, Quantity::Energy); return convert(energy_forward_gj_, Unit::GJ, u); },
             "Energy forward.",
             false, true);

    addPrint("energy_returned", Quantity::Energy,
             [&](Unit u){ assertQuantity(u, Quantity::Energy); return convert(energy_returned_gj_, Unit::GJ, u); },
             "Energy returned.",
             false, true);
}

shared_ptr<HeatMeter> createMultical803(MeterInfo &mi) {
    return shared_ptr<HeatMeter>(new MeterMultical803(mi));
}

double MeterMultical803::totalEnergyConsumption(Unit u)
{
    assertQuantity(u, Quantity::Energy);
    return convert(total_energy_gj_, Unit::KWH, u);
}

double MeterMultical803::totalVolume(Unit u)
{
    assertQuantity(u, Quantity::Volume);
    return convert(total_volume_m3_, Unit::M3, u);
}

double MeterMultical803::t1Temperature(Unit u)
{
    assertQuantity(u, Quantity::Temperature);
    return convert(t1_temperature_c_, Unit::C, u);
}

bool MeterMultical803::hasT1Temperature()
{
    return has_t1_temperature_;
}

double MeterMultical803::t2Temperature(Unit u)
{
    assertQuantity(u, Quantity::Temperature);
    return convert(t2_temperature_c_, Unit::C, u);
}

bool MeterMultical803::hasT2Temperature()
{
    return has_t2_temperature_;
}

double MeterMultical803::volumeFlow(Unit u)
{
    assertQuantity(u, Quantity::Flow);
    return convert(volume_flow_m3h_, Unit::M3H, u);
}

void MeterMultical803::processContent(Telegram *t)
{
    /*
      (multical803) 13: 78 tpl-ci-field (EN 13757-3 Application Layer (no tplh))
      (multical803) 14: 04 dif (32 Bit Integer/Binary Instantaneous value)
      (multical803) 15: 06 vif (Energy kWh)
      (multical803) 16: * A5000000 total energy consumption (165.000000 kWh)
      (multical803) 1a: 04 dif (32 Bit Integer/Binary Instantaneous value)
      (multical803) 1b: FF vif (Vendor extension)
      (multical803) 1c: 07 vife (?)
      (multical803) 1d: 2B010000
      (multical803) 21: 04 dif (32 Bit Integer/Binary Instantaneous value)
      (multical803) 22: FF vif (Vendor extension)
      (multical803) 23: 08 vife (?)
      (multical803) 24: 9C000000
      (multical803) 28: 04 dif (32 Bit Integer/Binary Instantaneous value)
      (multical803) 29: 14 vif (Volume 10⁻² m³)
      (multical803) 2a: * 21020000 total volume (5.450000 m3)
      (multical803) 2e: 04 dif (32 Bit Integer/Binary Instantaneous value)
      (multical803) 2f: 3B vif (Volume flow l/h)
      (multical803) 30: * 12000000 volume flow (0.018000 m3/h)
      (multical803) 34: 02 dif (16 Bit Integer/Binary Instantaneous value)
      (multical803) 35: 59 vif (Flow temperature 10⁻² °C)
      (multical803) 36: * D014 T1 flow temperature (53.280000 °C)
      (multical803) 38: 02 dif (16 Bit Integer/Binary Instantaneous value)
      (multical803) 39: 5D vif (Return temperature 10⁻² °C)
      (multical803) 3a: * 0009 T2 flow temperature (23.040000 °C)
      (multical803) 3c: 04 dif (32 Bit Integer/Binary Instantaneous value)
      (multical803) 3d: FF vif (Vendor extension)
      (multical803) 3e: 22 vife (per hour)
      (multical803) 3f: * 00000000 info codes ()
*/
    int offset;
    string key;

    extractDVuint8(&t->values, "04FF22", &offset, &info_codes_);
    t->addMoreExplanation(offset, " info codes (%s)", status().c_str());

    extractDVuint32(&t->values, "04FF07", &offset, &energy_forward_gj_);
    t->addMoreExplanation(offset, " energy forward gj (%zu)", energy_forward_gj_);

    extractDVuint32(&t->values, "04FF08", &offset, &energy_returned_gj_);
    t->addMoreExplanation(offset, " energy returned gj (%zu)", energy_returned_gj_);

    if(findKey(MeasurementType::Instantaneous, ValueInformation::EnergyWh, 0, 0, &key, &t->values)) {
        extractDVdouble(&t->values, key, &offset, &total_energy_gj_);
        t->addMoreExplanation(offset, " total energy consumption (%f kWh)", total_energy_gj_);
    }

    if(findKey(MeasurementType::Instantaneous, ValueInformation::Volume, 0, 0, &key, &t->values)) {
        extractDVdouble(&t->values, key, &offset, &total_volume_m3_);
        t->addMoreExplanation(offset, " total volume (%f m3)", total_volume_m3_);
    }

    if(findKey(MeasurementType::Unknown, ValueInformation::VolumeFlow, 0, 0, &key, &t->values)) {
        extractDVdouble(&t->values, key, &offset, &volume_flow_m3h_);
        t->addMoreExplanation(offset, " volume flow (%f m3/h)", volume_flow_m3h_);
    }

    if(findKey(MeasurementType::Instantaneous, ValueInformation::FlowTemperature, 0, 0, &key, &t->values)) {
        has_t1_temperature_ = extractDVdouble(&t->values, key, &offset, &t1_temperature_c_);
        t->addMoreExplanation(offset, " T1 flow temperature (%f °C)", t1_temperature_c_);
    }

    if(findKey(MeasurementType::Instantaneous, ValueInformation::ReturnTemperature, 0, 0, &key, &t->values)) {
        has_t2_temperature_ = extractDVdouble(&t->values, key, &offset, &t2_temperature_c_);
        t->addMoreExplanation(offset, " T2 flow temperature (%f °C)", t2_temperature_c_);
    }

    if (findKey(MeasurementType::Unknown, ValueInformation::Date, 0, 0, &key, &t->values)) {
        struct tm datetime;
        extractDVdate(&t->values, key, &offset, &datetime);
        target_date_ = strdatetime(&datetime);
        t->addMoreExplanation(offset, " target date (%s)", target_date_.c_str());
    }
}

string MeterMultical803::status()
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
