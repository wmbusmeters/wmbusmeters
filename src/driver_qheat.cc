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
        di.setName("qheat");
        di.setMeterType(MeterType::HeatMeter);
        di.addLinkMode(LinkMode::C1);
        di.addDetection(MANUFACTURER_QDS, 0x04,  0x23);
        di.setConstructor([](MeterInfo& mi, DriverInfo& di){ return shared_ptr<Meter>(new Driver(mi, di)); });
    });

    Driver::Driver(MeterInfo &mi, DriverInfo &di) :  MeterCommonImplementation(mi, di)
    {
        addNumericFieldWithExtractor(
            "total_energy_consumption",
            "The total energy consumption recorded by this meter.",
            PrintProperty::FIELD | PrintProperty::JSON | PrintProperty::IMPORTANT,
            Quantity::Energy,
            VifScaling::Auto,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::AnyEnergyVIF)
            );

        addStringFieldWithExtractor(
            "last_month_date",
             "Last day previous month when total energy consumption was recorded.",
            PrintProperty::FIELD | PrintProperty::JSON | PrintProperty::IMPORTANT,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(StorageNr(17))
            .set(VIFRange::Date)
            );

        addNumericFieldWithExtractor(
            "last_month_energy_consumption",
            "The total energy consumption recorded at the last day of the previous month.",
            PrintProperty::FIELD | PrintProperty::JSON,
            Quantity::Energy,
            VifScaling::Auto,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(StorageNr(17))
            .set(VIFRange::AnyEnergyVIF)
            );

        addStringFieldWithExtractor(
            "last_year_date",
             "Last day previous year when total energy consumption was recorded.",
            PrintProperty::JSON,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(StorageNr(1))
            .set(VIFRange::Date)
            );

        addNumericFieldWithExtractor(
            "last_year_energy_consumption",
            "The total energy consumption recorded at the last day of the previous year.",
            PrintProperty::JSON,
            Quantity::Energy,
            VifScaling::Auto,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(StorageNr(1))
            .set(VIFRange::AnyEnergyVIF)
            );

        addStringFieldWithExtractor(
            "device_date_time",
             "Device date time.",
            PrintProperty::JSON,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::DateTime)
            );

        addStringFieldWithExtractor(
            "device_error_date",
             "Device error date.",
            PrintProperty::JSON,
            FieldMatcher::build()
            .set(MeasurementType::AtError)
            .set(VIFRange::Date)
            );
    }
}

// Test: QHeato qheat 67228058 NOKEY
// telegram=|3C449344957002372337725880226793442304DC0000200C05043900004C0500000000426C9F2CCC080551070000C2086CBE29326CFFFF046D280DB62A|
// {"media":"heat","meter":"qheat","name":"QHeato","id":"67228058","total_energy_consumption_kwh":390.4,"last_month_date":"2021-09-30","last_month_energy_consumption_kwh":75.1,"last_year_date":"2020-12-31","last_year_energy_consumption_kwh":0,"device_date_time":"2021-10-22 13:40","device_error_date":"2127-15-31","timestamp":"1111-11-11T11:11:11Z"}
// |QHeato;67228058;390.400000;2021-09-30;75.100000;1111-11-11 11:11.11
