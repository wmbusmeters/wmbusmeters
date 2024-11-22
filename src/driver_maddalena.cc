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
        di.setName("maddalena");
        di.setDefaultFields("name,id,total_m3,total_backwards_m3,status,timestamp");
        di.setMeterType(MeterType::WaterMeter);
        di.addLinkMode(LinkMode::T1);
        di.addLinkMode(LinkMode::C1);
        di.addDetection(MANUFACTURER_MAD,  0x06,  0x01);    // warm water
        di.addDetection(MANUFACTURER_MAD,  0x07,  0x01);    // water meter

        di.setConstructor([](MeterInfo& mi, DriverInfo& di){ return shared_ptr<Meter>(new Driver(mi, di)); });
    });

    Driver::Driver(MeterInfo &mi, DriverInfo &di) : MeterCommonImplementation(mi, di)
    {
        addStringFieldWithExtractorAndLookup(
            "status",
            "Status and error flags.",
            DEFAULT_PRINT_PROPERTIES | PrintProperty::INCLUDE_TPL_STATUS | PrintProperty::STATUS,
            FieldMatcher::build()
            .set(VIFRange::ErrorFlags),
            {
                {
                    {
                        "ERROR_FLAGS",
                        Translate::MapType::BitToString,
                        AlwaysTrigger, MaskBits(0xffff),
                        "OK",
                        {
                            { 0x01, "SW_ERROR" },
                            { 0x02, "CRC_ERROR" },
                            { 0x04, "SENSOR_ERROR" },
                            { 0x08, "MEASUREMENT_ERROR" },
                            { 0x10, "BATTERY_VOLTAGE_ERROR" },
                            { 0x20, "MANIPULATION" },
                            { 0x40, "LEAKAGE_OR_NO_USAGE" },
                            { 0x80, "REVERSE_FLOW" },
                            { 0x100, "OVERLOAD" },
                        }
                    },
                },
            });

        addNumericFieldWithExtractor(
            "meter",
            "Device date time.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::PointInTime,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::DateTime)
            );

        addNumericFieldWithExtractor(
            "total",
            "The total water consumption recorded by this meter.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Volume,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::Volume)
            );

        addNumericFieldWithExtractor(
            "total_backwards",
            "The total backward water volume recorded by this meter.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Volume,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::AnyVolumeVIF)
            .add(VIFCombinable::BackwardFlow)
            );

        addStringFieldWithExtractorAndLookup(
            "current_status",
            "Status and error flags. (Deprecated use status instead.)",
            DEFAULT_PRINT_PROPERTIES | PrintProperty::INCLUDE_TPL_STATUS | PrintProperty::STATUS | PrintProperty::DEPRECATED,
            FieldMatcher::build()
            .set(VIFRange::ErrorFlags),
            {
                {
                    {
                        "ERROR_FLAGS",
                        Translate::MapType::BitToString,
                        AlwaysTrigger, MaskBits(0xffff),
                        "OK",
                        {
                            { 0x01, "SW_ERROR" },
                            { 0x02, "CRC_ERROR" },
                            { 0x04, "SENSOR_ERROR" },
                            { 0x08, "MEASUREMENT_ERROR" },
                            { 0x10, "BATTERY_VOLTAGE_ERROR" },
                            { 0x20, "MANIPULATION" },
                            { 0x40, "LEAKAGE_OR_NO_USAGE" },
                            { 0x80, "REVERSE_FLOW" },
                            { 0x100, "OVERLOAD" },
                        }
                    },
                },
            });

        addStringFieldWithExtractor(
            "meter_version",
            "Meter model/version.",
            DEFAULT_PRINT_PROPERTIES,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::ModelVersion)
            );

        addStringFieldWithExtractor(
            "parameter_set",
            "Parameter set.",
            DEFAULT_PRINT_PROPERTIES,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::ParameterSet)
            );

        addNumericFieldWithExtractor(
            "battery",
            "The battery voltage.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Voltage,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::Voltage)
            );

        addNumericFieldWithExtractor(
            "set",
            "The most recent billing period date.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::PointInTime,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::Date)
            .set(StorageNr(1)),
            Unit::DateLT
            );

        addNumericFieldWithExtractor(
            "consumption_at_set_date",
            "The total water consumption at the most recent billing period date.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Volume,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::Volume)
            .set(StorageNr(1))
            );

        addNumericFieldWithExtractor(
            "consumption_at_history_{storage_counter - 1 counter}",
            "The total water consumption at the historic date.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Volume,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::Volume)
            .set(StorageNr(2),StorageNr(16))
            );

        addNumericFieldWithCalculatorAndMatcher(
            "history_{storage_counter - 1 counter}",
            "The historic date.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::PointInTime,
            "meter_datetime - ((storage_counter-1counter) * 1 month)",
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::Volume)
            .set(StorageNr(2),StorageNr(16)),
            Unit::DateLT
            );
    }
}

// Test: Water maddalena 24018699 NOKEY
// telegram=|4E4424349986012401077AF2000020_2F2F0413A7000000046D0E0C163B04FD17000000000E789986012401FF441300000000426C01018401134A00000082016C1F3AD3013B470500C4016D1B14153B|
// {"media":"water","meter":"maddalena","name":"water","id":"24018699","consumption_at_history_1_m3":0.074,"consumption_at_set_date_m3":0,"history_1_date":"2024-10-22","meter_datetime":"2024-11-22 12:14","set_date":"2000-01-01","total_m3":0.167,"current_status":"OK","status":"OK","timestamp":"2024-11-22T12:14:40Z","device":"rtlwmbus[00000001]","rssi_dbm":144}