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

struct MeterAventiesHCA : public virtual HeatCostAllocationMeter, public virtual MeterCommonImplementation {
    MeterAventiesHCA(MeterInfo &mi);

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

MeterAventiesHCA::MeterAventiesHCA(MeterInfo &mi) :
    MeterCommonImplementation(mi, MeterDriver::AVENTIESHCA)
{
    setExpectedTPLSecurityMode(TPLSecurityMode::AES_CBC_IV);

    addLinkMode(LinkMode::T1);

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

shared_ptr<HeatCostAllocationMeter> createAventiesHCA(MeterInfo &mi)
{
    return shared_ptr<HeatCostAllocationMeter>(new MeterAventiesHCA(mi));
}

double MeterAventiesHCA::currentConsumption(Unit u)
{
    return current_consumption_hca_;
}

string MeterAventiesHCA::setDate()
{
    return set_date_;
}

double MeterAventiesHCA::consumptionAtSetDate(Unit u)
{
    return consumption_at_set_date_hca_[0];
}

string MeterAventiesHCA::errorFlagsHumanReadable()
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

void MeterAventiesHCA::processContent(Telegram *t)
{
    // These meters can be configured to send long telegrams 18 measurement values, no date.
    // or short telegrams 2 measurement values, with a date.

    // Here is a long telegram:
    // (aventieshca) 00: 76 length (118 bytes)
    // (aventieshca) 01: 44 dll-c (from meter SND_NR)
    // (aventieshca) 02: 2104 dll-mfct (AAA)
    // (aventieshca) 04: 34019060 dll-id (60900134)
    // (aventieshca) 08: 55 dll-version
    // (aventieshca) 09: 08 dll-type (Heat Cost Allocator)
    // (aventieshca) 0a: 72 tpl-ci-field (EN 13757-3 Application Layer (long tplh))
    // (aventieshca) 0b: 34019060 tpl-id (60900134)
    // (aventieshca) 0f: 2104 tpl-mfct (AAA)
    // (aventieshca) 11: 55 tpl-version
    // (aventieshca) 12: 08 tpl-type (Heat Cost Allocator)
    // (aventieshca) 13: 05 tpl-acc-field
    // (aventieshca) 14: 00 tpl-sts-field (OK)
    // (aventieshca) 15: 6005 tpl-cfg 0560 (AES_CBC_IV nb=6 cntn=0 ra=0 hc=0 )
    // (aventieshca) 17: 2f2f decrypt check bytes
    // (aventieshca) 19: 0B dif (6 digit BCD Instantaneous value)
    // (aventieshca) 1a: 6E vif (Units for H.C.A.)
    // (aventieshca) 1b: * 911000 current consumption (1091.000000 hca)
    // (aventieshca) 1e: 42 dif (16 Bit Integer/Binary Instantaneous value storagenr=1)
    // (aventieshca) 1f: 6E vif (Units for H.C.A.)
    // (aventieshca) 20: * 4304 consumption at set date 1 (1091.000000 hca)
    // (aventieshca) 22: 82 dif (16 Bit Integer/Binary Instantaneous value)
    // (aventieshca) 23: 01 dife (subunit=0 tariff=0 storagenr=2)
    // (aventieshca) 24: 6E vif (Units for H.C.A.)
    // (aventieshca) 25: * 3C04 consumption at set date 2 (1084.000000 hca)
    // (aventieshca) 27: C2 dif (16 Bit Integer/Binary Instantaneous value storagenr=1)
    // (aventieshca) 28: 01 dife (subunit=0 tariff=0 storagenr=3)
    // (aventieshca) 29: 6E vif (Units for H.C.A.)
    // (aventieshca) 2a: * CC03 consumption at set date 3 (972.000000 hca)
    // (aventieshca) 2c: 82 dif (16 Bit Integer/Binary Instantaneous value)
    // (aventieshca) 2d: 02 dife (subunit=0 tariff=0 storagenr=4)
    // (aventieshca) 2e: 6E vif (Units for H.C.A.)
    // (aventieshca) 2f: * 1E03 consumption at set date 4 (798.000000 hca)
    // (aventieshca) 31: C2 dif (16 Bit Integer/Binary Instantaneous value storagenr=1)
    // (aventieshca) 32: 02 dife (subunit=0 tariff=0 storagenr=5)
    // (aventieshca) 33: 6E vif (Units for H.C.A.)
    // (aventieshca) 34: * 3402 consumption at set date 5 (564.000000 hca)
    // (aventieshca) 36: 82 dif (16 Bit Integer/Binary Instantaneous value)
    // (aventieshca) 37: 03 dife (subunit=0 tariff=0 storagenr=6)
    // (aventieshca) 38: 6E vif (Units for H.C.A.)
    // (aventieshca) 39: * 3101 consumption at set date 6 (305.000000 hca)
    // (aventieshca) 3b: C2 dif (16 Bit Integer/Binary Instantaneous value storagenr=1)
    // (aventieshca) 3c: 03 dife (subunit=0 tariff=0 storagenr=7)
    // (aventieshca) 3d: 6E vif (Units for H.C.A.)
    // (aventieshca) 3e: * F701 consumption at set date 7 (503.000000 hca)
    // (aventieshca) 40: 82 dif (16 Bit Integer/Binary Instantaneous value)
    // (aventieshca) 41: 04 dife (subunit=0 tariff=0 storagenr=8)
    // (aventieshca) 42: 6E vif (Units for H.C.A.)
    // (aventieshca) 43: * FD00 consumption at set date 8 (253.000000 hca)
    // (aventieshca) 45: C2 dif (16 Bit Integer/Binary Instantaneous value storagenr=1)
    // (aventieshca) 46: 04 dife (subunit=0 tariff=0 storagenr=9)
    // (aventieshca) 47: 6E vif (Units for H.C.A.)
    // (aventieshca) 48: * 6F00 consumption at set date 9 (111.000000 hca)
    // (aventieshca) 4a: 82 dif (16 Bit Integer/Binary Instantaneous value)
    // (aventieshca) 4b: 05 dife (subunit=0 tariff=0 storagenr=10)
    // (aventieshca) 4c: 6E vif (Units for H.C.A.)
    // (aventieshca) 4d: * 4100 consumption at set date 10 (65.000000 hca)
    // (aventieshca) 4f: C2 dif (16 Bit Integer/Binary Instantaneous value storagenr=1)
    // (aventieshca) 50: 05 dife (subunit=0 tariff=0 storagenr=11)
    // (aventieshca) 51: 6E vif (Units for H.C.A.)
    // (aventieshca) 52: * 3E00 consumption at set date 11 (62.000000 hca)
    // (aventieshca) 54: 82 dif (16 Bit Integer/Binary Instantaneous value)
    // (aventieshca) 55: 06 dife (subunit=0 tariff=0 storagenr=12)
    // (aventieshca) 56: 6E vif (Units for H.C.A.)
    // (aventieshca) 57: * 3E00 consumption at set date 12 (62.000000 hca)
    // (aventieshca) 59: C2 dif (16 Bit Integer/Binary Instantaneous value storagenr=1)
    // (aventieshca) 5a: 06 dife (subunit=0 tariff=0 storagenr=13)
    // (aventieshca) 5b: 6E vif (Units for H.C.A.)
    // (aventieshca) 5c: * 3E00 consumption at set date 13 (62.000000 hca)
    // (aventieshca) 5e: 82 dif (16 Bit Integer/Binary Instantaneous value)
    // (aventieshca) 5f: 07 dife (subunit=0 tariff=0 storagenr=14)
    // (aventieshca) 60: 6E vif (Units for H.C.A.)
    // (aventieshca) 61: * 3E00 consumption at set date 14 (62.000000 hca)
    // (aventieshca) 63: C2 dif (16 Bit Integer/Binary Instantaneous value storagenr=1)
    // (aventieshca) 64: 07 dife (subunit=0 tariff=0 storagenr=15)
    // (aventieshca) 65: 6E vif (Units for H.C.A.)
    // (aventieshca) 66: * 3E00 consumption at set date 15 (62.000000 hca)
    // (aventieshca) 68: 82 dif (16 Bit Integer/Binary Instantaneous value)
    // (aventieshca) 69: 08 dife (subunit=0 tariff=0 storagenr=16)
    // (aventieshca) 6a: 6E vif (Units for H.C.A.)
    // (aventieshca) 6b: * 3D00 consumption at set date 16 (61.000000 hca)
    // (aventieshca) 6d: C2 dif (16 Bit Integer/Binary Instantaneous value storagenr=1)
    // (aventieshca) 6e: 08 dife (subunit=0 tariff=0 storagenr=17)
    // (aventieshca) 6f: 6E vif (Units for H.C.A.)
    // (aventieshca) 70: * 3500 consumption at set date 17 (53.000000 hca)
    // (aventieshca) 72: 02 dif (16 Bit Integer/Binary Instantaneous value)
    // (aventieshca) 73: FD vif (Second extension FD of VIF-codes)
    // (aventieshca) 74: 17 vife (Error flags (binary))
    // (aventieshca) 75: * 0000 error flags (0000)

    int offset;
    string key;

    if (findKey(MeasurementType::Unknown, ValueInformation::HeatCostAllocation, 0, 0, &key, &t->values))
    {
        extractDVdouble(&t->values, key, &offset, &current_consumption_hca_);
        t->addMoreExplanation(offset, " current consumption (%f hca)", current_consumption_hca_);
    }

    if (findKey(MeasurementType::Unknown, ValueInformation::Date, 1, 0, &key, &t->values)) {
        struct tm date;
        extractDVdate(&t->values, key, &offset, &date);
        set_date_ = strdate(&date);
        t->addMoreExplanation(offset, " set date (%s)", set_date_.c_str());
    }

    for (int i=1; i<=17; ++i)
    {
        if (findKey(MeasurementType::Unknown, ValueInformation::HeatCostAllocation, i, 0, &key, &t->values))
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
