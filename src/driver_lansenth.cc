/*
 Copyright (C) 2019-2022 Fredrik Öhrström (gpl-3.0-or-later)

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
        di.setName("lansenth");
        di.setDefaultFields("name,id,current_temperature_c,current_relative_humidity_rh,timestamp");
        di.setMeterType(MeterType::TempHygroMeter);
        di.addDetection(MANUFACTURER_LAS,  0x1b,  0x07);
        di.addMfctTPLStatusBits(
            {
                {
                    {
                        "TPL_STS",
                        Translate::Type::BitToString,
                        AlwaysTrigger, MaskBits(0xe0), // Always use 0xe0 for tpl mfct status bits.
                        "OK",
                        {
                            { 0x40, "SABOTAGE_ENCLOSURE" }
                        }
                    },
                },
            });
        di.setConstructor([](MeterInfo& mi, DriverInfo& di){ return shared_ptr<Meter>(new Driver(mi, di)); });
    });

    Driver::Driver(MeterInfo &mi, DriverInfo &di) : MeterCommonImplementation(mi, di)
    {
        addStringField(
            "status",
            "Meter status from tpl status field.",
            PrintProperty::JSON | PrintProperty::FIELD | PrintProperty::IMPORTANT |
            PrintProperty::STATUS | PrintProperty::JOIN_TPL_STATUS);

        addNumericFieldWithExtractor(
            "current_temperature",
             "The current temperature.",
            PrintProperty::FIELD | PrintProperty::JSON | PrintProperty::IMPORTANT,
            Quantity::Temperature,
            VifScaling::Auto,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::ExternalTemperature)
            );

        addNumericFieldWithExtractor(
            "current_relative_humidity",
             "The current humidity.",
            PrintProperty::FIELD | PrintProperty::JSON | PrintProperty::IMPORTANT,
            Quantity::RelativeHumidity,
            VifScaling::Auto,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::RelativeHumidity)
            );

        addNumericFieldWithExtractor(
            "average_temperature_1h",
             "The average temperature over the last hour.",
            PrintProperty::FIELD | PrintProperty::JSON,
            Quantity::Temperature,
            VifScaling::Auto,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::ExternalTemperature)
            .set(StorageNr(1))
            );

        addNumericFieldWithExtractor(
            "average_relative_humidity_1h",
             "The average humidity over the last hour.",
            PrintProperty::FIELD | PrintProperty::JSON,
            Quantity::RelativeHumidity,
            VifScaling::Auto,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::RelativeHumidity)
            .set(StorageNr(1))
            );

        addNumericFieldWithExtractor(
            "average_temperature_24h",
             "The average temperature over the last 24 hours.",
            PrintProperty::FIELD | PrintProperty::JSON,
            Quantity::Temperature,
            VifScaling::Auto,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::ExternalTemperature)
            .set(StorageNr(2))
            );

        addNumericFieldWithExtractor(
            "average_relative_humidity_24h",
             "The average humidity over the last 24 hours.",
            PrintProperty::FIELD | PrintProperty::JSON,
            Quantity::RelativeHumidity,
            VifScaling::Auto,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::RelativeHumidity)
            .set(StorageNr(2))
            );

    }
}


// Test: Tempoo lansenth 00010203 NOKEY
// telegram=|2e44333003020100071b7a634820252f2f0265840842658308820165950802fb1aae0142fb1aae018201fb1aa9012f|
// {"media":"room sensor","meter":"lansenth","name":"Tempoo","id":"00010203","status":"PERMANENT_ERROR SABOTAGE_ENCLOSURE","current_temperature_c":21.8,"current_relative_humidity_rh":43,"average_temperature_1h_c":21.79,"average_relative_humidity_1h_rh":43,"average_temperature_24h_c":21.97,"average_relative_humidity_24h_rh":42.5,"timestamp":"1111-11-11T11:11:11Z"}
// |Tempoo;00010203;21.8;43;1111-11-11 11:11.11
