/*
 Copyright (C) 2019-2020 Fredrik Öhrström

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
#include<cmath>

struct MeterAmiplus : public virtual ElectricityMeter, public virtual MeterCommonImplementation {
    MeterAmiplus(MeterInfo &mi);

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
    double voltage_L_[3]{NAN, NAN, NAN};

    string device_date_time_;
};

MeterAmiplus::MeterAmiplus(MeterInfo &mi) :
    MeterCommonImplementation(mi, MeterDriver::AMIPLUS)
{
    setExpectedTPLSecurityMode(TPLSecurityMode::AES_CBC_IV);

    addLinkMode(LinkMode::T1);

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

    addPrint("current_power_production", Quantity::Power,
             [&](Unit u){ return currentPowerProduction(u); },
             "Current power production.",
             true, true);

    addPrint("voltage_at_phase_1", Quantity::Voltage,
	     [&](Unit u){ return convert(voltage_L_[0], Unit::Volt, u); },
	     "Voltage at phase L1.",
	     true, true);

    addPrint("voltage_at_phase_2", Quantity::Voltage,
	     [&](Unit u){ return convert(voltage_L_[1], Unit::Volt, u); },
	     "Voltage at phase L2.",
	     true, true);

    addPrint("voltage_at_phase_3", Quantity::Voltage,
	     [&](Unit u){ return convert(voltage_L_[2], Unit::Volt, u); },
	     "Voltage at phase L3.",
	     true, true);

    addPrint("device_date_time", Quantity::Text,
             [&](){ return device_date_time_; },
             "Device date time.",
             false, true);
}

shared_ptr<ElectricityMeter> createAmiplus(MeterInfo &mi)
{
    return shared_ptr<ElectricityMeter>(new MeterAmiplus(mi));
}

double MeterAmiplus::totalEnergyConsumption(Unit u)
{
    assertQuantity(u, Quantity::Energy);
    return convert(total_energy_kwh_, Unit::KWH, u);
}

double MeterAmiplus::currentPowerConsumption(Unit u)
{
    assertQuantity(u, Quantity::Power);
    return convert(current_power_kw_, Unit::KW, u);
}

double MeterAmiplus::totalEnergyProduction(Unit u)
{
    assertQuantity(u, Quantity::Energy);
    return convert(total_energy_returned_kwh_, Unit::KWH, u);
}

double MeterAmiplus::currentPowerProduction(Unit u)
{
    assertQuantity(u, Quantity::Power);
    return convert(current_power_returned_kw_, Unit::KW, u);
}

void MeterAmiplus::processContent(Telegram *t)
{
    int offset;
    string key;

    if (findKey(MeasurementType::Unknown, ValueInformation::EnergyWh, 0, 0, &key, &t->values)) {
        extractDVdouble(&t->values, key, &offset, &total_energy_kwh_);
        t->addMoreExplanation(offset, " total energy (%f kwh)", total_energy_kwh_);
    }

    if (findKey(MeasurementType::Unknown, ValueInformation::PowerW, 0, 0, &key, &t->values)) {
        extractDVdouble(&t->values, key, &offset, &current_power_kw_);
        t->addMoreExplanation(offset, " current power (%f kw)", current_power_kw_);
    }

    extractDVdouble(&t->values, "0E833C", &offset, &total_energy_returned_kwh_);
    t->addMoreExplanation(offset, " total energy returned (%f kwh)", total_energy_returned_kwh_);

    extractDVdouble(&t->values, "0BAB3C", &offset, &current_power_returned_kw_);
    t->addMoreExplanation(offset, " current power returned (%f kw)", current_power_returned_kw_);

    voltage_L_[0]=voltage_L_[1]=voltage_L_[2] = NAN;
    uint64_t tmpvolt {};

    if (extractDVlong(&t->values, "0AFDC9FC01", &offset, &tmpvolt))
    {
    voltage_L_[0] = ((double)tmpvolt);
    t->addMoreExplanation(offset, " voltage L1 (%f volts)", voltage_L_[0]);
    }

    if (extractDVlong(&t->values, "0AFDC9FC02", &offset, &tmpvolt))
    {
    voltage_L_[1] = ((double)tmpvolt);
    t->addMoreExplanation(offset, " voltage L2 (%f volts)", voltage_L_[1]);
    }

    if (extractDVlong(&t->values, "0AFDC9FC03", &offset, &tmpvolt))
    {
    voltage_L_[2] = ((double)tmpvolt);
    t->addMoreExplanation(offset, " voltage L3 (%f volts)", voltage_L_[2]);
    }
        

    if (findKey(MeasurementType::Unknown, ValueInformation::DateTime, 0, 0, &key, &t->values)) {
        struct tm datetime;
        extractDVdate(&t->values, key, &offset, &datetime);
        device_date_time_ = strdatetime(&datetime);
        t->addMoreExplanation(offset, " device datetime (%s)", device_date_time_.c_str());
    }
}
