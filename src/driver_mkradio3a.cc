/*
 Copyright (C) 2019-2023 Fredrik Öhrström (gpl-3.0-or-later)

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
    };

    static bool ok = registerDriver([](DriverInfo&di)
    {
        di.setName("mkradio3a");
        di.setDefaultFields("name,id,total_m3,target_m3,current_date,prev_date,timestamp");
        di.setMeterType(MeterType::WaterMeter);
        di.addLinkMode(LinkMode::T1);
        di.addDetection(MANUFACTURER_TCH, 0x72,  0x50);
        di.usesProcessContent();
        di.setConstructor([](MeterInfo& mi, DriverInfo& di){ return shared_ptr<Meter>(new Driver(mi, di)); });
    });

    Driver::Driver(MeterInfo &mi, DriverInfo &di) : MeterCommonImplementation(mi, di)
    {
        addNumericField("total",
                        Quantity::Volume,
                        DEFAULT_PRINT_PROPERTIES,
                        "The total water consumption recorded by this meter.");

        addNumericField("target",
                        Quantity::Volume,
                        DEFAULT_PRINT_PROPERTIES,
                        "The total water consumption recorded at the beginning of this month.");

        addStringField("current_date",
                       "Date of current billing period.",
                       DEFAULT_PRINT_PROPERTIES);

        addStringField("prev_date",
                       "Date of previous billing period.",
                       DEFAULT_PRINT_PROPERTIES);
    }

    void Driver::processContent(Telegram *t)
    {
        // Unfortunately, the MK Radio 3 is mostly a proprieatary protocol
        // simple wrapped inside a wmbus telegram since the ci-field is 0xa2.
        // Which means that the entire payload is manufacturer specific.

        map<string,pair<int,DVEntry>> vendor_values;
        vector<uchar> content;

        t->extractPayload(&content);

        // Previous  & Current date
        uint16_t prev_date = (content[3] << 8) | content[2];
        uint prev_date_day = (prev_date >> 0) & 0x1F;
        uint prev_date_month = (prev_date >> 5) & 0x0F;
        uint prev_date_year = (prev_date >> 9) & 0xFF;
        setStringValue("prev_date", tostrprintf("%d-%02d-%02dT02:00:00Z", prev_date_year+2000, prev_date_month, prev_date_day));
        setStringValue("current_date", tostrprintf("%d-%02d-%02dT02:00:00Z", prev_date_year+2000, prev_date_month, prev_date_day));

        // Previous consumption
        uchar prev_lo = content[4];
        uchar prev_me = content[5];
        uchar prev_hi = content[6];
        double prev = (256.0*256.0*prev_hi+256.0*prev_me+prev_lo)/10.0;

        // Current consumption
        // uchar curr_lo = content[7];
        // uchar curr_hi = content[8];
        // double curr = (256.0*curr_hi+curr_lo)/10.0;
        double curr = prev;

        double total_water_consumption_m3 = curr;
        setNumericValue("total", Unit::M3, total_water_consumption_m3);
        double target_water_consumption_m3 = prev;
        setNumericValue("target", Unit::M3, target_water_consumption_m3);
    }
}

// Test: TCH mkradio3a 62560642 NOKEY
// 7C31 => 317C : Date
// 102500 => 002510 : water meter value (same as digital one)
// 2934 => 3429 : ???
// telegram=|36446850420656625072A2_0C007C3110250000293400373A002E38000E15002F37003A39003835002F24003930001D2500312500162900|
// {"_":"telegram", "media":"cold water", "meter":"mkradio3a", "name":"", "id":"62560642", "target_m3":948.8, "total_m3":948.8, "current_date":"2024-11-28T02:00:00Z", "prev_date":"2024-11-28T02:00:00Z", "timestamp":"2024-11-28T08:18:58Z"}
// 
