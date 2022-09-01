/*
 Copyright (C) 2022 Fredrik Öhrström (gpl-3.0-or-later)

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

namespace
{
    struct Driver : public virtual MeterCommonImplementation
    {
        Driver(MeterInfo &mi, DriverInfo &di);

        void processContent(Telegram *t);

        double allocation_hca_ {};
        double room_c_ {};
    };

    static bool ok = registerDriver([](DriverInfo&di)
    {
        di.setName("apatoreitn");
        di.setDefaultFields("name,id,room_c,allocation_hca,timestamp");
        di.setMeterType(MeterType::HeatCostAllocationMeter);
        di.addDetection(MANUFACTURER_APA, 0x08,  0x04);
        di.setConstructor([](MeterInfo& mi, DriverInfo& di){ return shared_ptr<Meter>(new Driver(mi, di)); });
    });

    Driver::Driver(MeterInfo &mi, DriverInfo &di) : MeterCommonImplementation(mi, di)
    {
        addPrint("allocation", Quantity::HCA,
                 [&](Unit u){ return convert(allocation_hca_, Unit::HCA, u); },
                 "The current heat cost allocation calculated by this meter.",
                 PrintProperty::FIELD | PrintProperty::JSON);

        addPrint("room", Quantity::Temperature,
                 [&](Unit u){ return convert(room_c_, Unit::C, u); },
                 "The room temperature.",
                 PrintProperty::FIELD | PrintProperty::JSON);
    }

    void Driver::processContent(Telegram *t)
    {
        vector<uchar> content;
        t->extractPayload(&content);

        size_t offset = 23;
        if (offset+1 < content.size())
        {
            double hi = content[offset+1];
            double lo = content[offset+0];
            room_c_ = hi + lo/256.0;

            t->addSpecialExplanation(offset, 2, KindOfData::CONTENT, Understanding::FULL,
                                     "*** %02X%02X room (%f c)",
                                     content[offset+0], content[offset+1],
                                     room_c_);
        }
    }
}

// Test: HCA apatoreitn 37373737 NOKEY
// telegram=|25441486373737370408B60AFFFFF5450186F41B9D58A0A100007809000000001F2D6416C819|
// {"media":"heat cost allocation","meter":"apatoreitn","name":"HCA","id":"37373737","allocation_hca":0,"room_c":22.390625,"timestamp":"1111-11-11T11:11:11Z"}
// |HCA;37373737;22.390625;0;1111-11-11 11:11.11
