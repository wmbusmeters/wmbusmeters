/*
 Copyright (C) 2020 Fredrik Öhrström

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

/* This is an S1 meter that we do not fully understand yet.
   Perhaps we need to send a message to it to acquire the full readout?
*/
struct MeterWhe46x : public virtual HeatCostAllocationMeter, public virtual MeterCommonImplementation {
    MeterWhe46x(MeterInfo &mi);

    double currentConsumption(Unit u);
    string setDate();
    double consumptionAtSetDate(Unit u);

private:

    void processContent(Telegram *t);
    double flowTemperature(Unit u);

    double current_consumption_hca_ {};
    string set_date_;
    double consumption_at_set_date_hca_ {};
    string set_date_17_;
    double consumption_at_set_date_17_hca_ {};
    string error_date_;
    double flow_temperature_c_ { 127 };
    uchar  listening_window_management_data_type_L_ {};
    string device_date_time_;
    // Temporarily store the vendor data here.
    string vendor_data_;
};

MeterWhe46x::MeterWhe46x(MeterInfo &mi) :
    MeterCommonImplementation(mi, MeterDriver::WHE46X)
{
    setExpectedTPLSecurityMode(TPLSecurityMode::AES_CBC_IV);

    addLinkMode(LinkMode::C1);

    addPrint("current_consumption", Quantity::HCA,
             [&](Unit u){ return currentConsumption(u); },
             "The current heat cost allocation.",
             true, true);

    addPrint("set_date", Quantity::Text,
             [&](){ return setDate(); },
             "The most recent billing period date.",
             true, true);

    addPrint("consumption_at_set_date", Quantity::HCA,
             [&](Unit u){ return consumptionAtSetDate(u); },
             "Heat cost allocation at the most recent billing period date.",
             true, true);

    addPrint("set_date_1", Quantity::Text,
             [&](){ return setDate(); },
             "The 1 billing period date.",
             false, true);

    addPrint("consumption_at_set_date_1", Quantity::HCA,
             [&](Unit u){ return consumptionAtSetDate(u); },
             "Heat cost allocation at the 1 billing period date.",
             false, true);

    addPrint("flow_temperature", Quantity::Temperature,
             [&](Unit u){ return flowTemperature(u); },
             "The water temperature.",
             true, true);

    addPrint("error_date", Quantity::Text,
             [&](){ return error_date_; },
             "Error date.",
             false, true);

    addPrint("device_date_time", Quantity::Text,
             [&](){ return device_date_time_; },
             "Device date time.",
             false, true);

    addPrint("unknown", Quantity::Text,
             [&](){ return vendor_data_; },
             "Not yet understood information.",
             true, true);

}

shared_ptr<Meter> createWhe46x(MeterInfo &mi)
{
    return shared_ptr<HeatCostAllocationMeter>(new MeterWhe46x(mi));
}

double MeterWhe46x::currentConsumption(Unit u)
{
    return current_consumption_hca_;
}

string MeterWhe46x::setDate()
{
    return set_date_;
}

double MeterWhe46x::flowTemperature(Unit u)
{
    assertQuantity(u, Quantity::Temperature);
    return convert(flow_temperature_c_, Unit::C, u);
}

double MeterWhe46x::consumptionAtSetDate(Unit u)
{
    return consumption_at_set_date_hca_;
}

void MeterWhe46x::processContent(Telegram *t)
{
    /*
      (whe46x) 0f: 04 dif (32 Bit Integer/Binary Instantaneous value)
      (whe46x) 10: 6D vif (Date and time type)
      (whe46x) 11: * 1311962C device datetime (2020-12-22 17:19)
      (whe46x) 15: 01 dif (8 Bit Integer/Binary Instantaneous value)
      (whe46x) 16: FD vif (Second extension of VIF-codes)
      (whe46x) 17: 0C vife (Model/Version)
      (whe46x) 18: 03
      (whe46x) 19: 32 dif (16 Bit Integer/Binary Value during error state)
      (whe46x) 1a: 6C vif (Date type G)
      (whe46x) 1b: * FFFF error date (2127-15-31)
      (whe46x) 1d: 01 dif (8 Bit Integer/Binary Instantaneous value)
      (whe46x) 1e: FD vif (Second extension of VIF-codes)
      (whe46x) 1f: 73 vife (Reserved)
      (whe46x) 20: 00
      (whe46x) 21: 02 dif (16 Bit Integer/Binary Instantaneous value)
      (whe46x) 22: 5A vif (Flow temperature 10⁻¹ °C)
      (whe46x) 23: C200
      (whe46x) 25: 0D dif (variable length Instantaneous value)
      (whe46x) 26: FF vif (Vendor extension)
      (whe46x) 27: 5F vife (duration of limit exceed last upper  is 3)
      (whe46x) 28: 0C varlen=12
      (whe46x) 29: * 0008003030810613080BFFFC vendor extension data
    */

    int offset;
    string key;

    // This heat cost allocator cannot even be bothered to send the HCA data according
    // to the wmbus protocol....Blech..... I suppose the HCA data is hidden
    // in the variable string vendor string at the end. Sigh.
    if (findKey(MeasurementType::Unknown, ValueInformation::HeatCostAllocation, 0, 0, &key, &t->values)) {
        extractDVdouble(&t->values, key, &offset, &current_consumption_hca_);
        t->addMoreExplanation(offset, " current consumption (%f hca)", current_consumption_hca_);
    }

    if (findKey(MeasurementType::Unknown, ValueInformation::Date, 1, 0, &key, &t->values)) {
        struct tm date;
        extractDVdate(&t->values, key, &offset, &date);
        set_date_ = strdate(&date);
        t->addMoreExplanation(offset, " set date (%s)", set_date_.c_str());
    }

    if (findKey(MeasurementType::Unknown, ValueInformation::HeatCostAllocation, 1, 0, &key, &t->values)) {
        extractDVdouble(&t->values, key, &offset, &consumption_at_set_date_hca_);
        t->addMoreExplanation(offset, " consumption at set date (%f hca)", consumption_at_set_date_hca_);
    }

    if (findKey(MeasurementType::Unknown, ValueInformation::HeatCostAllocation, 17, 0, &key, &t->values)) {
        extractDVdouble(&t->values, key, &offset, &consumption_at_set_date_17_hca_);
        t->addMoreExplanation(offset, " consumption at set date 17 (%f hca)", consumption_at_set_date_17_hca_);
    }

    if (findKey(MeasurementType::Unknown, ValueInformation::Date, 17, 0, &key, &t->values)) {
        struct tm date;
        extractDVdate(&t->values, key, &offset, &date);
        set_date_17_ = strdate(&date);
        t->addMoreExplanation(offset, " set date 17 (%s)", set_date_17_.c_str());
    }

    key = "326C";
    if (hasKey(&t->values, key)) {
        struct tm date;
        extractDVdate(&t->values, key, &offset, &date);
        error_date_ = strdate(&date);
        t->addMoreExplanation(offset, " error date (%s)", error_date_.c_str());
    }

    if (findKey(MeasurementType::Unknown, ValueInformation::DateTime, 0, 0, &key, &t->values)) {
        struct tm datetime;
        extractDVdate(&t->values, key, &offset, &datetime);
        device_date_time_ = strdatetime(&datetime);
        t->addMoreExplanation(offset, " device datetime (%s)", device_date_time_.c_str());
    }

    if(findKey(MeasurementType::Instantaneous, ValueInformation::FlowTemperature, 0, 0, &key, &t->values)) {
        extractDVdouble(&t->values, key, &offset, &flow_temperature_c_);
        t->addMoreExplanation(offset, " flow temperature (%f °C)", flow_temperature_c_);
    }

    key = "0DFF5F";
    if (hasKey(&t->values, key)) {
        extractDVstring(&t->values, key, &offset, &vendor_data_);
        t->addMoreExplanation(offset, " vendor extension data");
    }

    key = "01FD73";
    if (hasKey(&t->values, key)) {
        extractDVuint8(&t->values, key, &offset, &listening_window_management_data_type_L_);
        t->addMoreExplanation(offset, " listening window management data type L (%d)", listening_window_management_data_type_L_);
    }

}
