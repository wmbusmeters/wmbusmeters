/*
 Copyright (C) 2019-2021 Fredrik Öhrström

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

struct MeterLSE_08 : public virtual MeterCommonImplementation {
    MeterLSE_08(MeterInfo &mi);

    string setDate();
    double consumptionAtSetDate(Unit u);

private:

    void processContent(Telegram *t);

    double consumption_at_set_date_hca_ {};
    string set_date_;
    string device_date_time_;
    int duration_since_readout_s_ {};
    string software_version_;
};

MeterLSE_08::MeterLSE_08(MeterInfo &mi) :
    MeterCommonImplementation(mi, MeterDriver::LSE_08)
{
    setMeterType(MeterType::HeatCostAllocationMeter);

    setExpectedTPLSecurityMode(TPLSecurityMode::AES_CBC_IV);

    addLinkMode(LinkMode::C1);
    addLinkMode(LinkMode::S1);

    addPrint("set_date", Quantity::Text,
             [&](){ return setDate(); },
             "The most recent billing period date.",
             true, true);

    addPrint("consumption_at_set_date", Quantity::HCA,
             [&](Unit u){ return consumptionAtSetDate(u); },
             "Heat cost allocation at the most recent billing period date.",
             true, true);

    addPrint("device_date_time", Quantity::Text,
             [&](){ return device_date_time_; },
             "Device date time.",
             false, true);

    addPrint("duration_since_readout", Quantity::Time,
             [&](Unit u) {
                 assertQuantity(u, Quantity::Time);
                 return convert(duration_since_readout_s_, Unit::Second, u);
             },
             "Device date time.",
             false, true);

    addPrint("software_version", Quantity::Text,
             [&](){ return software_version_; },
             "Software version.",
             false, true);
}

shared_ptr<Meter> createLSE_08(MeterInfo &mi)
{
    return shared_ptr<Meter>(new MeterLSE_08(mi));
}

string MeterLSE_08::setDate()
{
    return set_date_;
}

double MeterLSE_08::consumptionAtSetDate(Unit u)
{
    assertQuantity(u, Quantity::HCA);
    return convert(consumption_at_set_date_hca_, Unit::HCA, u);
}

void MeterLSE_08::processContent(Telegram *t)
{
    int offset;
    string key;

    if (findKey(MeasurementType::Unknown, ValueInformation::HeatCostAllocation, 8, 0, &key, &t->values)) {
        extractDVdouble(&t->values, key, &offset, &consumption_at_set_date_hca_);
        t->addMoreExplanation(offset, " consumption at set date (%f hca)", consumption_at_set_date_hca_);
    }

    if (findKey(MeasurementType::Unknown, ValueInformation::Date, 8, 0, &key, &t->values)) {
        struct tm date;
        extractDVdate(&t->values, key, &offset, &date);
        set_date_ = strdate(&date);
        t->addMoreExplanation(offset, " set date (%s)", set_date_.c_str());
    }

    if (findKey(MeasurementType::Unknown, ValueInformation::DateTime, 0, 0, &key, &t->values)) {
        struct tm datetime;
        extractDVdate(&t->values, key, &offset, &datetime);
        device_date_time_ = strdatetime(&datetime);
        t->addMoreExplanation(offset, " device datetime (%s)", device_date_time_.c_str());
    }

    uint64_t seconds;
    if (extractDVlong(&t->values, "02FDAC7E", &offset, &seconds))
    {
        duration_since_readout_s_ = (int)seconds;
        t->addMoreExplanation(offset, " duration (%d s)", duration_since_readout_s_);
    }

    uint64_t serial;
    if (extractDVlong(&t->values, "01FD0C", &offset, &serial))
    {
        // 01 --> 01
        software_version_ = to_string(serial);
        t->addMoreExplanation(offset, " software version (%s)", software_version_.c_str());
    }

}
