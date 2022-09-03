/*
 Copyright (C) 2017-2022 Fredrik Öhrström (gpl-3.0-or-later)

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
    struct Driver : public virtual MeterCommonImplementation {
        Driver(MeterInfo &mi, DriverInfo &di);
    };

    static bool ok = registerDriver([](DriverInfo&di)
    {
        di.setName("multical21");
        di.setDefaultFields("name,id,total_m3,target_m3,max_flow_m3h,flow_temperature_c,external_temperature_c,status,timestamp");

        di.setMeterType(MeterType::WaterMeter);
        di.addLinkMode(LinkMode::C1);
        // Multical21
        di.addDetection(MANUFACTURER_KAM, 0x06,  0x1b);
        di.addDetection(MANUFACTURER_KAM, 0x16,  0x1b);

        di.setConstructor([](MeterInfo& mi, DriverInfo& di){ return shared_ptr<Meter>(new Driver(mi, di)); });
    });

    Driver::Driver(MeterInfo &mi, DriverInfo &di) : MeterCommonImplementation(mi, di)
    {
        addStringFieldWithExtractorAndLookup(
            "status",
            "Status of meter.",
            PrintProperty::JSON | PrintProperty::FIELD | PrintProperty::IMPORTANT | PrintProperty::STATUS,
            FieldMatcher::build()
            .set(DifVifKey("02FF20")),
            {
                {
                    {
                        "ERROR_FLAGS",
                        Translate::Type::BitToString,
                        0x000f,
                        "OK",
                        {
                            { 0x01 , "DRY" },
                            { 0x02 , "REVERSE" },
                            { 0x04 , "LEAK" },
                            { 0x08 , "BURST" },
                        }
                    },
                },
            });

        addNumericFieldWithExtractor(
            "total",
            "The total water consumption recorded by this meter.",
            PrintProperty::JSON | PrintProperty::FIELD | PrintProperty::IMPORTANT,
            Quantity::Volume,
            VifScaling::Auto,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::Volume)
            );

        addNumericFieldWithExtractor(
            "target",
             "The total water consumption recorded at the beginning of this month.",
            PrintProperty::JSON | PrintProperty::FIELD | PrintProperty::IMPORTANT,
            Quantity::Volume,
            VifScaling::Auto,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::Volume)
            .set(StorageNr(1))
            );

        addNumericFieldWithExtractor(
            "flow_temperature",
             "The water temperature.",
            PrintProperty::JSON | PrintProperty::FIELD | PrintProperty::OPTIONAL,
            Quantity::Temperature,
            VifScaling::Auto,
            FieldMatcher::build()
            .set(MeasurementType::Minimum)
            .set(VIFRange::FlowTemperature)
            .set(AnyStorageNr)
            );

        addNumericFieldWithExtractor(
            "external_temperature",
             "The external temperature outside of the meter.",
            PrintProperty::JSON | PrintProperty::FIELD | PrintProperty::OPTIONAL,
            Quantity::Temperature,
            VifScaling::Auto,
            FieldMatcher::build()
            .set(MeasurementType::Any)
            .set(VIFRange::ExternalTemperature)
            .set(AnyStorageNr)
            .add(VIFCombinable::Any)
            );

        addNumericFieldWithExtractor(
            "min_external_temperature",
             "The lowest external temperature outside of the meter.",
            PrintProperty::JSON | PrintProperty::FIELD | PrintProperty::OPTIONAL,
            Quantity::Temperature,
            VifScaling::Auto,
            FieldMatcher::build()
            .set(MeasurementType::Minimum)
            .set(VIFRange::ExternalTemperature)
            );

        addNumericFieldWithExtractor(
            "max_flow",
            "The maxium flow recorded during previous period.",
            PrintProperty::FIELD | PrintProperty::JSON | PrintProperty::OPTIONAL,
            Quantity::Flow,
            VifScaling::Auto,
            FieldMatcher::build()
            .set(MeasurementType::Maximum)
            .set(VIFRange::VolumeFlow)
            .set(AnyStorageNr)
            );

        addStringFieldWithExtractorAndLookup(
            "current_status",
            "Status of meter. This field will go away use status instead.",
            PrintProperty::JSON | PrintProperty::FIELD | PrintProperty::IMPORTANT | PrintProperty::DEPRECATED,
            FieldMatcher::build()
            .set(DifVifKey("02FF20")),
            {
                {
                    {
                        "ERROR_FLAGS",
                        Translate::Type::BitToString,
                        0x000f,
                        "",
                        {
                            { 0x01 , "DRY" },
                            { 0x02 , "REVERSE" },
                            { 0x04 , "LEAK" },
                            { 0x08 , "BURST" },
                        }
                    },
                },
            });

        addStringFieldWithExtractorAndLookup(
            "time_dry",
            "Amount of time the meter has been dry.",
            PrintProperty::JSON,
            FieldMatcher::build()
            .set(DifVifKey("02FF20")),
            {
                {
                    {
                        "DRY",
                        Translate::Type::IndexToString,
                        0x0070,
                        "",
                        {
                            { 0x0000, "" },
                            { 0x0010, "1-8 hours" },
                            { 0x0020, "9-24 hours" },
                            { 0x0030, "2-3 days" },
                            { 0x0040, "4-7 days" },
                            { 0x0050, "8-14 days" },
                            { 0x0060, "15-21 days" },
                            { 0x0070, "22-31 days" },
                        },
                    },
                },
            });

        addStringFieldWithExtractorAndLookup(
            "time_reversed",
            "Amount of time the meter has been reversed.",
            PrintProperty::JSON,
            FieldMatcher::build()
            .set(DifVifKey("02FF20")),
            {
                {
                    {
                        "REVERSED",
                        Translate::Type::IndexToString,
                        0x0380,
                        "",
                        {
                            { 0x0000, "" },
                            { 0x0080, "1-8 hours" },
                            { 0x0100, "9-24 hours" },
                            { 0x0180, "2-3 days" },
                            { 0x0200, "4-7 days" },
                            { 0x0280, "8-14 days" },
                            { 0x0300, "15-21 days" },
                            { 0x0380, "22-31 days" },
                        },
                    },
                },
            });

        addStringFieldWithExtractorAndLookup(
            "time_leaking",
            "Amount of time the meter has been leaking.",
            PrintProperty::JSON,
            FieldMatcher::build()
            .set(DifVifKey("02FF20")),
            {
                {
                    {
                        "LEAKING",
                        Translate::Type::IndexToString,
                        0x1c00,
                        "",
                        {
                            { 0x0000, "" },
                            { 0x0400, "1-8 hours" },
                            { 0x0800, "9-24 hours" },
                            { 0x0c00, "2-3 days" },
                            { 0x1000, "4-7 days" },
                            { 0x1400, "8-14 days" },
                            { 0x1800, "15-21 days" },
                            { 0x1c00, "22-31 days" },
                        },
                    },
                },
            });

        addStringFieldWithExtractorAndLookup(
            "time_bursting",
            "Amount of time the meter has been bursting.",
            PrintProperty::JSON,
            FieldMatcher::build()
            .set(DifVifKey("02FF20")),
            {
                {
                    {
                        "BURSTING",
                        Translate::Type::IndexToString,
                        0xe000,
                        "",
                        {
                            { 0x0000, "" },
                            { 0x2000, "1-8 hours" },
                            { 0x4000, "9-24 hours" },
                            { 0x6000, "2-3 days" },
                            { 0x8000, "4-7 days" },
                            { 0xa000, "8-14 days" },
                            { 0xc000, "15-21 days" },
                            { 0xe000, "22-31 days" },
                        },
                    },
                },
            });

    }
}

// Test: MyTapWater multical21 76348799 NOKEY
// Comment:
// telegram=|2A442D2C998734761B168D2091D37CAC21576C78_02FF207100041308190000441308190000615B7F616713|
// {"media":"cold water","meter":"multical21","name":"MyTapWater","id":"76348799","status":"DRY","total_m3":6.408,"target_m3":6.408,"flow_temperature_c":127,"external_temperature_c":19,"current_status":"DRY","time_dry":"22-31 days","time_reversed":"","time_leaking":"","time_bursting":"","timestamp":"1111-11-11T11:11:11Z"}
// |MyTapWater;76348799;6.408;6.408;null;127;19;DRY;1111-11-11 11:11.11

// telegram=|23442D2C998734761B168D2087D19EAD217F1779EDA86AB6_710008190000081900007F13|
// {"media":"cold water","meter":"multical21","name":"MyTapWater","id":"76348799","status":"DRY","total_m3":6.408,"target_m3":6.408,"flow_temperature_c":127,"external_temperature_c":19,"current_status":"DRY","time_dry":"22-31 days","time_reversed":"","time_leaking":"","time_bursting":"","timestamp":"1111-11-11T11:11:11Z"}
// |MyTapWater;76348799;6.408;6.408;null;127;19;DRY;1111-11-11 11:11.11

// Test: Vadden multical21 44556677 NOKEY
// telegram=|2D442D2C776655441B168D2083B48D3A20_46887802FF20000004132F4E000092013B3D01A1015B028101E7FF0F03|
// {"media":"cold water","meter":"multical21","name":"Vadden","id":"44556677","status":"OK","total_m3":20.015,"target_m3":null,"flow_temperature_c":2,"external_temperature_c":3,"max_flow_m3h":0.317,"current_status":"","time_dry":"","time_reversed":"","time_leaking":"","time_bursting":"","timestamp":"1111-11-11T11:11:11Z"}
// |Vadden;44556677;20.015;null;0.317;2;3;OK;1111-11-11 11:11.11

// telegram=|21442D2C776655441B168D2079CC8C3A20_F4307912C40DFF00002F4E00003D010203|
// {"media":"cold water","meter":"multical21","name":"Vadden","id":"44556677","status":"OK","total_m3":20.015,"target_m3":null,"flow_temperature_c":2,"external_temperature_c":3,"max_flow_m3h":0.317,"current_status":"","time_dry":"","time_reversed":"","time_leaking":"","time_bursting":"","timestamp":"1111-11-11T11:11:11Z"}
// |Vadden;44556677;20.015;null;0.317;2;3;OK;1111-11-11 11:11.11
