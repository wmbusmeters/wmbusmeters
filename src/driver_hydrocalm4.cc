/*
 Copyright (C) 2018-2022 Fredrik Öhrström (gpl-3.0-or-later)

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
        di.setName("hydrocalm4");
        di.setDefaultFields("name,id,total_heating_kwh,total_cooling_kwh,timestamp");
        di.setMeterType(MeterType::HeatCoolingMeter);
        di.addLinkMode(LinkMode::T1);
        di.addDetection(MANUFACTURER_BMT, 0x0d,  0x1a); 
        di.setConstructor([](MeterInfo& mi, DriverInfo& di){ return shared_ptr<Meter>(new Driver(mi, di)); });
    });

    Driver::Driver(MeterInfo &mi, DriverInfo &di) :  MeterCommonImplementation(mi, di)
    {
        setMfctTPLStatusBits(
            Translate::Lookup()
            .add(Translate::Rule("TPL_STS", Translate::MapType::BitToString)
                 .set(MaskBits(0xe0))
                 .set(DefaultMessage("OK"))
                 .add(Translate::Map(0x80 ,"SABOTAGE_ENCLOSURE", TestBit::Set))));

        addStringField(
            "status",
            "Meter status from tpl status field.",
            DEFAULT_PRINT_PROPERTIES  |
            PrintProperty::STATUS | PrintProperty::INCLUDE_TPL_STATUS);

        addNumericFieldWithExtractor(
            "total_heating",
            "The total heating energy consumption recorded by this meter.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Energy,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::AnyEnergyVIF)
            .set(IndexNr(1))
            );

        addNumericFieldWithExtractor(
            "device",
            "The date time when the recording was made.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::PointInTime,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::DateTime)
            );

        addNumericFieldWithExtractor(
            "total_cooling",
            "The total cooling energy consumption recorded by this meter.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Energy,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::AnyEnergyVIF)
            .set(SubUnitNr(0))
            .set(TariffNr(1))
            .set(StorageNr(0))
            );

        addNumericFieldWithExtractor(
            "total_heating",
            "Total heating volume of media.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Volume,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::Volume)
            .set(IndexNr(1))
            );

        addNumericFieldWithExtractor(
            "total_cooling",
            "Total cooling volume of media.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Volume,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::Volume)
            .set(SubUnitNr(0))
            .set(TariffNr(1))
            .set(StorageNr(0))
            );

        addNumericFieldWithExtractor(
            "c1_volume",
            "Supply c1 volume.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Volume,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::Volume)
            .set(IndexNr(3))
            );

        addNumericFieldWithExtractor(
            "c2_volume",
            "Return c2 volume.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Volume,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::Volume)
            .set(IndexNr(4))
            );

        addNumericFieldWithExtractor(
            "supply_temperature",
            "The supply t1 pipe temperature.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Temperature,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::FlowTemperature)
            .set(IndexNr(1))
            );

        addNumericFieldWithExtractor(
            "return_temperature",
            "The return t2 pipe temperature.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Temperature,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::ReturnTemperature)
            );

     }
}

// Test: testm4 hydrocalm4 05171338 NOKEY
// telegram=|3A44B409381317051A0D8C00B07A7D000000_046D05AA023A0C03000000000C13000000008C1003050000008C1013040000000F6401000000000000|
// {"media":"heat/cooling load","meter":"hydrocalm3","name":"testm4","id":"05171338","device_datetime":"2024-10-02 10:05","total_cooling_kwh":0.005,"total_cooling_m3":0.004,"total_heating_kwh":0,"total_heating_m3":0,"status":"OK","timestamp":"2024-10-02T08:06:16Z"}
// |testm4;05171338;0;0.005;2024-10-02 08:06:16