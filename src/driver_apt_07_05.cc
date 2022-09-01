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

        double total_m3_ {};
    };

    static bool ok = registerDriver([](DriverInfo&di)
    {
        di.setName("apt_07_05");
        di.setDefaultFields("name,id,total_m3,timestamp");
        di.setMeterType(MeterType::WaterMeter);
        di.addDetection(0x8614 /*APT?*/, 0x07,  0x05);
        di.setConstructor([](MeterInfo& mi, DriverInfo& di){ return shared_ptr<Meter>(new Driver(mi, di)); });
    });

    Driver::Driver(MeterInfo &mi, DriverInfo &di) : MeterCommonImplementation(mi, di)
    {
        addPrint("total", Quantity::Volume,
                 [&](Unit u){ return convert(total_m3_, Unit::M3, u); },
                 "The total volume consumed.",
                 PrintProperty::FIELD | PrintProperty::JSON);
    }

    void Driver::processContent(Telegram *t)
    {
        vector<uchar> content;
        t->extractPayload(&content);

        /*
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
        */
        total_m3_ = 4711.0;
    }
}

// Test: WATER apt_07_05 37373737 NOKEY
// telegram=|5A441486373737370507B60AFFFFF5450106F41BA5717A8700408535B24C132D721277A85089C02D4FDA886486A89EF3B7FF4E7AF666FE4C58AFD0746925F27F416F8237AB1A7C2612AA5F88615E46AA4D535493EBCA4DC31514BA|
// {"media":"water","meter":"apt_07_05","name":"WATER","id":"37373737","total_m3":4711,"timestamp":"1111-11-11T11:11:11Z"}
// |WATER;37373737;4711;1111-11-11 11:11.11
