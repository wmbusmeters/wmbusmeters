/*
 Copyright (C) 2020-2022 Fredrik Öhrström (gpl-3.0-or-later)

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
        di.setName("nzr");
        di.setDefaultFields("name,id,total_energy_consumption_kwh,current_power_consumption_kw,current_power_consumption_1_kw,current_power_consumption_2_kw,current_power_consumption_3_kw,voltage_at_phase_1_v,voltage_at_phase_2_v,voltage_at_phase_3_v,current_at_phase_1_a,current_at_phase_2_a,current_at_phase_3_a");
        di.setMeterType(MeterType::ElectricityMeter);
        di.addLinkMode(LinkMode::T1);
        di.addDetection(MANUFACTURER_NZR,  0x02,  0x00);
        di.addDetection(MANUFACTURER_EMH,  0x02,  0x00);
        di.setConstructor([](MeterInfo& mi, DriverInfo& di){ return shared_ptr<Meter>(new Driver(mi, di)); });
    });

    Driver::Driver(MeterInfo &mi, DriverInfo &di) : MeterCommonImplementation(mi, di)
    {
        addStringField(
            "status",
            "Meter status. Includes both meter error field and tpl status field.",
            PrintProperty::STATUS | PrintProperty::INCLUDE_TPL_STATUS);

        addOptionalLibraryFields("on_time_h");

        addNumericFieldWithExtractor(
            "total_energy_consumption",
            "The total energy consumption recorded by this meter.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Energy,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::AnyEnergyVIF)
            .set(TariffNr(1))
            );

        addNumericFieldWithExtractor(
            "current_power_consumption",
            "Current power consumption.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Power,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::AnyPowerVIF)
            );
        
        addNumericFieldWithExtractor(
            "current_power_consumption_1",
            "Current power consumption L1.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Power,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::AnyPowerVIF)
            .set(StorageNr(2))
            );

        addNumericFieldWithExtractor(
            "current_power_consumption_2",
            "Current power consumption L2.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Power,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::AnyPowerVIF)
            .set(StorageNr(4))
            );

        addNumericFieldWithExtractor(
            "current_power_consumption_3",
            "Current power consumption L3.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Power,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::AnyPowerVIF)
            .set(StorageNr(6))
            );
        
        addNumericFieldWithExtractor(
            "voltage_at_phase_1",
            "Voltage at phase L1.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Voltage,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::Voltage)
            .set(StorageNr(2))
            );

        addNumericFieldWithExtractor(
            "voltage_at_phase_2",
            "Voltage at phase L2.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Voltage,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::Voltage)
            .set(StorageNr(4))
            );

        addNumericFieldWithExtractor(
            "voltage_at_phase_3",
            "Voltage at phase L3.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Voltage,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::Voltage)
            .set(StorageNr(6))
            );
       
       addNumericFieldWithExtractor(
            "current_at_phase_1",
            "Current at phase L1.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Amperage,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::Amperage)
            .set(StorageNr(2))
            );

        addNumericFieldWithExtractor(
            "current_at_phase_2",
            "Current at phase L2.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Amperage,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::Amperage)
            .set(StorageNr(4))
            );

        addNumericFieldWithExtractor(
            "current_at_phase_3",
            "Current at phase L3.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Amperage,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::Amperage)
            .set(StorageNr(6))
            );

    }
}

// Test: MyElectricity1 nzr 07911459 NOKEY
// telegram=|68222268080B7259149107523B0002580000008c1005857649008400292f6a010001fd17001fd116|
// {"media":"electricity","meter":"nzr","name":"MyElectricity1","id":"07911459","total_energy_consumption_kwh":49768.5,"current_power_consumption_kw":0.92719,"timestamp":"1111-11-11T11:11:11Z"}
// |MyElectricity1;07911459;49768.5;0.92719;null;null;null;null;null;null;null;null;null

// Test: MyElectricity2 nzr 17911459 NOKEY
// telegram=|684f4f68080B7259149117523B00029200000084002ccB00000084012962fB0000840229e85f010084032905Bf00008201fd49ed008202fd49e9008203fd49ea008201fd5a21018202fd5a8d018203fd5ad700e216|
// {"media":"electricity","meter":"nzr","name":"MyElectricity2","id":"17911459","current_power_consumption_kw":2.03,"current_power_consumption_1_kw":0.64354,"current_power_consumption_2_kw":0.90088,"current_power_consumption_3_kw":0.48901,"voltage_at_phase_1_v":237,"voltage_at_phase_2_v":233,"voltage_at_phase_3_v":234,"current_at_phase_1_a":2.89,"current_at_phase_2_a":3.97,"current_at_phase_3_a":2.15,"timestamp":"1111-11-11T11:11:11Z"}
// |MyElectricity2;17911459;null;2.03;0.64354;0.90088;0.48901;237;233;234;2.89;3.97;2.15



