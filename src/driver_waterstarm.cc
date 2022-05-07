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
        di.setName("waterstarm");
        di.setMeterType(MeterType::WaterMeter);
        di.addLinkMode(LinkMode::T1);
        di.addLinkMode(LinkMode::C1);
        di.addDetection(MANUFACTURER_DWZ,  0x06,  0x02);
        di.addDetection(MANUFACTURER_DWZ,  0x07,  0x02);
        di.addDetection(MANUFACTURER_EFE,  0x07,  0x03);
        di.setConstructor([](MeterInfo& mi, DriverInfo& di){ return shared_ptr<Meter>(new Driver(mi, di)); });
    });

    Driver::Driver(MeterInfo &mi, DriverInfo &di) : MeterCommonImplementation(mi, di)
    {
        addStringFieldWithExtractor(
            "meter_timestamp",
            "Device date time.",
            PrintProperty::JSON,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::DateTime)
            );

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
            "total_backwards",
            "The total backward water volume recorded by this meter.",
            PrintProperty::JSON | PrintProperty::FIELD,
            Quantity::Volume,
            VifScaling::Auto,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::AnyVolumeVIF)
            .add(VIFCombinable::BackwardFlow)
            );

        addStringFieldWithExtractorAndLookup(
            "current_status",
            "Status and error flags.",
            PrintProperty::JSON | PrintProperty::FIELD,
            FieldMatcher::build()
            .set(VIFRange::ErrorFlags),
            {
                {
                    {
                        "ERROR_FLAGS",
                        Translate::Type::BitToString,
                        0xffff,
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
            PrintProperty::JSON | PrintProperty::OPTIONAL,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::ModelVersion)
            );

        addStringFieldWithExtractor(
            "parameter_set",
            "Parameter set.",
            PrintProperty::JSON | PrintProperty::OPTIONAL,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::ParameterSet)
            );
    }
}
