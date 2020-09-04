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

struct MeterQCaloric : public virtual HeatCostMeter, public virtual MeterCommonImplementation {
    MeterQCaloric(WMBus *bus, MeterInfo &mi);

    double currentConsumption(Unit u);
    string setDate();
    double consumptionAtSetDate(Unit u);

private:

    void processContent(Telegram *t);

    // Telegram type 1
    double current_consumption_hca_ {};
    string set_date_;
    double consumption_at_set_date_hca_ {};
    string set_date_17_;
    double consumption_at_set_date_17_hca_ {};
    string error_date_;

    // Telegram type 2
    string vendor_proprietary_data_;
    string device_date_time_;
};

MeterQCaloric::MeterQCaloric(WMBus *bus, MeterInfo &mi) :
    MeterCommonImplementation(bus, mi, MeterType::QCALORIC)
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

    addPrint("set_date_17", Quantity::Text,
             [&](){ return set_date_17_; },
             "The 17 billing period date.",
             false, true);

    addPrint("consumption_at_set_date_17", Quantity::HCA,
             [&](Unit u){ return consumption_at_set_date_17_hca_; },
             "Heat cost allocation at the 17 billing period date.",
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

unique_ptr<HeatCostMeter> createQCaloric(WMBus *bus, MeterInfo &mi)
{
    return unique_ptr<HeatCostMeter>(new MeterQCaloric(bus, mi));
}

double MeterQCaloric::currentConsumption(Unit u)
{
    return current_consumption_hca_;
}

string MeterQCaloric::setDate()
{
    return set_date_;
}

double MeterQCaloric::consumptionAtSetDate(Unit u)
{
    return consumption_at_set_date_hca_;
}

void MeterQCaloric::processContent(Telegram *t)
{
    // These meter sends two types of telegrams:
    // Type 1:
    // (qcaloric) 0f: 0B dif (6 digit BCD Instantaneous value)
    // (qcaloric) 10: 6E vif (Units for H.C.A.)
    // (qcaloric) 11: 270100 current consumption (127.000000 hca)
    //
    // (qcaloric) 14: 4B dif (6 digit BCD Instantaneous value storagenr=1)
    // (qcaloric) 15: 6E vif (Units for H.C.A.)
    // (qcaloric) 16: 450100 consumption at set date (145.000000 hca)
    //
    // (qcaloric) 19: 42 dif (16 Bit Integer/Binary Instantaneous value storagenr=1)
    // (qcaloric) 1a: 6C vif (Date type G)
    // (qcaloric) 1b: 5F2C set date (2018-12-31) set date (2018-12-31)
    //
    // (qcaloric) 1d: CB dif (6 digit BCD Instantaneous value storagenr=1)
    // (qcaloric) 1e: 08 dife (subunit=0 tariff=0 storagenr=17)
    // (qcaloric) 1f: 6E vif (Units for H.C.A.)
    // (qcaloric) 20: 790000 consumption at set date 17 (79.000000 hca)
    //
    // (qcaloric) 23: C2 dif (16 Bit Integer/Binary Instantaneous value storagenr=1)
    // (qcaloric) 24: 08 dife (subunit=0 tariff=0 storagenr=17)
    // (qcaloric) 25: 6C vif (Date type G)
    // (qcaloric) 26: 7F21 set date 17 (2019-01-31)
    //
    // (qcaloric) 28: 32 dif (16 Bit Integer/Binary Value during error state)
    // (qcaloric) 29: 6C vif (Date type G)
    // (qcaloric) 2a: FFFF error date (2127-15-31)

    // (qcaloric) 2c: 04 dif (32 Bit Integer/Binary Instantaneous value)
    // (qcaloric) 2d: 6D vif (Date and time type)
    // (qcaloric) 2e: 200B7422 device datetime (2019-02-20 11:32)

    // Type 2:
    // (qcaloric) 0b: 0D dif (variable length Instantaneous value)
    // (qcaloric) 0c: FF vif (Vendor extension)
    // (qcaloric) 0d: 5F vife (?)
    // (qcaloric) 0e: varlen=53
    // (qcaloric) 0f: 0082750000810007B06EFFFF270100005F2C450100007F2179000000008000800080008000000000000000000C002E005700BEFF2F
    // (qcaloric) 44: 04 dif (32 Bit Integer/Binary Instantaneous value)
    // (qcaloric) 45: 6D vif (Date and time type)
    // (qcaloric) 46: 2D0B7422 device datetime (2019-02-20 11:45)

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

    key = "0DFF5F";
    if (hasKey(&t->values, key)) {
        string hex;
        extractDVstring(&t->values, key, &offset, &hex);
        t->addMoreExplanation(offset, " vendor extension data");
        // This is not stored anywhere yet, we need to understand it, if necessary.
    }
}
