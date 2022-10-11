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
    };

    static bool ok = registerDriver([](DriverInfo&di)
    {
        di.setName("itron");
        di.setDefaultFields("name,id,total_m3,target_m3,timestamp");
        di.setMeterType(MeterType::WaterMeter);
        di.addLinkMode(LinkMode::T1);
        di.addDetection(MANUFACTURER_ITW,  0x07,  0x03);
        di.addDetection(MANUFACTURER_ITW,  0x07,  0x33);
        di.setConstructor([](MeterInfo& mi, DriverInfo& di){ return shared_ptr<Meter>(new Driver(mi, di)); });
    });

    Driver::Driver(MeterInfo &mi, DriverInfo &di) : MeterCommonImplementation(mi, di)
    {
        setMeterType(MeterType::WaterMeter);

        setExpectedTPLSecurityMode(TPLSecurityMode::AES_CBC_IV);

        addLinkMode(LinkMode::T1);

        addOptionalCommonFields();
        addOptionalFlowRelatedFields();

        addStringFieldWithExtractorAndLookup(
            "status",
            "Status and error flags.",
            PrintProperty::JSON | PrintProperty::FIELD | PrintProperty::OPTIONAL,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::ErrorFlags)
            .add(VIFCombinable::RecordErrorCodeMeterToController),
            {
                {
                    {
                        "ERROR_FLAGS",
                        Translate::Type::BitToString,
                        0xffffff,
                        "OK",
                        {
                            // No known layout for field
                        }
                    },
                },
            });

        addNumericFieldWithExtractor(
            "target",
             "The total water consumption recorded at the end of previous billing period.",
            PrintProperty::JSON | PrintProperty::FIELD | PrintProperty::IMPORTANT,
            Quantity::Volume,
            VifScaling::Auto,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::Volume)
            .set(StorageNr(1))
            );

        addStringFieldWithExtractor(
            "target_date",
            "Date when previous billing period ended.",
            PrintProperty::JSON | PrintProperty::OPTIONAL,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::Date)
            .set(StorageNr(1))
            );

        addStringFieldWithExtractorAndLookup(
            "unknown_a",
            "Unknown flags.",
            PrintProperty::JSON | PrintProperty::OPTIONAL,
            FieldMatcher::build()
            .set(DifVifKey("047F")),
            {
                {
                    {
                        "WOOTA",
                        Translate::Type::BitToString,
                        0xffffffff,
                        "",
                        {
                            // No known layout for field
                        }
                    },
                },
            });

        addStringFieldWithExtractorAndLookup(
            "unknown_b",
            "Unknown flags.",
            PrintProperty::JSON | PrintProperty::OPTIONAL,
            FieldMatcher::build()
            .set(DifVifKey("027F")),
            {
                {
                    {
                        "WOOTB",
                        Translate::Type::BitToString,
                        0xffff,
                        "",
                        {
                            // No known layout for field
                        }
                    },
                },
            });

    }
}

// Test: SomeWater itron 12345698 NOKEY
// Comment: Test ITRON T1 telegram not encrypted, which has no 2f2f markers.
// telegram=|384497269856341203077AD90000A0#0413FD110000066D2C1AA1D521004413300F0000426CBF2C047F0000060C027F862A0E79678372082100|
// {"media":"water","meter":"itron","name":"SomeWater","id":"12345698","enhanced_id":"002108728367","meter_datetime":"2022-01-21 01:26","total_m3":4.605,"target_m3":3.888,"target_date":"2021-12-31","unknown_a":"WOOTA_C060000","unknown_b":"WOOTB_2A86","timestamp":"1111-11-11T11:11:11Z"}
// |SomeWater;12345698;4.605;3.888;1111-11-11 11:11.11

// Test: MoreWater itron 18000056 NOKEY
// telegram=|46449726560000183307725600001897263307AF0030052F2F_066D0E1015C82A000C13771252000C933C000000000B3B0400004C1361045200426CC12A03FD971C0000002F2F2F|
// {"media":"water","meter":"itron","name":"MoreWater","id":"18000056","meter_datetime":"2022-10-08 21:16","total_m3":521.277,"total_backward_m3":0,"volume_flow_m3h":0.004,"status":"OK","target_m3":520.461,"target_date":"2022-10-01","timestamp":"1111-11-11T11:11:11Z"}
// |MoreWater;18000056;521.277;520.461;1111-11-11 11:11.11
