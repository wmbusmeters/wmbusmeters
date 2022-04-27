/*
 Copyright (C) 2021-2022 Fredrik Öhrström (gpl-3.0-or-later)

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
        di.setName("piigth");
        di.setMeterType(MeterType::TempHygroMeter);
        di.addLinkMode(LinkMode::MBUS);
        di.setConstructor([](MeterInfo& mi, DriverInfo& di){ return shared_ptr<Meter>(new Driver(mi, di)); });
        di.addDetection(MANUFACTURER_PII,  0x1b,  0x01);
    });

    Driver::Driver(MeterInfo &mi, DriverInfo &di) : MeterCommonImplementation(mi, di)
    {
        addNumericFieldWithExtractor(
            "temperature",
            "The current temperature.",
            PrintProperty::JSON | PrintProperty::FIELD,
            Quantity::Temperature,
            VifScaling::Auto,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::ExternalTemperature)
            );

        addNumericFieldWithExtractor(
            "average_temperature_1h",
            "The average temperature over the last hour.",
            PrintProperty::JSON | PrintProperty::FIELD,
            Quantity::Temperature,
            VifScaling::Auto,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::ExternalTemperature)
            );

        addNumericFieldWithExtractor(
            "average_temperature_24h",
            "The average temperature over the last 24 hours.",
            PrintProperty::JSON | PrintProperty::FIELD,
            Quantity::Temperature,
            VifScaling::Auto,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::ExternalTemperature)
            .set(StorageNr(2))
            );

        /*
        addNumericFieldWithExtractor(
            "relative_humidity",
            "The current relative humidity.",
            PrintProperty::JSON | PrintProperty::FIELD,
            Quantity::RelativeHumidity,
            VifScaling::Auto,
            FieldMatcher::build().
            set(DifVifKey("02FB1A"))
            );
        */
        addStringFieldWithExtractor(
            "fabrication_no",
            "Fabrication number.",
            PrintProperty::JSON,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::FabricationNo)
            );
    }
}
