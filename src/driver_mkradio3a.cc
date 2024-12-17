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
        di.setDefaultFields("name,id,total_m3,target_m3,timestamp");
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

        addNumericField("last_jan",
                        Quantity::Volume,
                        DEFAULT_PRINT_PROPERTIES,
                        "The total water consumption recorded at the beginning of this month.");

        addNumericField("last_feb",
                        Quantity::Volume,
                        DEFAULT_PRINT_PROPERTIES,
                        "The total water consumption recorded at the beginning of this month.");

        addNumericField("last_mar",
                        Quantity::Volume,
                        DEFAULT_PRINT_PROPERTIES,
                        "The total water consumption recorded at the beginning of this month.");

        addNumericField("last_apr",
                        Quantity::Volume,
                        DEFAULT_PRINT_PROPERTIES,
                        "The total water consumption recorded at the beginning of this month.");

        addNumericField("last_may",
                        Quantity::Volume,
                        DEFAULT_PRINT_PROPERTIES,
                        "The total water consumption recorded at the beginning of this month.");

        addNumericField("last_jun",
                        Quantity::Volume,
                        DEFAULT_PRINT_PROPERTIES,
                        "The total water consumption recorded at the beginning of this month.");

        addNumericField("last_jul",
                        Quantity::Volume,
                        DEFAULT_PRINT_PROPERTIES,
                        "The total water consumption recorded at the beginning of this month.");

        addNumericField("last_aug",
                        Quantity::Volume,
                        DEFAULT_PRINT_PROPERTIES,
                        "The total water consumption recorded at the beginning of this month.");

        addNumericField("last_sep",
                        Quantity::Volume,
                        DEFAULT_PRINT_PROPERTIES,
                        "The total water consumption recorded at the beginning of this month.");

        addNumericField("last_oct",
                        Quantity::Volume,
                        DEFAULT_PRINT_PROPERTIES,
                        "The total water consumption recorded at the beginning of this month.");

        addNumericField("last_nov",
                        Quantity::Volume,
                        DEFAULT_PRINT_PROPERTIES,
                        "The total water consumption recorded at the beginning of this month.");

        addNumericField("last_dec",
                        Quantity::Volume,
                        DEFAULT_PRINT_PROPERTIES,
                        "The total water consumption recorded at the beginning of this month.");

        addStringField("target_date",
                       "Date of current billing period.",
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

        // Current date
        uint16_t curr_date = (content[3] << 8) | content[2];
        uint curr_date_day = (curr_date >> 0) & 0x1F;
        uint curr_date_month = (curr_date >> 5) & 0x0F;
        uint curr_date_year = (curr_date >> 9) & 0xFF;
        setStringValue("target_date", tostrprintf("%d-%02d-%02dT02:00:00Z", curr_date_year+2000, curr_date_month, curr_date_day));

        // Previous consumption
        // uchar prev_lo = content[8];
        // uchar prev_hi = content[9];
        // double prev = (256.0*prev_hi+prev_lo)/10.0;

        // Total consumption
        uchar total_lo = content[4];
        uchar total_me = content[5];
        uchar total_hi = content[6];
        double total_water_consumption_m3 = (256.0*256.0*total_hi+256.0*total_me+total_lo)/10.0;
        setNumericValue("total", Unit::M3, total_water_consumption_m3);


        double* prev_month = new double[20];
        double curr_month_m3;

        // Current & Prev Month consumption
        if (curr_date_day <= 15) {
           uchar prev_lo = content[8];
           curr_month_m3 = prev_lo/10.0;
           prev_month[ (curr_date_month + 12) % 12 ] = curr_month_m3;
           prev_lo = content[9];
           uchar prev_hi = content[11];
           prev_month[ (curr_date_month + 12 - 1) % 12 ] = (prev_hi+prev_lo)/10.0;
           prev_lo = content[12];
           prev_hi = content[14];
           prev_month[ (curr_date_month + 12 - 2) % 12 ] = (prev_hi+prev_lo)/10.0;
           prev_lo = content[15];
           prev_hi = content[17];
           prev_month[ (curr_date_month + 12 - 3) % 12 ] = (prev_hi+prev_lo)/10.0;
           prev_lo = content[18];
           prev_hi = content[20];
           prev_month[ (curr_date_month + 12 - 4) % 12 ] = (prev_hi+prev_lo)/10.0;
           prev_lo = content[21];
           prev_hi = content[23];
           prev_month[ (curr_date_month + 12 - 5) % 12 ] = (prev_hi+prev_lo)/10.0;
           prev_lo = content[24];
           prev_hi = content[26];
           prev_month[ (curr_date_month + 12 - 6) % 12 ] = (prev_hi+prev_lo)/10.0;
           prev_lo = content[27];
           prev_hi = content[29];
           prev_month[ (curr_date_month + 12 - 7) % 12 ] = (prev_hi+prev_lo)/10.0;
           prev_lo = content[30];
           prev_hi = content[32];
           prev_month[ (curr_date_month + 12 - 8) % 12 ] = (prev_hi+prev_lo)/10.0;
           prev_lo = content[33];
           prev_hi = content[35];
           prev_month[ (curr_date_month + 12 - 9) % 12 ] = (prev_hi+prev_lo)/10.0;
           prev_lo = content[36];
           prev_hi = content[38];
           prev_month[ (curr_date_month + 12 - 10) % 12 ] = (prev_hi+prev_lo)/10.0;
           prev_lo = content[39];
           prev_hi = content[41];
           prev_month[ (curr_date_month + 12 - 11) % 12 ] = (prev_hi+prev_lo)/10.0;
           //prev_lo = content[42];
           //prev_month[ (curr_date_month + 12 - 11) % 12 ] = 0.0;
        }else{
           uchar prev_lo = content[8];
           uchar prev_hi = content[9];
           curr_month_m3 = (prev_hi+prev_lo)/10.0;
           prev_month[ (curr_date_month + 12) % 12 ] = curr_month_m3;
           prev_lo = content[11];
           prev_hi = content[12];
           prev_month[ (curr_date_month + 12 - 1) % 12 ] = (prev_hi+prev_lo)/10.0;
           prev_lo = content[14];
           prev_hi = content[15];
           prev_month[ (curr_date_month + 12 - 2) % 12 ] = (prev_hi+prev_lo)/10.0;
           prev_lo = content[17];
           prev_hi = content[18];
           prev_month[ (curr_date_month + 12 - 3) % 12 ] = (prev_hi+prev_lo)/10.0;
           prev_lo = content[20];
           prev_hi = content[21];
           prev_month[ (curr_date_month + 12 - 4) % 12 ] = (prev_hi+prev_lo)/10.0;
           prev_lo = content[23];
           prev_hi = content[24];
           prev_month[ (curr_date_month + 12 - 5) % 12 ] = (prev_hi+prev_lo)/10.0;
           prev_lo = content[26];
           prev_hi = content[27];
           prev_month[ (curr_date_month + 12 - 6) % 12 ] = (prev_hi+prev_lo)/10.0;
           prev_lo = content[29];
           prev_hi = content[30];
           prev_month[ (curr_date_month + 12 - 7) % 12 ] = (prev_hi+prev_lo)/10.0;
           prev_lo = content[32];
           prev_hi = content[33];
           prev_month[ (curr_date_month + 12 - 8) % 12 ] = (prev_hi+prev_lo)/10.0;
           prev_lo = content[35];
           prev_hi = content[36];
           prev_month[ (curr_date_month + 12 - 9) % 12 ] = (prev_hi+prev_lo)/10.0;
           prev_lo = content[38];
           prev_hi = content[39];
           prev_month[ (curr_date_month + 12 - 10) % 12 ] = (prev_hi+prev_lo)/10.0;
           prev_lo = content[41];
           prev_hi = content[42];
           prev_month[ (curr_date_month + 12 - 11) % 12 ] = (prev_hi+prev_lo)/10.0;
        }
        setNumericValue("target", Unit::M3, curr_month_m3);
        setNumericValue("last_jan", Unit::M3, prev_month[1]);
        setNumericValue("last_feb", Unit::M3, prev_month[2]);
        setNumericValue("last_mar", Unit::M3, prev_month[3]);
        setNumericValue("last_apr", Unit::M3, prev_month[4]);
        setNumericValue("last_may", Unit::M3, prev_month[5]);
        setNumericValue("last_jun", Unit::M3, prev_month[6]);
        setNumericValue("last_jul", Unit::M3, prev_month[7]);
        setNumericValue("last_aug", Unit::M3, prev_month[8]);
        setNumericValue("last_sep", Unit::M3, prev_month[9]);
        setNumericValue("last_oct", Unit::M3, prev_month[10]);
        setNumericValue("last_nov", Unit::M3, prev_month[11]);
        setNumericValue("last_dec", Unit::M3, prev_month[0]);
    }
}

// Test: TCH mkradio3a 62560642 NOKEY
// 7C31 => 317C : Date
// 102500 => 002510 : water meter value (same as digital one)
// 2934 => 3429 : current month
// telegram=|36446850420656625072A2_0C007C3110250000293400373A002E38000E15002F37003A39003835002F24003930001D2500312500162900|
//{ "_":"telegram", "media":"cold water", "meter":"mkradio3a", "name":"", "id":"62560642", "target_m3":9.3, "last_apr_m3":8.3, "last_aug_m3":3.5, "last_dec_m3":6.3, "last_feb_m3":6.6, "last_jan_m3":8.6, "last_jul_m3":10.2, "last_jun_m3":11.5, "last_mar_m3":10.5, "last_may_m3":10.9, "last_nov_m3":9.3, "last_oct_m3":11.3, "last_sep_m3":10.2, "total_m3":948.8, "target_date":"2024-11-28T02:00:00Z", "timestamp":"2024-12-01T17:27:58Z" }
