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
        di.setName("kampress");
        di.setMeterType(MeterType::PressureSensor);
        di.addLinkMode(LinkMode::C1);
        di.addDetection(MANUFACTURER_KAM,  0x18,  0x01);
        di.setConstructor([](MeterInfo& mi, DriverInfo& di){ return shared_ptr<Meter>(new Driver(mi, di)); });
    });

    Driver::Driver(MeterInfo &mi, DriverInfo &di) : MeterCommonImplementation(mi, di)
    {
        addNumericFieldWithExtractor(
            "pressure",
            "The measured pressure.",
            PrintProperty::JSON | PrintProperty::FIELD | PrintProperty::IMPORTANT,
            Quantity::Pressure,
            VifScaling::Auto,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::Pressure)
            );

        addNumericFieldWithExtractor(
            "max_pressure",
            "The maximum pressure measured during ?.",
            PrintProperty::JSON | PrintProperty::FIELD | PrintProperty::IMPORTANT,
            Quantity::Pressure,
            VifScaling::Auto,
            FieldMatcher::build()
            .set(MeasurementType::Maximum)
            .set(VIFRange::Pressure)
            );

        addNumericFieldWithExtractor(
            "min_pressure",
            "The minumum pressure measured during ?.",
            PrintProperty::JSON | PrintProperty::FIELD | PrintProperty::IMPORTANT,
            Quantity::Pressure,
            VifScaling::Auto,
            FieldMatcher::build()
            .set(MeasurementType::Minimum)
            .set(VIFRange::Pressure)
            );

        addStringFieldWithExtractorAndLookup(
            "status",
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
//                            { 0x01, "?" },
                        }
                    },
                },
            });
    }
}

// Test: Pressing kampress 77000317 NOKEY
// telegram=|32442D2C1703007701188D280080E39322DB8F78_22696600126967000269660005FF091954A33A05FF0A99BD823A02FD170800|
// {"media":"pressure","meter":"kampress","name":"Pressing","id":"77000317","pressure_bar":1.02,"max_pressure_bar":1.03,"min_pressure_bar":1.02,"status":"ERROR_FLAGS_8","timestamp":"1111-11-11T11:11:11Z"}
// |Pressing;77000317;1.020000;1.030000;1.020000;ERROR_FLAGS_8;1111-11-11 11:11.11


// telegram=|27442D2C1703007701188D280194E393226EC679DE735657_660067006600962B913A21B9423A0800|
// {"media":"pressure","meter":"kampress","name":"Pressing","id":"77000317","pressure_bar":1.02,"max_pressure_bar":1.03,"min_pressure_bar":1.02,"status":"ERROR_FLAGS_8","timestamp":"1111-11-11T11:11:11Z"}
// |Pressing;77000317;1.020000;1.030000;1.020000;ERROR_FLAGS_8;1111-11-11 11:11.11
