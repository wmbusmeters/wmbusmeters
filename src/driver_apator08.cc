/*
 Copyright (C) 2020-2022 Fredrik Öhrström (gpl-3.0-or-later)

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

#include"meters_common_implementation.h"

using namespace std;

struct MeterApator08 : public virtual MeterCommonImplementation
{
    MeterApator08(MeterInfo &mi, DriverInfo &di);

private:

    void processContent(Telegram *t);

    double total_water_consumption_m3_ {};
};

static bool ok = registerDriver([](DriverInfo&di)
{
    di.setName("apator08");
    di.setMeterType(MeterType::WaterMeter);
    di.setExpectedTPLSecurityMode(TPLSecurityMode::AES_CBC_IV);
    di.addLinkMode(LinkMode::T1);
    di.addDetection(0x8614/*APT?*/, 0x03,  0x03);
    di.setConstructor([](MeterInfo& mi, DriverInfo& di){ return shared_ptr<Meter>(new MeterApator08(mi, di)); });
});

MeterApator08::MeterApator08(MeterInfo &mi, DriverInfo &di) : MeterCommonImplementation(mi, di)
{
    addField(
        "total",
        Quantity::Volume,
        PrintProperty::JSON | PrintProperty::FIELD | PrintProperty::IMPORTANT,
        "The total water consumption recorded by this meter.",
        SET_FUNC(total_water_consumption_m3_, Unit::M3),
        GET_FUNC(total_water_consumption_m3_, Unit::M3));
}

void MeterApator08::processContent(Telegram *t)
{
    // Unfortunately, the at-wmbus-08 is mostly a proprietary protocol
    // simple wrapped inside a wmbus telegram. Naughty!

    // The telegram says gas (0x03) but it is a water meter.... so fix this.
    t->dll_type = 0x07;

    vector<uchar> content;
    t->extractPayload(&content);

    map<string,pair<int,DVEntry>> vendor_values;

    string total;
    strprintf(total, "%02x%02x%02x%02x", content[0], content[1], content[2], content[3]);

    vendor_values["0413"] = { 25, DVEntry(MeasurementType::Instantaneous, 0x13, 0, 0, 0, total) };
    int offset;
    string key;
    if(findKey(MeasurementType::Unknown, ValueInformation::Volume, 0, 0, &key, &vendor_values))
    {
        extractDVdouble(&vendor_values, key, &offset, &total_water_consumption_m3_);
        // Now divide with 3! Is this the same for all apator08 meters? Time will tell.
        total_water_consumption_m3_ /= 3.0;

        total = "*** 10|"+total+" total consumption (%f m3)";
        t->addSpecialExplanation(offset, 4, KindOfData::CONTENT, Understanding::FULL, total.c_str(), total_water_consumption_m3_);
    }
}

// Test: Vatten apator08 004444dd NOKEY
// telegram=|73441486DD4444000303A0B9E527004C4034B31CED0106FF01D093270065F022009661230054D02300EC49240018B424005F012500936D2500FFD525000E3D26001EAC26000B2027000300000000371D0B2000000000000024000000000000280000000000002C0033150C010D2F000000000000|
// {"media":"water","meter":"apator08","name":"Vatten","id":"004444dd","total_m3":871.571,"timestamp":"1111-11-11T11:11:11Z"}
// |Vatten;004444dd;871.571000;1111-11-11 11:11.11
