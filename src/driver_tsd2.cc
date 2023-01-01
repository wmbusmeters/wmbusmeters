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

namespace
{
    struct Driver : public virtual MeterCommonImplementation
    {
        Driver(MeterInfo &mi, DriverInfo &di);

        void processContent(Telegram *t);

    private:

        string status();
        bool smokeDetected();
        string previousDate();

        uint16_t info_codes_ {};
        bool error_ {};
        string previous_date_ {};
    };

    static bool ok = registerDriver([](DriverInfo&di)
    {
        di.setName("tsd2");
        di.setDefaultFields("name,id,status,prev_date,timestamp");
        di.setMeterType(MeterType::WaterMeter);
        di.addLinkMode(LinkMode::T1);
        di.addDetection(MANUFACTURER_TCH, 0xf0,  0x76);

        di.setConstructor([](MeterInfo& mi, DriverInfo& di){ return shared_ptr<Meter>(new Driver(mi, di)); });
    });

    Driver::Driver(MeterInfo &mi, DriverInfo &di) : MeterCommonImplementation(mi, di)
    {
        setMeterType(MeterType::SmokeDetector);

        setExpectedTPLSecurityMode(TPLSecurityMode::AES_CBC_IV);

        addLinkMode(LinkMode::T1);

        addPrint("status", Quantity::Text,
                 [&](){ return status(); },
                 "The current status: OK, SMOKE or ERROR.",
                  DEFAULT_PRINT_PROPERTIES);

        addPrint("prev_date", Quantity::Text,
                 [&](){ return previousDate(); },
                 "Date of previous billing period.",
                  DEFAULT_PRINT_PROPERTIES);
    }

#define INFO_CODE_SMOKE 0x0001

    bool Driver::smokeDetected()
    {
        return info_codes_ & INFO_CODE_SMOKE;
    }

    void Driver::processContent(Telegram *t)
    {
        vector<uchar> data;
        t->extractPayload(&data);

        if(data.empty())
        {
            error_ = true;
            return;
        }

        info_codes_ = data[0];
        error_ = false;
        // Check CRC etc here....

        // Previous date
        uint16_t prev_date = (data[2] << 8) | data[1];
        uint prev_date_day = (prev_date >> 0) & 0x1F;
        uint prev_date_month = (prev_date >> 5) & 0x0F;
        uint prev_date_year = (prev_date >> 9) & 0x3F;
        strprintf(&previous_date_, "%d-%02d-%02dT02:00:00Z", prev_date_year+2000, prev_date_month, prev_date_day);

        string prev_date_str;
        strprintf(&prev_date_str, "%04x", prev_date);
        uint offset = t->parsed.size() + 1;
        t->explanations.push_back(Explanation(offset, 1, prev_date_str, KindOfData::CONTENT, Understanding::FULL));
        t->addMoreExplanation(offset, " previous date (%s)", previous_date_.c_str());
    }

    string Driver::status()
    {
        string s;
        bool smoke = info_codes_ & INFO_CODE_SMOKE;
        if (smoke)
        {
            s.append("SMOKE ");
        }

        if (error_)
        {
            s.append("ERROR ");
        }

        if (s.length() > 0)
        {
            s.pop_back();
            return s;
        }
        return "OK";
    }

    string Driver::previousDate()
    {
        return previous_date_;
    }
}

// Test: Smokey tsd2 91633569 NOKEY
// telegram=|294468506935639176F0A0_009F2782290060822900000401D6311AF93E1BF93E008DC3009ED4000FE500|
// {"media":"smoke detector","meter":"tsd2","name":"Smokey","id":"91633569","status":"OK","prev_date":"2019-12-31T02:00:00Z","timestamp":"1111-11-11T11:11:11Z"}
// |Smokey;91633569;OK;2019-12-31T02:00:00Z;1111-11-11 11:11.11
