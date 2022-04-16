/*
 Copyright (C) 2020 Fredrik Öhrström (gpl-3.0-or-later)

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

struct MeterSontex868 : public virtual MeterCommonImplementation {
    MeterSontex868(MeterInfo &mi);

    double currentConsumption(Unit u);
    string setDate();
    double consumptionAtSetDate(Unit u);

    double currentTemp(Unit u);
    double currentRoomTemp(Unit u);

    double maxTemp(Unit u);
    double maxTempPreviousPeriod(Unit u);

private:

    void processContent(Telegram *t);

    // Telegram type 1
    double current_consumption_hca_ {};
    string set_date_;
    double consumption_at_set_date_hca_ {};
    string set_date_17_;

    double curr_temp_c_ {};
    double curr_room_temp_c_ {};

    double max_temp_c_ {};
    double max_temp_previous_period_c_ {};

    string device_date_time_;
};

MeterSontex868::MeterSontex868(MeterInfo &mi) :
    MeterCommonImplementation(mi, "sontex868")
{
    setMeterType(MeterType::HeatCostAllocationMeter);

    setExpectedTPLSecurityMode(TPLSecurityMode::AES_CBC_IV);

    addLinkMode(LinkMode::C1);

    addPrint("current_consumption", Quantity::HCA,
             [&](Unit u){ return currentConsumption(u); },
             "The current heat cost allocation.",
             PrintProperty::FIELD | PrintProperty::JSON);

    addPrint("set_date", Quantity::Text,
             [&](){ return setDate(); },
             "The most recent billing period date.",
             PrintProperty::FIELD | PrintProperty::JSON);

    addPrint("consumption_at_set_date", Quantity::HCA,
             [&](Unit u){ return consumptionAtSetDate(u); },
             "Heat cost allocation at the most recent billing period date.",
             PrintProperty::FIELD | PrintProperty::JSON);

    addPrint("current_temp", Quantity::Temperature,
             [&](Unit u){ return currentTemp(u); },
             "The current temperature of the heating element.",
             PrintProperty::JSON);

    addPrint("current_room_temp", Quantity::Temperature,
             [&](Unit u){ return currentRoomTemp(u); },
             "The current room temperature.",
             PrintProperty::JSON);

    addPrint("max_temp", Quantity::Temperature,
             [&](Unit u){ return maxTemp(u); },
             "The maximum temperature so far during this billing period.",
             PrintProperty::JSON);

    addPrint("max_temp_previous_period", Quantity::Temperature,
             [&](Unit u){ return maxTempPreviousPeriod(u); },
             "The maximum temperature during the previous billing period.",
             PrintProperty::JSON);

    addPrint("device_date_time", Quantity::Text,
             [&](){ return device_date_time_; },
             "Device date time.",
             PrintProperty::JSON);
}

shared_ptr<Meter> createSontex868(MeterInfo &mi)
{
    return shared_ptr<Meter>(new MeterSontex868(mi));
}

double MeterSontex868::currentConsumption(Unit u)
{
    return current_consumption_hca_;
}

string MeterSontex868::setDate()
{
    return set_date_;
}

double MeterSontex868::consumptionAtSetDate(Unit u)
{
    return consumption_at_set_date_hca_;
}

double MeterSontex868::currentTemp(Unit u)
{
    assertQuantity(u, Quantity::Temperature);
    return convert(curr_temp_c_, Unit::C, u);
}

double MeterSontex868::currentRoomTemp(Unit u)
{
    assertQuantity(u, Quantity::Temperature);
    return convert(curr_room_temp_c_, Unit::C, u);
}

double MeterSontex868::maxTemp(Unit u)
{
    assertQuantity(u, Quantity::Temperature);
    return convert(max_temp_c_, Unit::C, u);
}

double MeterSontex868::maxTempPreviousPeriod(Unit u)
{
    assertQuantity(u, Quantity::Temperature);
    return convert(max_temp_previous_period_c_, Unit::C, u);
}

void MeterSontex868::processContent(Telegram *t)
{
    /*
      (sontex868) 0f: 04 dif (32 Bit Integer/Binary Instantaneous value)
      (sontex868) 10: 6D vif (Date and time type)
      (sontex868) 11: * 040A9F2A device datetime (2020-10-31 10:04)
      (sontex868) 15: 03 dif (24 Bit Integer/Binary Instantaneous value)
      (sontex868) 16: 6E vif (Units for H.C.A.)
      (sontex868) 17: * 000000 current consumption (0.000000 hca)
      (sontex868) 1a: 42 dif (16 Bit Integer/Binary Instantaneous value storagenr=1)
      (sontex868) 1b: 6C vif (Date type G)
      (sontex868) 1c: * E1F7 set date (2127-07-01)
      (sontex868) 1e: 43 dif (24 Bit Integer/Binary Instantaneous value storagenr=1)
      (sontex868) 1f: 6E vif (Units for H.C.A.)
      (sontex868) 20: * 000000 consumption at set date (0.000000 hca)
      (sontex868) 23: 52 dif (16 Bit Integer/Binary Maximum value storagenr=1)
      (sontex868) 24: 59 vif (Flow temperature 10⁻² °C)
      (sontex868) 25: * 0000 max temperature previous period (0.000000 °C)
      (sontex868) 27: 82 dif (16 Bit Integer/Binary Instantaneous value)
      (sontex868) 28: 88 dife (subunit=0 tariff=0 storagenr=16)
      (sontex868) 29: 01 dife (subunit=0 tariff=0 storagenr=48)
      (sontex868) 2a: 6C vif (Date type G)
      (sontex868) 2b: 6125
      (sontex868) 2d: 83 dif (24 Bit Integer/Binary Instantaneous value)
      (sontex868) 2e: 88 dife (subunit=0 tariff=0 storagenr=16)
      (sontex868) 2f: 01 dife (subunit=0 tariff=0 storagenr=48)
      (sontex868) 30: 6E vif (Units for H.C.A.)
      (sontex868) 31: 000000
      (sontex868) 34: 8D dif (variable length Instantaneous value)
      (sontex868) 35: 88 dife (subunit=0 tariff=0 storagenr=16)
      (sontex868) 36: 01 dife (subunit=0 tariff=0 storagenr=48)
      (sontex868) 37: EE vif (Units for H.C.A.)
      (sontex868) 38: 1E vife (Compact profile with register)
      (sontex868) 39: 35 varlen=53
      (sontex868) 3a: 33FE000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000
      (sontex868) 6f: 05 dif (32 Bit Real Instantaneous value)
      (sontex868) 70: FF vif (Vendor extension)
      (sontex868) 71: 2D vife (per m3)
      (sontex868) 72: 0000803F
      (sontex868) 76: 85 dif (32 Bit Real Instantaneous value)
      (sontex868) 77: 20 dife (subunit=0 tariff=2 storagenr=0)
      (sontex868) 78: FF vif (Vendor extension)
      (sontex868) 79: 2D vife (per m3)
      (sontex868) 7a: 0000803F
      (sontex868) 7e: 02 dif (16 Bit Integer/Binary Instantaneous value)
      (sontex868) 7f: 59 vif (Flow temperature 10⁻² °C)
      (sontex868) 80: * AD0A current temperature (27.330000 °C)
      (sontex868) 82: 02 dif (16 Bit Integer/Binary Instantaneous value)
      (sontex868) 83: 65 vif (External temperature 10⁻² °C)
      (sontex868) 84: * D804 current room temperature (12.400000 °C)
      (sontex868) 86: 12 dif (16 Bit Integer/Binary Maximum value)
      (sontex868) 87: 59 vif (Flow temperature 10⁻² °C)
      (sontex868) 88: * AD0A max temperature current period (27.330000 °C)
      (sontex868) 8a: 83 dif (24 Bit Integer/Binary Instantaneous value)
      (sontex868) 8b: 10 dife (subunit=0 tariff=1 storagenr=0)
      (sontex868) 8c: FD vif (Second extension of VIF-codes)
      (sontex868) 8d: 31 vife (Duration of tariff [minute(s)])
      (sontex868) 8e: 000000
      (sontex868) 91: 82 dif (16 Bit Integer/Binary Instantaneous value)
      (sontex868) 92: 10 dife (subunit=0 tariff=1 storagenr=0)
      (sontex868) 93: 6C vif (Date type G)
      (sontex868) 94: 0101
      (sontex868) 96: 81 dif (8 Bit Integer/Binary Instantaneous value)
      (sontex868) 97: 10 dife (subunit=0 tariff=1 storagenr=0)
      (sontex868) 98: FD vif (Second extension of VIF-codes)
      (sontex868) 99: 61 vife (Cumulation counter)
      (sontex868) 9a: 00
      (sontex868) 9b: 82 dif (16 Bit Integer/Binary Instantaneous value)
      (sontex868) 9c: 20 dife (subunit=0 tariff=2 storagenr=0)
      (sontex868) 9d: 6C vif (Date type G)
      (sontex868) 9e: 9F2A
      (sontex868) a0: 0B dif (6 digit BCD Instantaneous value)
      (sontex868) a1: FD vif (Second extension of VIF-codes)
      (sontex868) a2: 0F vife (Software version #)
      (sontex868) a3: 010301
      (sontex868) a6: 02 dif (16 Bit Integer/Binary Instantaneous value)
      (sontex868) a7: FF vif (Vendor extension)
      (sontex868) a8: 2C vife (per litre)
      (sontex868) a9: 0000
      (sontex868) ab: 02 dif (16 Bit Integer/Binary Instantaneous value)
      (sontex868) ac: FD vif (Second extension of VIF-codes)
      (sontex868) ad: 66 vife (State of parameter activation)
      (sontex868) ae: AC08
    */

    int offset;
    string key;

    if (findKey(MeasurementType::Unknown, VIFRange::HeatCostAllocation, 0, 0, &key, &t->values)) {
        extractDVdouble(&t->values, key, &offset, &current_consumption_hca_);
        t->addMoreExplanation(offset, " current consumption (%f hca)", current_consumption_hca_);
    }

    if (findKey(MeasurementType::Unknown, VIFRange::Date, 1, 0, &key, &t->values)) {
        struct tm date;
        extractDVdate(&t->values, key, &offset, &date);
        set_date_ = strdate(&date);
        t->addMoreExplanation(offset, " set date (%s)", set_date_.c_str());
    }

    if (findKey(MeasurementType::Unknown, VIFRange::HeatCostAllocation, 1, 0, &key, &t->values)) {
        extractDVdouble(&t->values, key, &offset, &consumption_at_set_date_hca_);
        t->addMoreExplanation(offset, " consumption at set date (%f hca)", consumption_at_set_date_hca_);
    }

    if(findKey(MeasurementType::Instantaneous, VIFRange::FlowTemperature, 0, 0, &key, &t->values)) {
        extractDVdouble(&t->values, key, &offset, &curr_temp_c_);
        t->addMoreExplanation(offset, " current temperature (%f °C)", curr_temp_c_);
    }

    if(findKey(MeasurementType::Instantaneous, VIFRange::ExternalTemperature, 0, 0, &key, &t->values)) {
        extractDVdouble(&t->values, key, &offset, &curr_room_temp_c_);
        t->addMoreExplanation(offset, " current room temperature (%f °C)", curr_room_temp_c_);
    }

    if(findKey(MeasurementType::Maximum, VIFRange::FlowTemperature, 0, 0, &key, &t->values)) {
        extractDVdouble(&t->values, key, &offset, &max_temp_c_);
        t->addMoreExplanation(offset, " max temperature current period (%f °C)", max_temp_c_);
    }

    if(findKey(MeasurementType::Maximum, VIFRange::FlowTemperature, 1, 0, &key, &t->values)) {
        extractDVdouble(&t->values, key, &offset, &max_temp_previous_period_c_);
        t->addMoreExplanation(offset, " max temperature previous period (%f °C)", max_temp_previous_period_c_);
    }

    if (findKey(MeasurementType::Unknown, VIFRange::DateTime, 0, 0, &key, &t->values)) {
        struct tm datetime;
        extractDVdate(&t->values, key, &offset, &datetime);
        device_date_time_ = strdatetime(&datetime);
        t->addMoreExplanation(offset, " device datetime (%s)", device_date_time_.c_str());
    }

    key = "0DFF5F";
    if (hasKey(&t->values, key)) {
        string hex;
        extractDVHexString(&t->values, key, &offset, &hex);
        t->addMoreExplanation(offset, " vendor extension data");
        // This is not stored anywhere yet, we need to understand it, if necessary.
    }
}
