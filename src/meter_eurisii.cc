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
    string set_date_; // The set date for index 0 below, is only sent for short telegrams.
    // Ie for long telegrams with 17 values, then the set_date_ is empty.
    double consumption_at_set_date_hca_[17]; // 1 to 17 store in index 0 to 16
    uint16_t error_flags_;
};

MeterEurisII::MeterEurisII(WMBus *bus, MeterInfo &mi) :
    MeterCommonImplementation(bus, mi, MeterType::EURISII, MANUFACTURER_INE)
{
    setExpectedTPLSecurityMode(TPLSecurityMode::AES_CBC_IV);

    addMedia(0x08);

    addLinkMode(LinkMode::T1);

    setExpectedVersion(0x55);

    addPrint("current_consumption", Quantity::HCA,
             [&](Unit u){ return currentConsumption(u); },
             "The current heat cost allocation.",
             true, true);

    addPrint("set_date", Quantity::Text,
             [&](){ return setDate(); },
             "The most recent billing period date.",
             true, true);

    addPrint("consumption_at_set_date", Quantity::HCA,
             [&](Unit u){ return consumption_at_set_date_hca_[0]; },
             "Heat cost allocation at the most recent billing period date.",
             false, true);

    for (int i=1; i<=17; ++i)
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
             "Error flags.",
             true, true);
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
    return set_date_;
}

double MeterEurisII::consumptionAtSetDate(Unit u)
{
    return consumption_at_set_date_hca_[0];
}

string MeterEurisII::errorFlagsHumanReadable()
{
    string s;
    if (error_flags_ & 0x01) s += "MEASUREMENT ";
    if (error_flags_ & 0x02) s += "SABOTAGE ";
    if (error_flags_ & 0x04) s += "BATTERY ";
    if (error_flags_ & 0x08) s += "CS ";
    if (error_flags_ & 0x10) s += "HF ";
    if (error_flags_ & 0x20) s += "RESET ";

    if (s.length() > 0) {
        s.pop_back();
        return s;
    }

    if (error_flags_ != 0 && s.length() == 0) {
        // Some higher bits are set that we do not know about! Fall back to printing the number!
        strprintf(s, "0x%04X", error_flags_);
    }
    return s;
}

void MeterEurisII::processContent(Telegram *t)
{
    // These meters can be configured to send long telegrams 18 measurement values, no date.
    // or short telegrams 2 measurement values, with a date.

    // Here is a long telegram:
    // (eurisii) 0f: 2F skip
    // (eurisii) 10: 2F skip
    // (eurisii) 11: 0B dif (6 digit BCD Instantaneous value)
    // (eurisii) 12: 6E vif (Units for H.C.A.)
    // (eurisii) 13: * 112233 current consumption (332211.000000 hca)
    // (eurisii) 16: 42 dif (16 Bit Integer/Binary Instantaneous value storagenr=1)
    // (eurisii) 17: 6E vif (Units for H.C.A.)
    // (eurisii) 18: * 0000 consumption at set date 1 (0.000000 hca)
    // (eurisii) 1a: 82 dif (16 Bit Integer/Binary Instantaneous value)
    // (eurisii) 1b: 01 dife (subunit=0 tariff=0 storagenr=2)
    // (eurisii) 1c: 6E vif (Units for H.C.A.)
    // (eurisii) 1d: * 0000 consumption at set date 2 (0.000000 hca)
    // (eurisii) 1f: C2 dif (16 Bit Integer/Binary Instantaneous value storagenr=1)
    // (eurisii) 20: 01 dife (subunit=0 tariff=0 storagenr=3)
    // (eurisii) 21: 6E vif (Units for H.C.A.)
    // (eurisii) 22: * 0000 consumption at set date 3 (0.000000 hca)
    // (eurisii) 24: 82 dif (16 Bit Integer/Binary Instantaneous value)
    // (eurisii) 25: 02 dife (subunit=0 tariff=0 storagenr=4)
    // (eurisii) 26: 6E vif (Units for H.C.A.)
    // (eurisii) 27: * 0000 consumption at set date 4 (0.000000 hca)
    // (eurisii) 29: C2 dif (16 Bit Integer/Binary Instantaneous value storagenr=1)
    // (eurisii) 2a: 02 dife (subunit=0 tariff=0 storagenr=5)
    // (eurisii) 2b: 6E vif (Units for H.C.A.)
    // (eurisii) 2c: * 0000 consumption at set date 5 (0.000000 hca)
    // (eurisii) 2e: 82 dif (16 Bit Integer/Binary Instantaneous value)
    // (eurisii) 2f: 03 dife (subunit=0 tariff=0 storagenr=6)
    // (eurisii) 30: 6E vif (Units for H.C.A.)
    // (eurisii) 31: * 0000 consumption at set date 6 (0.000000 hca)
    // (eurisii) 33: C2 dif (16 Bit Integer/Binary Instantaneous value storagenr=1)
    // (eurisii) 34: 03 dife (subunit=0 tariff=0 storagenr=7)
    // (eurisii) 35: 6E vif (Units for H.C.A.)
    // (eurisii) 36: * 0000 consumption at set date 7 (0.000000 hca)
    // (eurisii) 38: 82 dif (16 Bit Integer/Binary Instantaneous value)
    // (eurisii) 39: 04 dife (subunit=0 tariff=0 storagenr=8)
    // (eurisii) 3a: 6E vif (Units for H.C.A.)
    // (eurisii) 3b: * 0000 consumption at set date 8 (0.000000 hca)
    // (eurisii) 3d: C2 dif (16 Bit Integer/Binary Instantaneous value storagenr=1)
    // (eurisii) 3e: 04 dife (subunit=0 tariff=0 storagenr=9)
    // (eurisii) 3f: 6E vif (Units for H.C.A.)
    // (eurisii) 40: * 0000 consumption at set date 9 (0.000000 hca)
    // (eurisii) 42: 82 dif (16 Bit Integer/Binary Instantaneous value)
    // (eurisii) 43: 05 dife (subunit=0 tariff=0 storagenr=10)
    // (eurisii) 44: 6E vif (Units for H.C.A.)
    // (eurisii) 45: * 0000 consumption at set date 10 (0.000000 hca)
    // (eurisii) 47: C2 dif (16 Bit Integer/Binary Instantaneous value storagenr=1)
    // (eurisii) 48: 05 dife (subunit=0 tariff=0 storagenr=11)
    // (eurisii) 49: 6E vif (Units for H.C.A.)
    // (eurisii) 4a: * 0000 consumption at set date 11 (0.000000 hca)
    // (eurisii) 4c: 82 dif (16 Bit Integer/Binary Instantaneous value)
    // (eurisii) 4d: 06 dife (subunit=0 tariff=0 storagenr=12)
    // (eurisii) 4e: 6E vif (Units for H.C.A.)
    // (eurisii) 4f: * 0000 consumption at set date 12 (0.000000 hca)
    // (eurisii) 51: C2 dif (16 Bit Integer/Binary Instantaneous value storagenr=1)
    // (eurisii) 52: 06 dife (subunit=0 tariff=0 storagenr=13)
    // (eurisii) 53: 6E vif (Units for H.C.A.)
    // (eurisii) 54: * 0000 consumption at set date 13 (0.000000 hca)
    // (eurisii) 56: 82 dif (16 Bit Integer/Binary Instantaneous value)
    // (eurisii) 57: 07 dife (subunit=0 tariff=0 storagenr=14)
    // (eurisii) 58: 6E vif (Units for H.C.A.)
    // (eurisii) 59: * 0000 consumption at set date 14 (0.000000 hca)
    // (eurisii) 5b: C2 dif (16 Bit Integer/Binary Instantaneous value storagenr=1)
    // (eurisii) 5c: 07 dife (subunit=0 tariff=0 storagenr=15)
    // (eurisii) 5d: 6E vif (Units for H.C.A.)
    // (eurisii) 5e: * 0000 consumption at set date 15 (0.000000 hca)
    // (eurisii) 60: 82 dif (16 Bit Integer/Binary Instantaneous value)
    // (eurisii) 61: 08 dife (subunit=0 tariff=0 storagenr=16)
    // (eurisii) 62: 6E vif (Units for H.C.A.)
    // (eurisii) 63: * 0000 consumption at set date 16 (0.000000 hca)
    // (eurisii) 65: C2 dif (16 Bit Integer/Binary Instantaneous value storagenr=1)
    // (eurisii) 66: 08 dife (subunit=0 tariff=0 storagenr=17)
    // (eurisii) 67: 6E vif (Units for H.C.A.)
    // (eurisii) 68: * 0000 consumption at set date 17 (0.000000 hca)
    // (eurisii) 6a: 02 dif (16 Bit Integer/Binary Instantaneous value)
    // (eurisii) 6b: FD vif (Second extension of VIF-codes)
    // (eurisii) 6c: 17 vife (Error flags (binary))
    // (eurisii) 6d: * 0000 error flags (0000)

    int offset;
    string key;

    if (findKey(MeasurementType::Unknown, ValueInformation::HeatCostAllocation, 0, &key, &t->values))
    {
        extractDVdouble(&t->values, key, &offset, &current_consumption_hca_);
        t->addMoreExplanation(offset, " current consumption (%f hca)", current_consumption_hca_);
    }

    if (findKey(MeasurementType::Unknown, ValueInformation::Date, 1, &key, &t->values)) {
        struct tm date;
        extractDVdate(&t->values, key, &offset, &date);
        set_date_ = strdate(&date);
        t->addMoreExplanation(offset, " set date (%s)", set_date_.c_str());
    }

    for (int i=1; i<=17; ++i)
    {
        if (findKey(MeasurementType::Unknown, ValueInformation::HeatCostAllocation, i, &key, &t->values))
        {
            string info;
            strprintf(info, " consumption at set date %d (%%f hca)", i);
            extractDVdouble(&t->values, key, &offset, &consumption_at_set_date_hca_[i-1]);
            t->addMoreExplanation(offset, info.c_str(), consumption_at_set_date_hca_[i-1]);
        }
    }

    key = "02FD17";
    if (hasKey(&t->values, key)) {
        extractDVuint16(&t->values, "02FD17", &offset, &error_flags_);
        t->addMoreExplanation(offset, " error flags (%04X)", error_flags_);
    }
}
