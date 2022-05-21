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
            PrintProperty::JSON | PrintProperty::FIELD | JOIN_TPL_STATUS,
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

// Test: Woter waterstarm 20096221 BEDB81B52C29B5C143388CBB0D15A051
// telegram=|3944FA122162092002067A3600202567C94D48D00DC47B11213E23383DB51968A705AAFA60C60E263D50CD259D7C9A03FD0C08000002FD0B0011|
// {"media":"warm water","meter":"waterstarm","name":"Woter","id":"20096221","meter_timestamp":"2020-07-30 10:40","total_m3":0.106,"total_backwards_m3":0,"current_status":"OK","meter_version":"000008","parameter_set":"1100","timestamp":"1111-11-11T11:11:11Z"}
// |Woter;20096221;0.106000;0.000000;OK;1111-11-11 11:11.11

// telegram=|3944FA122162092002067A3604202567C94D48D00DC47B11213E23383DB51968A705AAFA60C60E263D50CD259D7C9A03FD0C08000002FD0B0011|
// {"media":"warm water","meter":"waterstarm","name":"Woter","id":"20096221","meter_timestamp":"2020-07-30 10:40","total_m3":0.106,"total_backwards_m3":0,"current_status":"POWER_LOW","meter_version":"000008","parameter_set":"1100","timestamp":"1111-11-11T11:11:11Z"}
// |Woter;20096221;0.106000;0.000000;POWER_LOW;1111-11-11 11:11.11

// Test: Water waterstarm 22996221 NOKEY
// telegram=|3944FA122162992202067A360420252F2F_046D282A9E2704136A00000002FD17400004933C000000002F2F2F2F2F2F03FD0C08000002FD0B0011|
// {"media":"warm water","meter":"waterstarm","name":"Water","id":"22996221","meter_timestamp":"2020-07-30 10:40","total_m3":0.106,"total_backwards_m3":0,"current_status":"LEAKAGE_OR_NO_USAGE POWER_LOW","meter_version":"000008","parameter_set":"1100","timestamp":"1111-11-11T11:11:11Z"}
// |Water;22996221;0.106000;0.000000;LEAKAGE_OR_NO_USAGE POWER_LOW;1111-11-11 11:11.11
