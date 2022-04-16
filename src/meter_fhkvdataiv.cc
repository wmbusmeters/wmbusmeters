/*
 Copyright (C) 2019-2020 Fredrik Öhrström (gpl-3.0-or-later)

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

struct MeterFHKVDataIV : public virtual MeterCommonImplementation {
    MeterFHKVDataIV(MeterInfo &mi);

    double currentConsumption(Unit u);
    string setDate();
    double consumptionAtSetDate(Unit u);

private:

    void processContent(Telegram *t);

    // Telegram type 1
    double current_consumption_hca_ {};
    string set_date_;
    double consumption_at_set_date_hca_ {};
    string set_date_8_;
    double consumption_at_set_date_8_hca_ {};
    string error_date_;

    // Telegram type 2
    string vendor_proprietary_data_;
    string device_date_time_;
};

MeterFHKVDataIV::MeterFHKVDataIV(MeterInfo &mi) :
    MeterCommonImplementation(mi, "fhkvdataiv")
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

    addPrint("set_date_1", Quantity::Text,
             [&](){ return setDate(); },
             "The 1 billing period date.",
             PrintProperty::JSON);

    addPrint("consumption_at_set_date_1", Quantity::HCA,
             [&](Unit u){ return consumptionAtSetDate(u); },
             "Heat cost allocation at the 1 billing period date.",
             PrintProperty::JSON);

    addPrint("set_date_17", Quantity::Text,
             [&](){ return set_date_8_; },
             "The 8 billing period date.",
             PrintProperty::JSON);

    addPrint("consumption_at_set_date_8", Quantity::HCA,
             [&](Unit u){ return consumption_at_set_date_8_hca_; },
             "Heat cost allocation at the 8 billing period date.",
             PrintProperty::JSON);

    addPrint("error_date", Quantity::Text,
             [&](){ return error_date_; },
             "Error date.",
             PrintProperty::JSON);

    addPrint("device_date_time", Quantity::Text,
             [&](){ return device_date_time_; },
             "Device date time.",
             PrintProperty::JSON);
}

shared_ptr<Meter> createFHKVDataIV(MeterInfo &mi)
{
    return shared_ptr<Meter>(new MeterFHKVDataIV(mi));
}

double MeterFHKVDataIV::currentConsumption(Unit u)
{
    return current_consumption_hca_;
}

string MeterFHKVDataIV::setDate()
{
    return set_date_;
}

double MeterFHKVDataIV::consumptionAtSetDate(Unit u)
{
    return consumption_at_set_date_hca_;
}

void MeterFHKVDataIV::processContent(Telegram *t)
{
    /*
      (fhkvdataiv) 0f: 2f2f decrypt check bytes
      (fhkvdataiv) 11: 03 dif (24 Bit Integer/Binary Instantaneous value)
      (fhkvdataiv) 12: 6E vif (Units for H.C.A.)
      (fhkvdataiv) 13: * 020000 current consumption (2.000000 hca)
      (fhkvdataiv) 16: 43 dif (24 Bit Integer/Binary Instantaneous value storagenr=1)
      (fhkvdataiv) 17: 6E vif (Units for H.C.A.)
      (fhkvdataiv) 18: * 190000 consumption at set date (25.000000 hca)
      (fhkvdataiv) 1b: 42 dif (16 Bit Integer/Binary Instantaneous value storagenr=1)
      (fhkvdataiv) 1c: 6C vif (Date type G)
      (fhkvdataiv) 1d: * 9F2C set date (2020-12-31)
      (fhkvdataiv) 1f: 83 dif (24 Bit Integer/Binary Instantaneous value)
      (fhkvdataiv) 20: 04 dife (subunit=0 tariff=0 storagenr=8)
      (fhkvdataiv) 21: 6E vif (Units for H.C.A.)
      (fhkvdataiv) 22: * 000000 consumption at set date 8 (0.000000 hca)
      (fhkvdataiv) 25: 82 dif (16 Bit Integer/Binary Instantaneous value)
      (fhkvdataiv) 26: 04 dife (subunit=0 tariff=0 storagenr=8)
      (fhkvdataiv) 27: 6C vif (Date type G)
      (fhkvdataiv) 28: * 7F2A set date 8 (2019-10-31)
      (fhkvdataiv) 2a: 8D dif (variable length Instantaneous value)
      (fhkvdataiv) 2b: 04 dife (subunit=0 tariff=0 storagenr=8)
      (fhkvdataiv) 2c: EE vif (Units for H.C.A.)
      (fhkvdataiv) 2d: 1F vife (Compact profile without register)
      (fhkvdataiv) 2e: 1E varlen=30
      (fhkvdataiv) 2f: 72FE00000000000000000000000000000000000000000000000003001600
      (fhkvdataiv) 4d: 2F skip
      (fhkvdataiv) 4e: 2F skip
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

    if (findKey(MeasurementType::Unknown, VIFRange::HeatCostAllocation, 8, 0, &key, &t->values)) {
        extractDVdouble(&t->values, key, &offset, &consumption_at_set_date_8_hca_);
        t->addMoreExplanation(offset, " consumption at set date 8 (%f hca)", consumption_at_set_date_8_hca_);
    }

    if (findKey(MeasurementType::Unknown, VIFRange::Date, 8, 0, &key, &t->values)) {
        struct tm date;
        extractDVdate(&t->values, key, &offset, &date);
        set_date_8_ = strdate(&date);
        t->addMoreExplanation(offset, " set date 8 (%s)", set_date_8_.c_str());
    }

    key = "326C";
    if (hasKey(&t->values, key)) {
        struct tm date;
        extractDVdate(&t->values, key, &offset, &date);
        error_date_ = strdate(&date);
        t->addMoreExplanation(offset, " error date (%s)", error_date_.c_str());
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
