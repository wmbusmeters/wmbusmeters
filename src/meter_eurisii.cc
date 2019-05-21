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

struct MeterEurisII : public virtual HeatCostMeter, public virtual MeterCommonImplementation {
    MeterEurisII(WMBus *bus, MeterInfo &mi);

    double currentConsumption(Unit u);
    string setDate();
    double consumptionAtSetDate(Unit u);

private:

    void processContent(Telegram *t);

    // Telegram type 1
    double current_consumption_hca_ {};
    double consumption_at_set_date_hca_ {};
    double consumption_at_set_date_17_hca_ {};
    uint16_t error_flags_;

    // Telegram type 2
    string vendor_proprietary_data_;
    string device_date_time_;
};

MeterEurisII::MeterEurisII(WMBus *bus, MeterInfo &mi) :
    MeterCommonImplementation(bus, mi, MeterType::EURISII, MANUFACTURER_INE, LinkMode::T1)
{
    setEncryptionMode(EncryptionMode::AES_CBC); // Is it?

    addMedia(0x08);

    setExpectedVersion(0x55);

    addPrint("current_consumption", Quantity::HCA,
             [&](Unit u){ return currentConsumption(u); },
             "The current heat cost allocation.",
             true, true);

    addPrint("consumption_at_set_date_1", Quantity::HCA,
             [&](Unit u){ return consumption_at_set_date_hca_; },
             "Heat cost allocation at the 1 billing period date.",
             false, true);

    addPrint("consumption_at_set_date_17", Quantity::HCA,
             [&](Unit u){ return consumption_at_set_date_17_hca_; },
             "Heat cost allocation at the 17 billing period date.",
             false, true);

    MeterCommonImplementation::bus()->onTelegram(calll(this,handleTelegram,Telegram*));
}

unique_ptr<HeatCostMeter> createEurisII(WMBus *bus, MeterInfo &mi)
{
    return unique_ptr<HeatCostMeter>(new MeterEurisII(bus, mi));
}

double MeterEurisII::currentConsumption(Unit u)
{
    return current_consumption_hca_;
}

string MeterEurisII::setDate()
{
    return "";
}

double MeterEurisII::consumptionAtSetDate(Unit u)
{
    return 0.0;
}

void MeterEurisII::processContent(Telegram *t)
{
    map<string,pair<int,DVEntry>> values;
    parseDV(t, t->content, t->content.begin(), t->content.size(), &values);

    int offset;
    string key;

    if (findKey(ValueInformation::HeatCostAllocation, 0, &key, &values)) {
        extractDVdouble(&values, key, &offset, &current_consumption_hca_);
        t->addMoreExplanation(offset, " current consumption (%f hca)", current_consumption_hca_);
    }

    if (findKey(ValueInformation::HeatCostAllocation, 1, &key, &values)) {
        extractDVdouble(&values, key, &offset, &consumption_at_set_date_hca_);
        t->addMoreExplanation(offset, " consumption at set date (%f hca)", consumption_at_set_date_hca_);
    }

    if (findKey(ValueInformation::HeatCostAllocation, 17, &key, &values)) {
        extractDVdouble(&values, key, &offset, &consumption_at_set_date_17_hca_);
        t->addMoreExplanation(offset, " consumption at set date 17 (%f hca)", consumption_at_set_date_17_hca_);
    }

    key = "02FD17";
    if (hasKey(&values, key)) {
        extractDVuint16(&values, "02FD17", &offset, &error_flags_);
        t->addMoreExplanation(offset, " error flags (%04X)", error_flags_);
        // This is not stored anywhere yet, we need to understand it, if necessary.
    }
}
