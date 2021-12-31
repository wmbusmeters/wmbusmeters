/*
 Copyright (C) 2021 Fredrik Öhrström

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

struct MeterQHeat : public virtual HeatMeter, public virtual MeterCommonImplementation {
    MeterQHeat(MeterInfo &mi);

    double totalEnergyConsumption(Unit u);
    double targetEnergyConsumption(Unit u);
    double currentPowerConsumption(Unit u);
    string status();

private:
    void processContent(Telegram *t);

    double total_energy_kwh_ {};

    // This is the measurement at the end of last year. Stored in storage 1.
    string last_year_date_ {};
    double last_year_energy_kwh_ {};

    // For some reason the last month is stored in storage nr 17....woot?
    string last_month_date_ {};
    double last_month_energy_kwh_ {};

    string device_date_time_ {};

    string device_error_date_ {};
};

MeterQHeat::MeterQHeat(MeterInfo &mi) :
    MeterCommonImplementation(mi, MeterDriver::QHEAT)
{
    setExpectedELLSecurityMode(ELLSecurityMode::AES_CTR);

    addLinkMode(LinkMode::C1);

    addPrint("total_energy_consumption", Quantity::Energy,
             [&](Unit u){ return totalEnergyConsumption(u); },
             "The total energy consumption recorded by this meter.",
             true, true);

    addPrint("last_month_date", Quantity::Text,
             [&](){ return last_month_date_; },
             "Last day previous month when total energy consumption was recorded.",
             true, true);

    addPrint("last_month_energy_consumption", Quantity::Energy,
             [&](Unit u){ return targetEnergyConsumption(u); },
             "The total energy consumption recorded at the last day of the previous month.",
             true, true);

    addPrint("las_year_date", Quantity::Text,
             [&](){ return last_year_date_; },
             "Last day previous year when total energy consumption was recorded.",
             false, true);

    addPrint("last_year_energy_consumption", Quantity::Energy,
             [&](Unit u){ assertQuantity(u, Quantity::Energy); return convert(last_year_energy_kwh_, Unit::KWH, u); },
             "The total energy consumption recorded at the last day of the previous year.",
             false, true);

    addPrint("device_date_time", Quantity::Text,
             [&](){ return device_date_time_; },
             "Device date time.",
             false, true);

    addPrint("device_error_date", Quantity::Text,
             [&](){ return device_error_date_; },
             "Device error date.",
             false, true);

}

shared_ptr<Meter> createQHeat(MeterInfo &mi) {
    return shared_ptr<HeatMeter>(new MeterQHeat(mi));
}

double MeterQHeat::totalEnergyConsumption(Unit u)
{
    assertQuantity(u, Quantity::Energy);
    return convert(total_energy_kwh_, Unit::KWH, u);
}

double MeterQHeat::targetEnergyConsumption(Unit u)
{
    assertQuantity(u, Quantity::Energy);
    return convert(last_month_energy_kwh_, Unit::KWH, u);
}

double MeterQHeat::currentPowerConsumption(Unit u)
{
    assertQuantity(u, Quantity::Power);
    return convert(0, Unit::KW, u);
}

void MeterQHeat::processContent(Telegram *t)
{
    /*
      (qheat) 17: 0C dif (8 digit BCD Instantaneous value)
      (qheat) 18: 05 vif (Energy 10² Wh)
      (qheat) 19: * 04390000 total energy consumption (390.400000 kWh)
      (qheat) 1d: 4C dif (8 digit BCD Instantaneous value storagenr=1)
      (qheat) 1e: 05 vif (Energy 10² Wh)
      (qheat) 1f: * 00000000 target energy consumption (0.000000 kWh)
      (qheat) 23: 42 dif (16 Bit Integer/Binary Instantaneous value storagenr=1)
      (qheat) 24: 6C vif (Date type G)
      (qheat) 25: * 9F2C target date (2020-12-31 00:00)
      (qheat) 27: CC dif (8 digit BCD Instantaneous value storagenr=1)
      (qheat) 28: 08 dife (subunit=0 tariff=0 storagenr=17)
      (qheat) 29: 05 vif (Energy 10² Wh)
      (qheat) 2a: 51070000
      (qheat) 2e: C2 dif (16 Bit Integer/Binary Instantaneous value storagenr=1)
      (qheat) 2f: 08 dife (subunit=0 tariff=0 storagenr=17)
      (qheat) 30: 6C vif (Date type G)
      (qheat) 31: BE29
      (qheat) 33: 32 dif (16 Bit Integer/Binary Value during error state)
      (qheat) 34: 6C vif (Date type G)
      (qheat) 35: FFFF
      (qheat) 37: 04 dif (32 Bit Integer/Binary Instantaneous value)
      (qheat) 38: 6D vif (Date and time type)
      (qheat) 39: 280DB62A
    */
    int offset;
    string key;

    if(findKey(MeasurementType::Instantaneous, ValueInformation::EnergyWh, 0, 0, &key, &t->values)) {
        extractDVdouble(&t->values, key, &offset, &total_energy_kwh_);
        t->addMoreExplanation(offset, " total energy consumption (%f kWh)", total_energy_kwh_);
    }

    if(findKey(MeasurementType::Instantaneous, ValueInformation::EnergyWh, 1, 0, &key, &t->values)) {
        extractDVdouble(&t->values, key, &offset, &last_year_energy_kwh_);
        t->addMoreExplanation(offset, " last year energy consumption (%f kWh)", last_year_energy_kwh_);
    }

    if (findKey(MeasurementType::Unknown, ValueInformation::Date, 1, 0, &key, &t->values)) {
        struct tm datetime;
        extractDVdate(&t->values, key, &offset, &datetime);
        last_year_date_ = strdatetime(&datetime);
        t->addMoreExplanation(offset, " last year date (%s)", last_year_date_.c_str());
    }

    if(findKey(MeasurementType::Instantaneous, ValueInformation::EnergyWh, 17, 0, &key, &t->values)) {
        extractDVdouble(&t->values, key, &offset, &last_month_energy_kwh_);
        t->addMoreExplanation(offset, " last month energy consumption (%f kWh)", last_month_energy_kwh_);
    }

    if (findKey(MeasurementType::Unknown, ValueInformation::Date, 17, 0, &key, &t->values)) {
        struct tm datetime;
        extractDVdate(&t->values, key, &offset, &datetime);
        last_month_date_ = strdatetime(&datetime);
        t->addMoreExplanation(offset, " last month date (%s)", last_month_date_.c_str());
    }

    if (findKey(MeasurementType::Unknown, ValueInformation::DateTime, 0, 0, &key, &t->values)) {
            struct tm datetime;
        extractDVdate(&t->values, key, &offset, &datetime);
        device_date_time_ = strdatetime(&datetime);
        t->addMoreExplanation(offset, " device datetime (%s)", device_date_time_.c_str());
    }

    if (findKey(MeasurementType::AtError, ValueInformation::Date, 0, 0, &key, &t->values)) {
        struct tm datetime;
        extractDVdate(&t->values, key, &offset, &datetime);
        device_error_date_ = strdatetime(&datetime);
        t->addMoreExplanation(offset, " device error date (%s)", device_error_date_.c_str());
    }

}

string MeterQHeat::status()
{
    return "";
}
