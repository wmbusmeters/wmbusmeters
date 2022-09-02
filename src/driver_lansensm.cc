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
    };

    static bool ok = registerDriver([](DriverInfo&di)
    {
        di.setName("lansensm");
        di.setDefaultFields("name,id,status,timestamp");
        di.setMeterType(MeterType::SmokeDetector);
        di.addLinkMode(LinkMode::T1);
        di.addDetection(MANUFACTURER_LAS, 0x1a, 0x03);
        di.setConstructor([](MeterInfo& mi, DriverInfo& di){ return shared_ptr<Meter>(new Driver(mi, di)); });
    });

    Driver::Driver(MeterInfo &mi, DriverInfo &di) : MeterCommonImplementation(mi, di)
    {
        /* BATTERY Low battery in TPL sts?
           SMOKE Smoke present (Alarm) bit 0x04 in error flags.
           END OF LIFE Device is ending its max service time of 10 years where?
           MALFUNCTION Malfunction warning where?
           NO CONNECTION No connection between the radio - smokesensor where?
           MANUAL TEST Manual test performed bit 0x08 in error flags.
        */
        addStringFieldWithExtractorAndLookup(
            "status",
            "Meter error flags. IMPORTANT! Smoke alarm is NOT reported here! You MUST check last alarm date and counter!",
            PrintProperty::JSON | PrintProperty::FIELD | PrintProperty::IMPORTANT |
            PrintProperty::STATUS | PrintProperty::JOIN_TPL_STATUS,
            FieldMatcher::build()
            .set(VIFRange::ErrorFlags)
            .add(VIFCombinable::StandardConformantDataContent),
            Translate::Lookup(
            {
                {
                    {
                        "ERROR_FLAGS",
                        Translate::Type::BitToString,
                        0xffff,
                        "OK",
                        {
                            { 0x0004, "SMOKE" },
                            { 0x0008, "TEST" }
                        }
                    },
                },
            }));

        addNumericFieldWithExtractor(
            "access",
            "Unknown counter.",
            PrintProperty::FIELD | PrintProperty::JSON | PrintProperty::OPTIONAL,
            Quantity::Counter,
            VifScaling::None,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::AccessNumber)
            );

        addNumericFieldWithExtractor(
            "woot",
            "Unknown counter.",
            PrintProperty::FIELD | PrintProperty::JSON | PrintProperty::OPTIONAL,
            Quantity::Counter,
            VifScaling::None,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::Dimensionless)
            );

    }
}

// Test: SMOKEA lansensm 00010204 NOKEY
// telegram=|2E44333004020100031A7AC40020052F2F_02FD971D000004FD084C02000004FD3A467500002F2F2F2F2F2F2F2F2F2F|
// {"media":"smoke detector","meter":"lansensm","name":"SMOKEA","id":"00010204","status":"OK","access_counter":588,"woot_counter":30022,"timestamp":"1111-11-11T11:11:11Z"}
// |SMOKEA;00010204;OK;1111-11-11 11:11.11

// telegram=|2E44333004020100031A7ADE0020052F2F_02FD971D040004FD086502000004FD3A010000002F2F2F2F2F2F2F2F2F2F|
// {"media":"smoke detector","meter":"lansensm","name":"SMOKEA","id":"00010204","status":"SMOKE","access_counter":613,"woot_counter":1,"timestamp":"1111-11-11T11:11:11Z"}
// |SMOKEA;00010204;SMOKE;1111-11-11 11:11.11
