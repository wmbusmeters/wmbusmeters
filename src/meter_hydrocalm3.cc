/*
 Copyright (C) 2018-2021 Fredrik Öhrström (gpl-3.0-or-later)

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

struct MeterHydrocalM3 : public virtual MeterCommonImplementation {
    MeterHydrocalM3(MeterInfo &mi);

private:
    void processContent(Telegram *t);

    double total_heating_energy_kwh_ {};
    double total_heating_volume_m3_ {};
    double total_cooling_energy_kwh_ {};
    double total_cooling_volume_m3_ {};
    double t1_temperature_c_ { 127 };
    double t2_temperature_c_ { 127 };
    double c1_volume_m3_ {};
    double c2_volume_m3_ {};
    string device_date_time_ {};
};

MeterHydrocalM3::MeterHydrocalM3(MeterInfo &mi) :
    MeterCommonImplementation(mi, "hydrocalm3")
{
    setMeterType(MeterType::HeatMeter);

    setExpectedELLSecurityMode(ELLSecurityMode::AES_CTR);

    addLinkMode(LinkMode::T1);

    addPrint("total_heating", Quantity::Energy,
             [&](Unit u){ assertQuantity(u, Quantity::Energy); return convert(total_heating_energy_kwh_, Unit::KWH, u); },
             "The total heating energy consumption recorded by this meter.",
             PrintProperty::FIELD | PrintProperty::JSON);

    addPrint("total_cooling", Quantity::Energy,
             [&](Unit u){ assertQuantity(u, Quantity::Energy); return convert(total_cooling_energy_kwh_, Unit::KWH, u); },
             "The total cooling energy consumption recorded by this meter.",
             PrintProperty::FIELD | PrintProperty::JSON);

    addPrint("device_date_time", Quantity::Text,
             [&](){ return device_date_time_; },
             "Date when total energy consumption was recorded.",
             PrintProperty::JSON);

    addPrint("total_heating", Quantity::Volume,
             [&](Unit u){ assertQuantity(u, Quantity::Volume); return convert(total_heating_volume_m3_, Unit::M3, u); },
             "Total heating volume of media.",
             PrintProperty::JSON);

    addPrint("total_cooling", Quantity::Volume,
             [&](Unit u){ assertQuantity(u, Quantity::Volume); return convert(total_cooling_volume_m3_, Unit::M3, u); },
             "Total cooling volume of media.",
             PrintProperty::JSON);

    addPrint("c1_volume", Quantity::Volume,
             [&](Unit u){ assertQuantity(u, Quantity::Volume); return convert(c1_volume_m3_, Unit::M3, u); },
             "Supply c1 volume.",
             PrintProperty::JSON);

    addPrint("c2_volume", Quantity::Volume,
             [&](Unit u){ assertQuantity(u, Quantity::Volume); return convert(c2_volume_m3_, Unit::M3, u); },
             "Return c2 volume.",
             PrintProperty::JSON);

    addPrint("supply_temperature", Quantity::Temperature,
             [&](Unit u){ return convert(t1_temperature_c_, Unit::C, u); },
             "The supply t1 pipe temperature.",
             PrintProperty::JSON);

    addPrint("return_temperature", Quantity::Temperature,
             [&](Unit u){ return convert(t2_temperature_c_, Unit::C, u); },
             "The return t2 pipe temperature.",
             PrintProperty::JSON);
}

shared_ptr<Meter> createHydrocalM3(MeterInfo &mi) {
    return shared_ptr<Meter>(new MeterHydrocalM3(mi));
}

void MeterHydrocalM3::processContent(Telegram *t)
{
    int offset;
    string key;

    if (findKey(MeasurementType::Instantaneous, ValueInformation::DateTime, 0, 0, &key, &t->values))
    {
        struct tm datetime;
        extractDVdate(&t->values, key, &offset, &datetime);
        device_date_time_ = strdatetime(&datetime);
        t->addMoreExplanation(offset, " device date time (%s)", device_date_time_.c_str());
    }

    // The meter either sends the total energy consumed as kWh or as MJ.
    // First look for kwh
    if (findKeyWithNr(MeasurementType::Instantaneous, ValueInformation::EnergyWh, 0, 0, 1, &key, &t->values))
    {
        extractDVdouble(&t->values, key, &offset, &total_heating_energy_kwh_);
        t->addMoreExplanation(offset, " total heating energy consumption (%f kWh)", total_heating_energy_kwh_);
    }
    // Then look for mj.
    if (findKeyWithNr(MeasurementType::Instantaneous, ValueInformation::EnergyMJ, 0, 0, 1, &key, &t->values))
    {
        double mj;
        extractDVdouble(&t->values, key, &offset, &mj);
        total_heating_energy_kwh_ = convert(mj, Unit::MJ, Unit::KWH);
        t->addMoreExplanation(offset, " total heating energy consumption (%f MJ = %f kWh)", mj, total_heating_energy_kwh_);
    }
    if (findKeyWithNr(MeasurementType::Instantaneous, ValueInformation::Volume, 0, 0, 1, &key, &t->values))
    {
        extractDVdouble(&t->values, key, &offset, &total_heating_volume_m3_);
        t->addMoreExplanation(offset, " total heating volume (%f m3)", total_heating_volume_m3_);
    }
    // Now look for cooling energy, which uses the same DIFVIF but follows the first set of data.
    if (findKeyWithNr(MeasurementType::Instantaneous, ValueInformation::EnergyWh, 0, 0, 2, &key, &t->values))
    {
        extractDVdouble(&t->values, key, &offset, &total_cooling_energy_kwh_);
        t->addMoreExplanation(offset, " total cooling energy consumption (%f kWh)", total_cooling_energy_kwh_);
    }
    // Then look for mj.
    if (findKeyWithNr(MeasurementType::Instantaneous, ValueInformation::EnergyMJ, 0, 0, 2, &key, &t->values))
    {
        double mj;
        extractDVdouble(&t->values, key, &offset, &mj);
        total_cooling_energy_kwh_ = convert(mj, Unit::MJ, Unit::KWH);
        t->addMoreExplanation(offset, " total cooling energy consumption (%f MJ = %f kWh)", mj, total_cooling_energy_kwh_);
    }

    if (findKeyWithNr(MeasurementType::Instantaneous, ValueInformation::Volume, 0, 0, 2, &key, &t->values))
    {
        extractDVdouble(&t->values, key, &offset, &total_cooling_volume_m3_);
        t->addMoreExplanation(offset, " total cooling volume (%f m3)", total_cooling_volume_m3_);
    }

    if (findKeyWithNr(MeasurementType::Instantaneous, ValueInformation::Volume, 0, 0, 3, &key, &t->values))
    {
        extractDVdouble(&t->values, key, &offset, &c1_volume_m3_);
        t->addMoreExplanation(offset, " volume C1 (%f m3)", c1_volume_m3_);
    }

    if (findKeyWithNr(MeasurementType::Instantaneous, ValueInformation::Volume, 0, 0, 4, &key, &t->values))
    {
        extractDVdouble(&t->values, key, &offset, &c2_volume_m3_);
        t->addMoreExplanation(offset, " volume C2 (%f m3)", c2_volume_m3_);
    }

    if(findKey(MeasurementType::Instantaneous, ValueInformation::FlowTemperature, 0, 0, &key, &t->values)) {
        extractDVdouble(&t->values, key, &offset, &t1_temperature_c_);
        t->addMoreExplanation(offset, " supply temperature T1 (%f °C)", t1_temperature_c_);
    }

    if(findKey(MeasurementType::Instantaneous, ValueInformation::ReturnTemperature, 0, 0, &key, &t->values)) {
        extractDVdouble(&t->values, key, &offset, &t2_temperature_c_);
        t->addMoreExplanation(offset, " return temperature T2 (%f °C)", t2_temperature_c_);
    }
}
