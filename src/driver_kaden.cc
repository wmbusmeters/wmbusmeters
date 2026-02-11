/*
 Copyright (C) 2024 Fredrik Öhrström (gpl-3.0-or-later)

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
    private:
        void processContent(Telegram *t);
    };

    static bool ok = staticRegisterDriver([](DriverInfo&di)
    {
        di.setName("kaden");
        di.setDefaultFields("name,id,current_consumption_hca,set_date,consumption_at_set_date_hca,status,timestamp");
        di.setMeterType(MeterType::HeatCostAllocationMeter);
        di.addLinkMode(LinkMode::T1);
        di.addMVT(MANUFACTURER_VIP, 0x08, 0x1E);
        di.usesProcessContent();
        di.setConstructor([](MeterInfo& mi, DriverInfo& di){ return shared_ptr<Meter>(new Driver(mi, di)); });
    });

    Driver::Driver(MeterInfo &mi, DriverInfo &di) : MeterCommonImplementation(mi, di)
    {
        addStringFieldWithExtractorAndLookup(
            "status",
            "Meter status from error flags and tpl status field.",
            DEFAULT_PRINT_PROPERTIES |
            PrintProperty::STATUS | PrintProperty::INCLUDE_TPL_STATUS,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::ErrorFlags),
            Translate::Lookup(
            {
                {
                    {
                        "ERROR_FLAGS",
                        Translate::MapType::BitToString,
                        AlwaysTrigger, MaskBits(0xffff),
                        "OK",
                        {
                            { 0x0001, "VOLTAGE_INTERRUPTED" },
                            { 0x0004, "SENSOR_T2_OUTSIDE_MEASURING_RANGE" },
                            { 0x0008, "SENSOR_T1_OUTSIDE_MEASURING_RANGE" },
                            { 0x0020, "SENSOR_T3_OUTSIDE_MEASURING_RANGE" },
                        }
                    },
                },
            }));

        addStringFieldWithExtractor(
            "fabrication_no",
            "Fabrication number.",
            DEFAULT_PRINT_PROPERTIES,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::FabricationNo)
            );

        addNumericFieldWithExtractor(
            "current_consumption",
            "The current heat cost allocation.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::HCA,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::HeatCostAllocation)
            );

        addNumericFieldWithExtractor(
            "actuality_duration",
            "Current time without measurement in winter period.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Time,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::ActualityDuration),
            Unit::Minute
            );

        addStringFieldWithExtractor(
            "meter_datetime",
            "Date and time from meter.",
            DEFAULT_PRINT_PROPERTIES,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::DateTime)
            );

        addStringFieldWithExtractor(
            "set_date",
            "The most recent billing period date.",
            DEFAULT_PRINT_PROPERTIES,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::Date)
            .set(StorageNr(1))
            );

        addNumericFieldWithExtractor(
            "consumption_at_set_date",
            "Heat cost allocation at the most recent billing period date.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::HCA,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::HeatCostAllocation)
            .set(StorageNr(1))
            );

        addNumericFieldWithExtractor(
            "actuality_duration_at_set_date",
            "Previous time without measurement in winter period.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Time,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::ActualityDuration)
            .set(StorageNr(1)),
            Unit::Minute
            );

        addNumericFieldWithExtractor(
            "room_temperature",
            "Corrected room temperature.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Temperature,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::ExternalTemperature)
            );

        addNumericFieldWithExtractor(
            "radiator_temperature",
            "Radiator surface temperature.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Temperature,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::FlowTemperature)
            );

        addNumericFieldWithExtractor(
            "tampering_duration",
            "Total time the device has been removed from the radiator.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Time,
            VifScaling::None, DifSignedness::Signed,
            FieldMatcher::build()
            .set(DifVifKey("0474")),
            Unit::Second
            );
    }

    void Driver::processContent(Telegram *t)
    {
        // Reformat set_date: YYYY-MM-DD → DD-MM (billing day, year is sentinel)
        if (string_values_.count("set_date"))
        {
            string v = string_values_["set_date"].value;
            if (v.length() == 10)
            {
                setStringValue("set_date", v.substr(8, 2) + "-" + v.substr(5, 2));
            }
        }
    }
}

// Test: KadenD10 kaden 23800604 82B0551191F51D66EFCDAB8967452301
// telegram=|4e443059040680231e087ac40040050e6aa476257c0c3adae8277edc999b39b38222fcb387a91e94cb6ed47ceec6470f5f686f89a8574415fa262bd43c88f7f153ce3c66e8e44da338a06c62ab21b1|
// {"_":"telegram","media":"heat cost allocation","meter":"kaden","name":"KadenD10","id":"23800604","actuality_duration_min":56401,"actuality_duration_at_set_date_min":305280,"consumption_at_set_date_hca":3,"current_consumption_hca":0,"radiator_temperature_c":19.7,"room_temperature_c":19.6,"tampering_duration_s":13,"fabrication_no":"23800604","meter_datetime":"2026-02-09 04:01","set_date":"01-01","status":"SENSOR_T1_OUTSIDE_MEASURING_RANGE","timestamp":"1111-11-11T11:11:11Z"}
// |KadenD10;23800604;0;01-01;3;SENSOR_T1_OUTSIDE_MEASURING_RANGE;1111-11-11 11:11.11

// Test: KadenC10 kaden 23701267 82B0551191F51D66EFCDAB8967452301
// telegram=|4e443059671270231e087a6f0040051183e178afb952391c01f78104bc3fd33a5232ba7e70f514a062dac99059a7c74a55227dfae9d9590145f685f4ae6a62288bbba6eaf92d86797254644a2cdf46|
// {"_":"telegram","media":"heat cost allocation","meter":"kaden","name":"KadenC10","id":"23701267","actuality_duration_min":136,"actuality_duration_at_set_date_min":74069,"consumption_at_set_date_hca":2264,"current_consumption_hca":859,"radiator_temperature_c":39.3,"room_temperature_c":32,"tampering_duration_s":0,"fabrication_no":"23701267","meter_datetime":"2026-02-09 09:41","set_date":"01-01","status":"OK","timestamp":"1111-11-11T11:11:11Z"}
// |KadenC10;23701267;859;01-01;2264;OK;1111-11-11 11:11.11
