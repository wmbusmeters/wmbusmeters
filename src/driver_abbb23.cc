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
        di.setName("abbb23");
        di.setDefaultFields("name,id,total_energy_consumption_kwh,timestamp");
        di.setMeterType(MeterType::ElectricityMeter);
        di.addLinkMode(LinkMode::T1);
        di.addDetection(MANUFACTURER_ABB,  0x02,  0x20);
        di.setConstructor([](MeterInfo& mi, DriverInfo& di){ return shared_ptr<Meter>(new Driver(mi, di)); });
    });

    Driver::Driver(MeterInfo &mi, DriverInfo &di) : MeterCommonImplementation(mi, di)
    {
        addNumericFieldWithExtractor(
            "total_energy_consumption",
            "The total energy consumption recorded by this meter.",
            PrintProperty::JSON | PrintProperty::FIELD | PrintProperty::IMPORTANT,
            Quantity::Energy,
            VifScaling::Auto,
            FieldMatcher::build()
            .set(DifVifKey("0E8400"))
            );

        addStringFieldWithExtractor(
            "firmware_version",
            "Firmware version.",
            PrintProperty::JSON | PrintProperty::OPTIONAL,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::FirmwareVersion)
            .add(VIFCombinable::Any) // Stupid 00 combinable....
            );

        addStringFieldWithExtractor(
            "product_no",
            "The meter device product number.",
            PrintProperty::JSON | PrintProperty::OPTIONAL,
            FieldMatcher::build().
            set(DifVifKey("0DFFAA00")));
    }
}

// Test: ABBmeter abbb23 33221100 NOKEY
// telegram=|844442040011223320027A3E000020_0E840017495200000004FFA0150000000004FFA1150000000004FFA2150000000004FFA3150000000007FFA600000000000000000007FFA700000000000000000007FFA800000000000000000007FFA90000000000000000000DFD8E0007302E38322E31420DFFAA000B3030312D313131203332421F|
// {"media":"electricity","meter":"abbb23","name":"ABBmeter","id":"33221100","total_energy_consumption_kwh":5249.17,"firmware_version": "B1.28.0","product_no": "B23 111-100","timestamp":"1111-11-11T11:11:11Z"}
// |ABBmeter;33221100;5249.17;1111-11-11 11:11.11
