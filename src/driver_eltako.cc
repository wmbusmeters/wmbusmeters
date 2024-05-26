/*
 Copyright (C) 2024 Fredrik Öhrström (gpl-3.0-or-later)
               2024 Alexander Streit (gpl-3.0-or-later)

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
        di.setName("eltako_dsz15dm");
        di.setDefaultFields("name,id,total_energy_consumption_kwh,timestamp");
        di.setMeterType(MeterType::ElectricityMeter);
        di.addLinkMode(LinkMode::MBUS);
        di.addDetection(MANUFACTURER_ELT,  0x02,  0x01);
        di.setConstructor([](MeterInfo& mi, DriverInfo& di){ return shared_ptr<Meter>(new Driver(mi, di)); });
    });

    Driver::Driver(MeterInfo &mi, DriverInfo &di) : MeterCommonImplementation(mi, di)
    {
        addOptionalLibraryFields("firmware_version,manufacturer,meter_datetime,model_version");

        addStringField(
            "status",
            "Status and error flags.",
            PrintProperty::STATUS | PrintProperty::INCLUDE_TPL_STATUS);

        addStringFieldWithExtractorAndLookup(
            "error_flags",
            "Error flags.",
            PrintProperty::INJECT_INTO_STATUS,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::ErrorFlags),
            Translate::Lookup()
            .add(Translate::Rule("ERROR_FLAGS", Translate::MapType::IndexToString)
                 .set(MaskBits(0xff))
                 .add(Translate::Map(0x01, "CODE_01_SYS_BUSY", TestBit::Set))
                 .add(Translate::Map(0x02, "CODE_02_GENERIC_APPLICATION_ERROR", TestBit::Set))
                 .add(Translate::Map(0x04, "CODE_04_CURRENT_LOW", TestBit::Set))
                 .add(Translate::Map(0x08, "CODE_08_PERMANENT_ERROR", TestBit::Set))
                 .add(Translate::Map(0x10, "CODE_10_TEMPORARY_ERROR", TestBit::Set))
                ));

        addNumericFieldWithExtractor(
            "total_energy_consumption_tariff_{tariff_counter}",
            "Total cumulative active energy per tariff.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Energy,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::AnyEnergyVIF)
            .set(StorageNr(0))
            .set(TariffNr(1),TariffNr(2))
            );

        addNumericFieldWithExtractor(
            "reactive_energy_consumption_tariff_{tariff_counter}",
            "Total cumulative reactive energy per tariff.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Energy,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::AnyEnergyVIF)
            .set(StorageNr(2))
            .set(TariffNr(1),TariffNr(2))
            );

        addNumericFieldWithExtractor(
            "voltage_l1_n",
            "Instantaneous voltage between L1 and neutral.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Voltage,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::Voltage)
            .add(VIFCombinableRaw(0x7f01))
            );

        addNumericFieldWithExtractor(
            "current_l1",
            "Instantaneous current in the L1 phase.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Amperage,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::Amperage)
            .add(VIFCombinableRaw(0x7f01))
            );

        addNumericFieldWithExtractor(
            "active_consumption_l1",
            "Instantaneous active power for L1 phase.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Power,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::AnyPowerVIF)
            .add(VIFCombinableRaw(0x7f01))
            );

        addNumericFieldWithExtractor(
            "reactive_consumption_l1",
            "Instantaneous reactive power for L1 phase.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Power,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::AnyPowerVIF)
            .set(SubUnitNr(1))
            .add(VIFCombinableRaw(0x7f01))
            );

        addNumericFieldWithExtractor(
            "voltage_l2_n",
            "Instantaneous voltage between L2 and neutral.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Voltage,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::Voltage)
            .add(VIFCombinableRaw(0x7f02))
            );

        addNumericFieldWithExtractor(
            "current_l2",
            "Instantaneous current in the L2 phase.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Amperage,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::Amperage)
            .add(VIFCombinableRaw(0x7f02))
            );

        addNumericFieldWithExtractor(
            "active_consumption_l2",
            "Instantaneous active power for L2 phase.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Power,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::AnyPowerVIF)
            .add(VIFCombinableRaw(0x7f02))
            );

        addNumericFieldWithExtractor(
            "reactive_consumption_l2",
            "Instantaneous reactive power for L2 phase.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Power,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::AnyPowerVIF)
            .set(SubUnitNr(1))
            .add(VIFCombinableRaw(0x7f02))
            );

        addNumericFieldWithExtractor(
            "voltage_l3_n",
            "Instantaneous voltage between L3 and neutral.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Voltage,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::Voltage)
            .add(VIFCombinableRaw(0x7f03))
            );

        addNumericFieldWithExtractor(
            "current_l3",
            "Instantaneous current in the L3 phase.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Amperage,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::Amperage)
            .add(VIFCombinableRaw(0x7f03))
            );

        addNumericFieldWithExtractor(
            "active_consumption_l3",
            "Instantaneous active power for L3 phase.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Power,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::AnyPowerVIF)
            .add(VIFCombinableRaw(0x7f03))
            );

        addNumericFieldWithExtractor(
            "reactive_consumption_l3",
            "Instantaneous reactive power for L3 phase.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Power,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::AnyPowerVIF)
            .set(SubUnitNr(1))
            .add(VIFCombinableRaw(0x7f03))
            );

        addNumericFieldWithExtractor(
            "ct_numerator",
            "Current transformer ratio (numerator).",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Dimensionless,
            VifScaling::None, DifSignedness::Signed,
            FieldMatcher::build()
            .set(DifVifKey("02FF68")),
            Unit::NUMBER
            );

        addNumericFieldWithExtractor(
            "active_consumption_total",
            "Instantaneous active power for all phases.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Power,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::AnyPowerVIF)
            .add(VIFCombinableRaw(0x7f00))
            );

        addNumericFieldWithExtractor(
            "reactive_consumption_total",
            "Instantaneous reactive power for all phases.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Power,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::AnyPowerVIF)
            .set(SubUnitNr(1))
            .add(VIFCombinableRaw(0x7f00))
            );

        addNumericFieldWithExtractor(
            "active_tariff",
            "Active tariff.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Dimensionless,
            VifScaling::None, DifSignedness::Signed,
            FieldMatcher::build()
            .set(DifVifKey("01FF13")),
            Unit::NUMBER
            );



    }
}

// Test: Electricity eltako 24450291 NOKEY
// Telegram=|689292680801729102452494150102270000008C1004997500008C1104997500008C2004000000008C21040000000002FDC9FF01E80002FDDBFF01000002ACFF0101008240ACFF01010002FDC9FF02E80002FDDBFF02000002ACFF0200008240ACFF02000002FDC9FF03E70002FDDBFF03070002ACFF030E008240ACFF03080002FF68010002ACFF000F008240ACFF000A0001FF1300D416|
// { "media":"electricity", "meter":"eltako_dsz15dm", "name":"", "id":"24450291", "active_consumption_l1_kw":0.01, "active_consumption_l2_kw":0, "active_consumption_l3_kw":0.14, "active_consumption_total_kw":0.15, "active_tariff_nr":0, "ct_numerator_nr":1, "current_l1_a":0, "current_l2_a":0, "current_l3_a":0.7, "reactive_consumption_l1_kw":0.01, "reactive_consumption_l2_kw":0, "reactive_consumption_l3_kw":0.08, "reactive_consumption_total_kw":0.1, "reactive_energy_consumption_tariff_1_kwh":75.99, "reactive_energy_consumption_tariff_2_kwh":0, "total_energy_consumption_tariff_1_kwh":75.99, "total_energy_consumption_tariff_2_kwh":0, "voltage_l1_n_v":232, "voltage_l2_n_v":232, "voltage_l3_n_v":231, "status":"OK", "timestamp":"2024-05-25T22:51:20Z" }

