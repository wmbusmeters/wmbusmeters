/*
 Copyright (C) 2019-2022 Fredrik Öhrström (gpl-3.0-or-later)

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

        double totalWaterConsumption(Unit u);
        double targetWaterConsumption(Unit u);

        double total_water_consumption_m3_ {};
        double target_water_consumption_m3_ {};
    };

    static bool ok = registerDriver([](DriverInfo&di)
    {
        di.setName("mkradio4");
        di.setDefaultFields("name,id,total_m3,target_m3,timestamp");
        di.setMeterType(MeterType::WaterMeter);
        di.addLinkMode(LinkMode::T1);
        di.addDetection(MANUFACTURER_TCH, 0x62,  0x95);
        di.addDetection(MANUFACTURER_TCH, 0x62,  0x70);
        di.addDetection(MANUFACTURER_TCH, 0x72,  0x95);
        di.addDetection(MANUFACTURER_TCH, 0x72,  0x70);

        di.setConstructor([](MeterInfo& mi, DriverInfo& di){ return shared_ptr<Meter>(new Driver(mi, di)); });
    });

    Driver::Driver(MeterInfo &mi, DriverInfo &di) : MeterCommonImplementation(mi, di)
    {
        addPrint("total", Quantity::Volume,
                 [&](Unit u){ return totalWaterConsumption(u); },
                 "The total water consumption recorded by this meter.",
                  DEFAULT_PRINT_PROPERTIES);

        addPrint("target", Quantity::Volume,
                 [&](Unit u){ return targetWaterConsumption(u); },
                 "The total water consumption recorded at the beginning of this month.",
                  DEFAULT_PRINT_PROPERTIES);
    }

    void Driver::processContent(Telegram *t)
    {
        // Unfortunately, the MK Radio 4 is mostly a proprieatary protocol
        // simple wrapped inside a wmbus telegram since the ci-field is 0xa2.
        // Which means that the entire payload is manufacturer specific.

        map<string,pair<int,DVEntry>> vendor_values;
        vector<uchar> content;

        t->extractPayload(&content);

        uchar prev_lo = content[3];
        uchar prev_hi = content[4];
        double prev = (256.0*prev_hi+prev_lo)/10.0;

        string prevs;
        strprintf(&prevs, "%02x%02x", prev_lo, prev_hi);
        int offset = t->parsed.size()+3;
        vendor_values["0215"] = { offset, DVEntry(offset, DifVifKey("0215"), MeasurementType::Instantaneous, 0x15, {}, 0, 0, 0, prevs) };
        t->explanations.push_back(Explanation(offset, 2, prevs, KindOfData::CONTENT, Understanding::FULL));
        t->addMoreExplanation(offset, " prev consumption (%f m3)", prev);

        uchar curr_lo = content[7];
        uchar curr_hi = content[8];
        double curr = (256.0*curr_hi+curr_lo)/10.0;

        string currs;
        strprintf(&currs, "%02x%02x", curr_lo, curr_hi);
        offset = t->parsed.size()+7;
        vendor_values["0215"] = { offset, DVEntry(offset, DifVifKey("0215"), MeasurementType::Instantaneous, 0x15, {}, 0, 0, 0, currs) };
        t->explanations.push_back(Explanation(offset, 2, currs, KindOfData::CONTENT, Understanding::FULL));
        t->addMoreExplanation(offset, " curr consumption (%f m3)", curr);

        total_water_consumption_m3_ = prev+curr;
        target_water_consumption_m3_ = prev;
    }

    double Driver::totalWaterConsumption(Unit u)
    {
        assertQuantity(u, Quantity::Volume);
        return convert(total_water_consumption_m3_, Unit::M3, u);
    }

    double Driver::targetWaterConsumption(Unit u)
    {
        return target_water_consumption_m3_;
    }
}

// Test: Duschagain mkradio4 02410120 NOKEY
// telegram=|2F446850200141029562A2_06702901006017030004000300000000000000000000000000000000000000000000000000|
// {"media":"warm water","meter":"mkradio4","name":"Duschagain","id":"02410120","total_m3":0.4,"target_m3":0.1,"timestamp":"1111-11-11T11:11:11Z"}
// |Duschagain;02410120;0.4;0.1;1111-11-11 11:11.11
