/*
 Copyright (C) 2019 Fredrik Öhrström

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

struct MeterAmiplus : public virtual ElectricityMeter, public virtual MeterCommonImplementation {
    MeterAmiplus(WMBus *bus, MeterInfo &mi);

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
    string device_date_time_;
};

MeterAmiplus::MeterAmiplus(WMBus *bus, MeterInfo &mi) :
    MeterCommonImplementation(bus, mi, MeterType::AMIPLUS, 0)
{
    setEncryptionMode(EncryptionMode::AES_CBC);

    // This is one manufacturer of Amiplus compatible meters.
    addManufacturer(MANUFACTURER_APA);
    addMedia(0x02); // Electricity meter

    // This is another manufacturer
    addManufacturer(MANUFACTURER_DEV);
    // Oddly, this device has not been configured to send as a electricity meter,
    // but instead a device/media type that is used for gateway or relays or something?
    addMedia(0x37); // Radio converter (meter side)

    addLinkMode(LinkMode::T1);

    setExpectedVersion(0x02);

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

    addPrint("device_date_time", Quantity::Text,
             [&](){ return device_date_time_; },
             "Device date time.",
             false, true);

    MeterCommonImplementation::bus()->onTelegram(calll(this,handleTelegram,Telegram*));
}

unique_ptr<ElectricityMeter> createAmiplus(WMBus *bus, MeterInfo &mi)
{
    return unique_ptr<ElectricityMeter>(new MeterAmiplus(bus, mi));
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
    map<string,pair<int,DVEntry>> values;
    parseDV(t, t->content, t->content.begin(), t->content.size(), &values);

    int offset;
    string key;

    if (findKey(MeasurementType::Unknown, ValueInformation::EnergyWh, 0, &key, &values)) {
        extractDVdouble(&values, key, &offset, &total_energy_kwh_);
        t->addMoreExplanation(offset, " total energy (%f kwh)", total_energy_kwh_);
    }

    if (findKey(MeasurementType::Unknown, ValueInformation::PowerW, 0, &key, &values)) {
        extractDVdouble(&values, key, &offset, &current_power_kw_);
        t->addMoreExplanation(offset, " current power (%f kw)", current_power_kw_);
    }

    extractDVdouble(&values, "0E833C", &offset, &total_energy_returned_kwh_);
    t->addMoreExplanation(offset, " total energy returned (%f kwh)", total_energy_returned_kwh_);

    extractDVdouble(&values, "0BAB3C", &offset, &current_power_returned_kw_);
    t->addMoreExplanation(offset, " current power returned (%f kw)", current_power_returned_kw_);

    if (findKey(MeasurementType::Unknown, ValueInformation::DateTime, 0, &key, &values)) {
        struct tm datetime;
        extractDVdate(&values, key, &offset, &datetime);
        device_date_time_ = strdatetime(&datetime);
        t->addMoreExplanation(offset, " device datetime (%s)", device_date_time_.c_str());
    }
}
