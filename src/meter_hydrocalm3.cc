/*
 Copyright (C) 2018-2020 Fredrik Öhrström
                    2020 Eric Bus

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

struct MeterHydrocalM3 : public virtual HeatMeter, public virtual MeterCommonImplementation {
    MeterHydrocalM3(MeterInfo &mi);

    double totalHeatingEnergyConsumption(Unit u);
    double totalHeatingVolume(Unit u);
    double totalCoolingEnergyConsumption(Unit u);
    double totalCoolingVolume(Unit u);

private:
    void processContent(Telegram *t);

    double total_heating_energy_kwh_ {};
    double total_heating_volume_m3_ {};
    double total_cooling_energy_kwh_ {};
    double total_cooling_volume_m3_ {};
    string device_date_time_ {};
};

MeterHydrocalM3::MeterHydrocalM3(MeterInfo &mi) :
    MeterCommonImplementation(mi, MeterDriver::HYDROCALM3)
{
    setExpectedELLSecurityMode(ELLSecurityMode::AES_CTR);

    addLinkMode(LinkMode::T1);

    addPrint("total_energy_consumption", Quantity::Energy,
             [&](Unit u){ return totalEnergyConsumption(u); },
             "The total energy consumption recorded by this meter.",
             true, true);

    addPrint("device_date_time", Quantity::Text,
             [&](){ return device_date_time_; },
             "Date when total energy consumption was recorded.",
             false, true);

    addPrint("total_volume", Quantity::Volume,
             [&](Unit u){ return totalVolume(u); },
             "Total volume of media.",
             true, true);
}

shared_ptr<HeatMeter> createHydrocalM3(MeterInfo &mi) {
    return shared_ptr<HeatMeter>(new MeterHydrocalM3(mi));
}

double MeterHydrocalM3::totalHeatingEnergyConsumption(Unit u)
{
    assertQuantity(u, Quantity::Energy);
    return convert(total_heating_energy_kwh_, Unit::KWH, u);
}

double MeterHydrocalM3::totalHeatingVolume(Unit u)
{
    assertQuantity(u, Quantity::Volume);
    return convert(total_heating_volume_m3_, Unit::M3, u);
}

double MeterHydrocalM3::totalCoolingEnergyConsumption(Unit u)
{
    assertQuantity(u, Quantity::Energy);
    return convert(total_cooling_energy_kwh_, Unit::KWH, u);
}

double MeterHydrocalM3::totalCoolingVolume(Unit u)
{
    assertQuantity(u, Quantity::Volume);
    return convert(total_cooling_volume_m3_, Unit::M3, u);
}

void MeterHydrocalM3::processContent(Telegram *t)
{
    int offset;
    string key;

    // The meter either sends the total energy consumed as kWh or as MJ.
    // First look for kwh
    if(findKey(MeasurementType::Instantaneous, ValueInformation::EnergyWh, 0, 0, &key, &t->values)) {
        extractDVdouble(&t->values, key, &offset, &total_heating_energy_kwh_);
        t->addMoreExplanation(offset, " total heating energy consumption (%f kWh)", total_heating_energy_kwh_);
    }
    // Then look for mj.
    if(findKey(MeasurementType::Instantaneous, ValueInformation::EnergyMJ, 0, 0, &key, &t->values)) {
        double mj;
        extractDVdouble(&t->values, key, &offset, &mj);
        total_heating_energy_kwh_ = convert(mj, Unit::MJ, Unit::KWH);
        t->addMoreExplanation(offset, " total heating_energy consumption (%f MJ = %f kWh)", mj, total_heating_energy_kwh_);
    }

    if (findKey(MeasurementType::Instantaneous, ValueInformation::DateTime, 0, 0, &key, &t->values)) {
        struct tm datetime;
        extractDVdate(&t->values, key, &offset, &datetime);
        device_date_time_ = strdatetime(&datetime);
        t->addMoreExplanation(offset, " device date time (%s)", device_date_time_.c_str());
    }

    if(findKey(MeasurementType::Instantaneous, ValueInformation::Volume, 0, 0, &key, &t->values)) {
        extractDVdouble(&t->values, key, &offset, &total_heating_volume_m3_);
        t->addMoreExplanation(offset, " total heating_volume (%f m3)", total_heating_volume_m3_);
    }

}
