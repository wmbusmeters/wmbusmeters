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
