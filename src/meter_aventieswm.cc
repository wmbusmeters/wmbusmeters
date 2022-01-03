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
#include"util.h"

using namespace std;

struct MeterAventiesWM : public virtual MeterCommonImplementation {
    MeterAventiesWM(MeterInfo &mi);

    // Total water counted through the meter
    double totalWaterConsumption(Unit u);
    bool  hasTotalWaterConsumption();

    double consumptionAtSetDate(Unit u);

private:
    void processContent(Telegram *t);

    string errorFlagsHumanReadable();

    double total_water_consumption_m3_ {};
    double consumption_at_set_date_m3_[14];
    uint16_t error_flags_;

};

shared_ptr<Meter> createAventiesWM(MeterInfo &mi)
{
    return shared_ptr<Meter>(new MeterAventiesWM(mi));
}

MeterAventiesWM::MeterAventiesWM(MeterInfo &mi) :
    MeterCommonImplementation(mi, "aventieswm")
{
    setMeterType(MeterType::WaterMeter);

    setExpectedTPLSecurityMode(TPLSecurityMode::AES_CBC_IV);

    addLinkMode(LinkMode::T1);

    addPrint("total", Quantity::Volume,
             [&](Unit u){ return totalWaterConsumption(u); },
             "The total water consumption recorded by this meter.",
             true, true);

/*    addPrint("set_date", Quantity::Text,
             [&](){ return setDate(); },
             "The most recent billing period date.",
             false, true);
*/

    for (int i=1; i<=14; ++i)
    {
        string msg, info;
        strprintf(msg, "consumption_at_set_date_%d", i);
        strprintf(info, "Water consumption at the %d billing period date.", i);
        addPrint(msg, Quantity::Volume,
                 [this,i](Unit u){ return consumption_at_set_date_m3_[i-1]; },
                 info,
                 false, true);
    }

    addPrint("error_flags", Quantity::Text,
            [&](){ return errorFlagsHumanReadable(); },
            "Error flags.",
            true, true);
}

string MeterAventiesWM::errorFlagsHumanReadable()
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


void MeterAventiesWM::processContent(Telegram *t)
{
    /*
        (q400) 17: 2f2f decrypt check bytes
        (q400) 19: 04 dif (32 Bit Integer/Binary Instantaneous value)
        (q400) 1a: 13 vif (Volume l)
        (q400) 1b: * 1F480000 total consumption (18.463000 m3)
        (q400) 1f: 43 dif (24 Bit Integer/Binary Instantaneous value storagenr=1)
        (q400) 20: 14 vif (Volume 10⁻² m³)
        (q400) 21: * 360700 consumption at set date (18.460000 m3)
        (q400) 24: 83 dif (24 Bit Integer/Binary Instantaneous value)
        (q400) 25: 01 dife (subunit=0 tariff=0 storagenr=2)
        (q400) 26: 14 vif (Volume 10⁻² m³)
        (q400) 27: FE0600
        (q400) 2a: C3 dif (24 Bit Integer/Binary Instantaneous value storagenr=1)
        (q400) 2b: 01 dife (subunit=0 tariff=0 storagenr=3)
        (q400) 2c: 14 vif (Volume 10⁻² m³)
        (q400) 2d: DD0600
        (q400) 30: 83 dif (24 Bit Integer/Binary Instantaneous value)
        (q400) 31: 02 dife (subunit=0 tariff=0 storagenr=4)
        (q400) 32: 14 vif (Volume 10⁻² m³)
        (q400) 33: C30600
        (q400) 36: C3 dif (24 Bit Integer/Binary Instantaneous value storagenr=1)
        (q400) 37: 02 dife (subunit=0 tariff=0 storagenr=5)
        (q400) 38: 14 vif (Volume 10⁻² m³)
        (q400) 39: BD0600
        (q400) 3c: 83 dif (24 Bit Integer/Binary Instantaneous value)
        (q400) 3d: 03 dife (subunit=0 tariff=0 storagenr=6)
        (q400) 3e: 14 vif (Volume 10⁻² m³)
        (q400) 3f: BC0600
        (q400) 42: C3 dif (24 Bit Integer/Binary Instantaneous value storagenr=1)
        (q400) 43: 03 dife (subunit=0 tariff=0 storagenr=7)
        (q400) 44: 14 vif (Volume 10⁻² m³)
        (q400) 45: 000000
        (q400) 48: 83 dif (24 Bit Integer/Binary Instantaneous value)
        (q400) 49: 04 dife (subunit=0 tariff=0 storagenr=8)
        (q400) 4a: 14 vif (Volume 10⁻² m³)
        (q400) 4b: BB0600
        (q400) 4e: C3 dif (24 Bit Integer/Binary Instantaneous value storagenr=1)
        (q400) 4f: 04 dife (subunit=0 tariff=0 storagenr=9)
        (q400) 50: 14 vif (Volume 10⁻² m³)
        (q400) 51: BB0600
        (q400) 54: 83 dif (24 Bit Integer/Binary Instantaneous value)
        (q400) 55: 05 dife (subunit=0 tariff=0 storagenr=10)
        (q400) 56: 14 vif (Volume 10⁻² m³)
        (q400) 57: BA0600
        (q400) 5a: C3 dif (24 Bit Integer/Binary Instantaneous value storagenr=1)
        (q400) 5b: 05 dife (subunit=0 tariff=0 storagenr=11)
        (q400) 5c: 14 vif (Volume 10⁻² m³)
        (q400) 5d: B80600
        (q400) 60: 83 dif (24 Bit Integer/Binary Instantaneous value)
        (q400) 61: 06 dife (subunit=0 tariff=0 storagenr=12)
        (q400) 62: 14 vif (Volume 10⁻² m³)
        (q400) 63: 220600
        (q400) 66: C3 dif (24 Bit Integer/Binary Instantaneous value storagenr=1)
        (q400) 67: 06 dife (subunit=0 tariff=0 storagenr=13)
        (q400) 68: 14 vif (Volume 10⁻² m³)
        (q400) 69: DC0500
        (q400) 6c: 83 dif (24 Bit Integer/Binary Instantaneous value)
        (q400) 6d: 07 dife (subunit=0 tariff=0 storagenr=14)
        (q400) 6e: 14 vif (Volume 10⁻² m³)
        (q400) 6f: CD0500
        (q400) 72: 02 dif (16 Bit Integer/Binary Instantaneous value)
        (q400) 73: FD vif (Second extension FD of VIF-codes)
        (q400) 74: 17 vife (Error flags (binary))
        (q400) 75: 0000    */
    int offset;
    string key;

    if(findKey(MeasurementType::Unknown, ValueInformation::Volume, 0, 0, &key, &t->values)) {
        extractDVdouble(&t->values, key, &offset, &total_water_consumption_m3_);
        t->addMoreExplanation(offset, " total consumption (%f m3)", total_water_consumption_m3_);
    }

    for (int i=1; i<=14; ++i)
    {
        if (findKey(MeasurementType::Unknown, ValueInformation::Volume, i, 0, &key, &t->values))
        {
            string info;
            strprintf(info, " consumption at set date %d (%%f m3)", i);
            extractDVdouble(&t->values, key, &offset, &consumption_at_set_date_m3_[i-1]);
            t->addMoreExplanation(offset, info.c_str(), consumption_at_set_date_m3_[i-1]);
        }
    }

    key = "02FD17";
    if (hasKey(&t->values, key)) {
        extractDVuint16(&t->values, "02FD17", &offset, &error_flags_);
        t->addMoreExplanation(offset, " error flags (%04X)", error_flags_);
    }

}

double MeterAventiesWM::totalWaterConsumption(Unit u)
{
    assertQuantity(u, Quantity::Volume);
    return convert(total_water_consumption_m3_, Unit::M3, u);
}

bool MeterAventiesWM::hasTotalWaterConsumption()
{
    return true;
}

double MeterAventiesWM::consumptionAtSetDate(Unit u)
{
    return consumption_at_set_date_m3_[0];
}
