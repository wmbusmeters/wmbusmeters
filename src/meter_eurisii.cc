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

    string errorFlagsHumanReadable();

    double current_consumption_hca_ {};
    double consumption_at_set_date_hca_[17]; // 1 to 17 store in index 0 to 16
    uint16_t error_flags_;
};

MeterEurisII::MeterEurisII(WMBus *bus, MeterInfo &mi) :
    MeterCommonImplementation(bus, mi, MeterType::EURISII, MANUFACTURER_INE, LinkMode::T1)
{
    setEncryptionMode(EncryptionMode::AES_CBC);

    addMedia(0x08);

    setExpectedVersion(0x55);

    addPrint("current_consumption", Quantity::HCA,
             [&](Unit u){ return currentConsumption(u); },
             "The current heat cost allocation.",
             true, true);

    addPrint("consumption_at_set_date", Quantity::HCA,
             [&](Unit u){ return consumption_at_set_date_hca_[0]; },
             "Heat cost allocation at the 1 billing period date.",
             false, true);

    for (int i=2; i<=17; ++i)
    {
        string msg, info;
        strprintf(msg, "consumption_at_set_date_%d", i);
        strprintf(info, "Heat cost allocation at the %d billing period date.", i);
        addPrint(msg, Quantity::HCA,
                 [this,i](Unit u){ return consumption_at_set_date_hca_[i-1]; },
                 info,
                 false, true);
    }

    addPrint("error_flags", Quantity::Text,
             [&](){ return errorFlagsHumanReadable(); },
             "Error flags of meter.",
             true, true);

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

string MeterEurisII::errorFlagsHumanReadable()
{
    string s;
    strprintf(s, "0x%04X", error_flags_);
    return s;
}

void MeterEurisII::processContent(Telegram *t)
{
    map<string,pair<int,DVEntry>> values;
    parseDV(t, t->content, t->content.begin(), t->content.size(), &values);

    int offset;
    string key;

    if (findKey(ValueInformation::HeatCostAllocation, 0, &key, &values))
    {
        extractDVdouble(&values, key, &offset, &current_consumption_hca_);
        t->addMoreExplanation(offset, " current consumption (%f hca)", current_consumption_hca_);
    }

    for (int i=1; i<=17; ++i)
    {
        if (findKey(ValueInformation::HeatCostAllocation, i, &key, &values))
        {
            string info;
            strprintf(info, " consumption at set date %d (%%f hca)", i);
            extractDVdouble(&values, key, &offset, &consumption_at_set_date_hca_[i-1]);
            t->addMoreExplanation(offset, info.c_str(), consumption_at_set_date_hca_[i-1]);
        }
    }

    key = "02FD17";
    if (hasKey(&values, key)) {
        extractDVuint16(&values, "02FD17", &offset, &error_flags_);
        t->addMoreExplanation(offset, " error flags (%04X)", error_flags_);
        // This is not stored anywhere yet, we need to understand it, if necessary.
    }
}
