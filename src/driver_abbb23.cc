/*
 Copyright (C) 2019-2023 Fredrik Öhrström (gpl-3.0-or-later)

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
      addStringField(
            "status",
            "Status, error, warning and alarm flags.",
            DEFAULT_PRINT_PROPERTIES | PrintProperty::INCLUDE_TPL_STATUS | PrintProperty::STATUS);

        addNumericFieldWithExtractor(
            "total_energy_consumption",
            "Total cumulative active imported energy.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Energy,
            VifScaling::Auto,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::AnyEnergyVIF)
            .add(VIFCombinableRaw(0)) // Stupid 00 combinable....
            );

        addNumericFieldWithExtractor(
            "total_energy_consumption_tariff_{tariff_counter}",
            "Total cumulative active imported energy per tariff.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Energy,
            VifScaling::Auto,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::AnyEnergyVIF)
            .set(TariffNr(1),TariffNr(4))
            .add(VIFCombinableRaw(0)) // Stupid 00 combinable....
            );

        addNumericFieldWithExtractor(
            "total_energy_production",
            "Total cumulative active exported energy.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Energy,
            VifScaling::Auto,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::AnyEnergyVIF)
            .set(TariffNr(0))
            .set(SubUnitNr(1))
            .add(VIFCombinableRaw(0)) // Stupid 00 combinable....
            );

        addNumericFieldWithExtractor(
            "total_energy_production_tariff_{tariff_counter}",
            "Total cumulative active exported energy per tariff.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Energy,
            VifScaling::Auto,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::AnyEnergyVIF)
            .set(TariffNr(1),TariffNr(4))
            .set(SubUnitNr(1))
            .add(VIFCombinableRaw(0)) // Stupid 00 combinable....
            );

        addNumericFieldWithExtractor(
            "active_tariff",
            "Active tariff.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Dimensionless,
            VifScaling::None,
            FieldMatcher::build()
            .set(DifVifKey("01FF9300"))
            );

        addNumericFieldWithExtractor(
            "ct_numerator",
            "Current transformer ratio (numerator).",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Dimensionless,
            VifScaling::None,
            FieldMatcher::build()
            .set(DifVifKey("04FFA015")),
            Unit::FACTOR
            );

        addNumericFieldWithExtractor(
            "vt_numerator",
            "Voltage transformer ratio (numerator).",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Dimensionless,
            VifScaling::None,
            FieldMatcher::build()
            .set(DifVifKey("04FFA115")),
            Unit::FACTOR
            );

        addNumericFieldWithExtractor(
            "ct_denominator",
            "Current transformer ratio (denominator).",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Dimensionless,
            VifScaling::None,
            FieldMatcher::build()
            .set(DifVifKey("04FFA215")),
            Unit::FACTOR
            );

        addNumericFieldWithExtractor(
            "vt_denominator",
            "Voltage transformer ratio (denominator).",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Dimensionless,
            VifScaling::None,
            FieldMatcher::build()
            .set(DifVifKey("04FFA315")),
            Unit::FACTOR
            );

       addStringFieldWithExtractorAndLookup(
            "error_flags",
            "Error flags.",
            DEFAULT_PRINT_PROPERTIES | PrintProperty::INJECT_INTO_STATUS,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(DifVifKey("07FFA600")),
            Translate::Lookup()
            .add(Translate::Rule("ERROR_FLAGS", Translate::Type::BitToString)
                 .set(MaskBits(0xffffffffffffffff))
                 .set(DefaultMessage("OK"))
                ));

       addStringFieldWithExtractorAndLookup(
            "warning_flags",
            "Warning flags.",
            DEFAULT_PRINT_PROPERTIES | PrintProperty::INJECT_INTO_STATUS,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(DifVifKey("07FFA700")),
            Translate::Lookup()
            .add(Translate::Rule("WARNING_FLAGS", Translate::Type::BitToString)
                 .set(MaskBits(0xffffffffffffffff))
                 .set(DefaultMessage("OK"))
                ));

       addStringFieldWithExtractorAndLookup(
            "information_flags",
            "Information flags.",
            DEFAULT_PRINT_PROPERTIES,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(DifVifKey("07FFA800")),
            Translate::Lookup()
            .add(Translate::Rule("INFORMATION_FLAGS", Translate::Type::BitToString)
                 .set(MaskBits(0xffffffffffffffff))
                 .set(DefaultMessage(""))
                ));

       addStringFieldWithExtractorAndLookup(
            "alarm_flags",
            "alarm flags.",
            DEFAULT_PRINT_PROPERTIES | PrintProperty::INJECT_INTO_STATUS,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(DifVifKey("07FFA900")),
            Translate::Lookup()
            .add(Translate::Rule("ALARM_FLAGS", Translate::Type::BitToString)
                 .set(MaskBits(0xffffffffffffffff))
                 .set(DefaultMessage("OK"))
                ));

        addStringFieldWithExtractor(
            "firmware_version",
            "Firmware version.",
            DEFAULT_PRINT_PROPERTIES,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::FirmwareVersion)
            .add(VIFCombinableRaw(0)) // Stupid 00 combinable....
            );

        addStringFieldWithExtractor(
            "product_no",
            "The meter device product number.",
            DEFAULT_PRINT_PROPERTIES,
            FieldMatcher::build().
            set(DifVifKey("0DFFAA00")));

        addNumericFieldWithExtractor(
            "power_fail",
            "Power fail counter.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Dimensionless,
            VifScaling::None,
            FieldMatcher::build()
            .set(DifVifKey("04FF9800"))
            );

        addNumericFieldWithExtractor(
            "active_consumption",
            "Instantaneous total active imported power.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Power,
            VifScaling::Auto,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::AnyPowerVIF)
            .add(VIFCombinableRaw(0)) // Stupid 00 combinable....
            );

        addNumericFieldWithExtractor(
            "reactive_consumption",
            "Instantaneous total reactive imported power.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Power,
            VifScaling::AutoSigned,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::AnyPowerVIF)
            .set(SubUnitNr(2))
            .add(VIFCombinableRaw(0)) // Stupid 00 combinable....
            );

        addNumericFieldWithExtractor(
            "apparent_consumption",
            "Instantaneous total apparent imported power.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Power,
            VifScaling::AutoSigned,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::AnyPowerVIF)
            .set(SubUnitNr(4))
            .add(VIFCombinableRaw(0)) // Stupid 00 combinable....
            );

        addNumericFieldWithExtractor(
            "voltage_l1_n",
            "Instantaneous voltage between L1 and neutral.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Voltage,
            VifScaling::AutoSigned,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::Voltage)
            .add(VIFCombinableRaw(0x7f01))
            .add(VIFCombinableRaw(0)) // Stupid 00 combinable....
            );

        addNumericFieldWithExtractor(
            "voltage_l2_n",
            "Instantaneous voltage between L2 and neutral.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Voltage,
            VifScaling::AutoSigned,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::Voltage)
            .add(VIFCombinableRaw(0x7f02))
            .add(VIFCombinableRaw(0)) // Stupid 00 combinable....
            );

        addNumericFieldWithExtractor(
            "voltage_l3_n",
            "Instantaneous voltage between L3 and neutral.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Voltage,
            VifScaling::AutoSigned,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::Voltage)
            .add(VIFCombinableRaw(0x7f03))
            .add(VIFCombinableRaw(0)) // Stupid 00 combinable....
            );

        addNumericFieldWithExtractor(
            "current_l1",
            "Instantaneous current in the L1 phase.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Amperage,
            VifScaling::AutoSigned,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::Amperage)
            .add(VIFCombinableRaw(0x7f01))
            .add(VIFCombinableRaw(0)) // Stupid 00 combinable....
            );

        addNumericFieldWithExtractor(
            "current_l2",
            "Instantaneous current in the L2 phase.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Amperage,
            VifScaling::AutoSigned,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::Amperage)
            .add(VIFCombinableRaw(0x7f02))
            .add(VIFCombinableRaw(0)) // Stupid 00 combinable....
            );

        addNumericFieldWithExtractor(
            "current_l3",
            "Instantaneous current in the L3 phase.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Amperage,
            VifScaling::AutoSigned,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::Amperage)
            .add(VIFCombinableRaw(0x7f03))
            .add(VIFCombinableRaw(0)) // Stupid 00 combinable....
            );

        addNumericFieldWithExtractor(
            "frequency",
            "Frequency of AC",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Frequency,
            VifScaling::None,
            FieldMatcher::build()
            .set(DifVifKey("0AFFD900")),
            Unit::HZ,
            0.01
            );

        addNumericFieldWithExtractor(
            "power",
            "Power factor.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Dimensionless,
            VifScaling::None,
            FieldMatcher::build()
            .set(DifVifKey("02FFE000")),
            Unit::FACTOR,
            0.001
            );

        addNumericFieldWithExtractor(
            "energy_co2",
            "Energy in co2.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Mass,
            VifScaling::None,
            FieldMatcher::build()
            .set(DifVifKey("0EFFF9C400")),
            Unit::KG,
            0.001
            );


    }
}

/* / Test: ABBmeter abbb23 33221100 NOKEY
  / telegram=|844442040011223320027A3E000020_0E840017495200000004FFA0150000000004FFA1150000000004FFA2150000000004FFA3150000000007FFA600000000000000000007FFA700000000000000000007FFA800000000000000000007FFA90000000000000000000DFD8E0007302E38322E31420DFFAA000B3030312D313131203332421F|
  / {"media":"electricity","meter":"abbb23","name":"ABBmeter","id":"33221100","total_energy_consumption_kwh":5249.17,"firmware_version": "B1.28.0","product_no": "B23 111-100","timestamp":"1111-11-11T11:11:11Z"}
 / |ABBmeter;33221100;5249.17;1111-11-11 11:11.11
*/
