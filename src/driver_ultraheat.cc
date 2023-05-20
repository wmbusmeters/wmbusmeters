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
        di.setName("ultraheat");
        di.setDefaultFields("name,id,heat_kwh,timestamp");
        di.setMeterType(MeterType::HeatMeter);
        di.addDetection(MANUFACTURER_LUG, 0x04,  0x04);
        di.setConstructor([](MeterInfo& mi, DriverInfo& di){ return shared_ptr<Meter>(new Driver(mi, di)); });
    });

    Driver::Driver(MeterInfo &mi, DriverInfo &di) : MeterCommonImplementation(mi, di)
    {
        addNumericFieldWithExtractor(
            "heat",
            "The total heat energy consumption recorded by this meter.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Energy,
            VifScaling::Auto,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::AnyEnergyVIF)
            );

        addNumericFieldWithExtractor(
            "volume",
            "The total heating media volume recorded by this meter.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Volume,
            VifScaling::Auto,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::Volume)
            );

        addNumericFieldWithExtractor(
            "power",
            "The current power consumption.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Power,
            VifScaling::AutoSigned,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::PowerW)
            );

        addNumericFieldWithExtractor(
            "flow",
            "The current heat media volume flow.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Flow,
            VifScaling::AutoSigned,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::VolumeFlow)
            );

        addNumericFieldWithExtractor(
            "flow",
            "The current forward heat media temperature.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Temperature,
            VifScaling::Auto,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::FlowTemperature)
            );

        addNumericFieldWithExtractor(
            "return",
            "The current return heat media temperature.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Temperature,
            VifScaling::Auto,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::ReturnTemperature)
            );
    }
}

// Test: MyUltra ultraheat 70444600 NOKEY
// telegram=|68F8F86808007200464470A7320404270000000974040970040C0E082303000C14079519000B2D0500000B3B0808000A5B52000A5F51000A6206004C14061818004C0E490603000C7800464470891071609B102D020100DB102D0201009B103B6009009A105B78009A105F74000C22726701003C22000000007C2200000000426C01018C2006000000008C3006000000008C80100600000000CC200600000000CC300600000000CC801006000000009A115B64009A115F63009B113B5208009B112D020100BC0122000000008C010E490603008C2106000000008C3106000000008C811006000000008C011406181800046D310ACA210F21040010A0C116|
// {"media":"heat","meter":"ultraheat","name":"MyUltra","id":"70444600","heat_kwh":8974.444444,"volume_m3":1995.07,"power_kw":0.5,"flow_m3h":0.808,"flow_c":52,"return_c":51,"timestamp":"1111-11-11T11:11:11Z"}
// |MyUltra;70444600;8974.444444;1111-11-11 11:11.11
