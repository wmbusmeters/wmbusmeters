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

struct MeterWhe5x : public virtual MeterCommonImplementation {
    MeterWhe5x(MeterInfo &mi);

    double currentConsumption(Unit u);
    string setDate();
    double consumptionAtSetDate(Unit u);

private:

    void processContent(Telegram *t);

    // Telegram type 1
    double current_consumption_hca_ {};
    string set_date_;
    double consumption_at_set_date_hca_ {};
    string error_date_;
    string vendor_proprietary_data_;
    string device_date_time_;
};

MeterWhe5x::MeterWhe5x(MeterInfo &mi) :
    MeterCommonImplementation(mi, "whe5x")
{
    setMeterType(MeterType::HeatCostAllocationMeter);

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

    addPrint("error_date", Quantity::Text,
             [&](){ return error_date_; },
             "Error date.",
             false, true);

    addPrint("device_date_time", Quantity::Text,
             [&](){ return device_date_time_; },
             "Device date time.",
             false, true);
}

shared_ptr<Meter> createWhe5x(MeterInfo &mi)
{
    return shared_ptr<Meter>(new MeterWhe5x(mi));
}

double MeterWhe5x::currentConsumption(Unit u)
{
    return current_consumption_hca_;
}

string MeterWhe5x::setDate()
{
    return set_date_;
}

double MeterWhe5x::consumptionAtSetDate(Unit u)
{
    return consumption_at_set_date_hca_;
}

void MeterWhe5x::processContent(Telegram *t)
{

    int offset;
    string key;

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

    if (findKey(MeasurementType::Unknown, ValueInformation::DateTime, 0, 0, &key, &t->values)) {
        struct tm datetime;
        extractDVdate(&t->values, key, &offset, &datetime);
        device_date_time_ = strdatetime(&datetime);
        t->addMoreExplanation(offset, " device datetime (%s)", device_date_time_.c_str());
    }

    key = "326C";
    if (hasKey(&t->values, key)) {
        struct tm date;
        extractDVdate(&t->values, key, &offset, &date);
        error_date_ = strdate(&date);
        t->addMoreExplanation(offset, " error date (%s)", error_date_.c_str());
    }

    key = "0DFF5F";
    if (hasKey(&t->values, key)) {
        string hex;
        extractDVHexString(&t->values, key, &offset, &hex);
        t->addMoreExplanation(offset, " vendor extension data");
        // This is not stored anywhere yet, we need to understand it, if necessary.
    }
}
