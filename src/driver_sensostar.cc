/*
 Copyright (C) 2020-2022 Patrick Schwarz (gpl-3.0-or-later)

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
        di.setName("sensostar");
        di.setMeterType(MeterType::HeatMeter);
        di.addLinkMode(LinkMode::C1);
        di.addLinkMode(LinkMode::T1);
        di.addDetection(MANUFACTURER_EFE, 0x04,  0x00);
        di.setConstructor([](MeterInfo& mi, DriverInfo& di){ return shared_ptr<Meter>(new Driver(mi, di)); });
    });

    Driver::Driver(MeterInfo &mi, DriverInfo &di) : MeterCommonImplementation(mi, di)
    {
        addStringFieldWithExtractor(
            "meter_timestamp",
            "Date time for this reading.",
            PrintProperty::JSON,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::DateTime)
            );

        addNumericFieldWithExtractor(
            "total",
            "The total energy consumption recorded by this meter.",
            PrintProperty::JSON | PrintProperty::FIELD | PrintProperty::IMPORTANT,
            Quantity::Energy,
            VifScaling::Auto,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::AnyEnergyVIF)
            );

        addNumericFieldWithExtractor(
            "power",
            "The active power consumption.",
            PrintProperty::JSON | PrintProperty::OPTIONAL,
            Quantity::Power,
            VifScaling::Auto,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::AnyPowerVIF)
            );

        addNumericFieldWithExtractor(
            "power_max",
            "The maximum power consumption over ?period?.",
            PrintProperty::JSON | PrintProperty::OPTIONAL,
            Quantity::Power,
            VifScaling::Auto,
            FieldMatcher::build()
            .set(MeasurementType::Maximum)
            .set(VIFRange::AnyPowerVIF)
            );

        addNumericFieldWithExtractor(
            "flow_water",
            "The flow of water.",
            PrintProperty::JSON | PrintProperty::OPTIONAL,
            Quantity::Flow,
            VifScaling::Auto,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::VolumeFlow)
            );

        addNumericFieldWithExtractor(
            "flow_water_max",
            "The maximum forward flow of water over a ?period?.",
            PrintProperty::JSON | PrintProperty::OPTIONAL,
            Quantity::Flow,
            VifScaling::Auto,
            FieldMatcher::build()
            .set(MeasurementType::Maximum)
            .set(VIFRange::VolumeFlow)
            );

        addNumericFieldWithExtractor(
            "forward",
            "The forward temperature of the water.",
            PrintProperty::JSON | PrintProperty::OPTIONAL,
            Quantity::Temperature,
            VifScaling::Auto,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::FlowTemperature)
            );

        addNumericFieldWithExtractor(
            "return",
            "The return temperature of the water.",
            PrintProperty::JSON | PrintProperty::OPTIONAL,
            Quantity::Temperature,
            VifScaling::Auto,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::ReturnTemperature)
            );

        addNumericFieldWithExtractor(
            "difference",
            "The temperature difference forward-return for the water.",
            PrintProperty::JSON | PrintProperty::OPTIONAL,
            Quantity::Temperature,
            VifScaling::AutoSigned,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::TemperatureDifference)
            );

        addNumericFieldWithExtractor(
            "total_water",
            "The total amount of water that has passed through this meter.",
            PrintProperty::JSON | PrintProperty::FIELD,
            Quantity::Volume,
            VifScaling::Auto,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::Volume)
            );

        addStringFieldWithExtractorAndLookup(
            "current_status",
            "Status and error flags.",
            PrintProperty::JSON | PrintProperty::FIELD,
            FieldMatcher::build()
            .set(DifVifKey("01FD17")),
            {
                {
                    {
                        "ERROR_FLAGS",
                        Translate::Type::BitToString,
                        0xff,
                        "OK",
                        {
                            /* What are the bits? If a bit is known then add for example:
                               { 0x01, "OVERHEAT" }
                            */
                        }
                    },
                },
            });
    }
}

// Test: Heat sensostar 20480057 NOKEY
// Comment:
// telegram=|68B3B36808007257004820c51400046c100000047839803801040600000000041300000000042B00000000142B00000000043B00000000143B00000000025B1400025f15000261daff02235c00046d2c2ddc24440600000000441300000000426c000001fd171003fd0c05000084200600000000c420060000000084300600000000c430060000000084401300000000c44013000000008480401300000000c48040130000000084c0401300000000c4c0401300000000a216|
// {"media":"heat","meter":"sensostar","name":"Heat","id":"20480057","meter_timestamp":"2022-04-28 13:44","total_kwh":0,"power_kw":0,"power_max_kw":0,"flow_water_m3h":0,"flow_water_max_m3h":0,"forward_c":20,"return_c":21,"difference_c":-0.38,"total_water_m3":0,"current_status":"UNKNOWN_ERROR_FLAGS(0x10)","timestamp":"1111-11-11T11:11:11Z"}
// |Heat;20480057;0.000000;0.000000;UNKNOWN_ERROR_FLAGS(0x10);1111-11-11 11:11.11
