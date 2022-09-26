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
        di.setName("munia");
        di.setDefaultFields("name,id,current_temperature_c,current_relative_humidity_rh,timestamp");
        di.setMeterType(MeterType::TempHygroMeter);
        di.addLinkMode(LinkMode::MBUS);
        di.setConstructor([](MeterInfo& mi, DriverInfo& di){ return shared_ptr<Meter>(new Driver(mi, di)); });
        di.addDetection(MANUFACTURER_WEP, 0x1b,  0x02);
    });

    Driver::Driver(MeterInfo &mi, DriverInfo &di) : MeterCommonImplementation(mi, di)
    {
        addOptionalCommonFields();

        addStringFieldWithExtractorAndLookup(
            "status",
            "Meter status. Reports OK if neither tpl sts nor error flags have bits set.",
            PrintProperty::JSON | PrintProperty::FIELD | PrintProperty::IMPORTANT |
            PrintProperty::STATUS | PrintProperty::JOIN_TPL_STATUS,
            FieldMatcher::build()
            .set(DifVifKey("02FD971D")),
            Translate::Lookup({
                    {
                        {
                            "ERROR_FLAGS",
                            Translate::Type::BitToString,
                            0xffff,
                            "OK",
                            {
                                // What does these bits mean?
                            }
                        },
                    },
                }));

        /*
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::ErrorFlags)
            .add(VIFCombinable::StandardConformantDataContent)
*/
        addNumericFieldWithExtractor(
            "current_temperature",
            "The current temperature.",
            PrintProperty::JSON | PrintProperty::FIELD,
            Quantity::Temperature,
            VifScaling::Auto,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::ExternalTemperature)
            );

        addNumericFieldWithExtractor(
            "current_relative_humidity",
            "The current relative humidity.",
            PrintProperty::JSON | PrintProperty::FIELD,
            Quantity::RelativeHumidity,
            VifScaling::Auto,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::RelativeHumidity)
            );
    }
}

// Test: TempoHygro munia 00013482 NOKEY
// telegram=|2E44B05C82340100021B7A460000002F2F0A6601020AFB1A570602FD971D00002F2F2F2F2F2F2F2F2F2F2F2F2F2F2F|
// {"media":"room sensor","meter":"munia","name":"TempoHygro","id":"00013482","status":"OK","current_temperature_c":20.1,"current_relative_humidity_rh":65.7,"timestamp":"1111-11-11T11:11:11Z"}
// |TempoHygro;00013482;20.1;65.7;1111-11-11 11:11.11
