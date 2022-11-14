/*
 Copyright (C) 2018-2022 Fredrik Öhrström (gpl-3.0-or-later)
 Copyright (C)      2020 Eric Bus (gpl-3.0-or-later)
 Copyright (C)      2022 thecem (gpl-3.0-or-later)

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
    struct Driver : public virtual MeterCommonImplementation {
        Driver(MeterInfo &mi, DriverInfo &di);
    };

    static bool ok = registerDriver([](DriverInfo&di)
    {
        di.setName("multical603");
        di.setDefaultFields("name,id,total_energy_consumption_kwh,total_volume_m3,volume_flow_m3h,t1_temperature_c,t2_temperature_c,current_status,timestamp");
        di.setMeterType(MeterType::HeatMeter);
        di.addLinkMode(LinkMode::C1);
        di.addLinkMode(LinkMode::T1);
        di.addDetection(MANUFACTURER_KAM, 0x04, 0x35);
        di.addDetection(MANUFACTURER_KAM, 0x0c, 0x35);
        di.setConstructor([](MeterInfo& mi, DriverInfo& di){ return shared_ptr<Meter>(new Driver(mi, di)); });
    });

    Driver::Driver(MeterInfo &mi, DriverInfo &di) : MeterCommonImplementation(mi, di)
    {
        addOptionalCommonFields("on_time_h");

        // Technical Description Multical 603 page 116 section 7.7.2 Information code types on serial communication.
        addStringFieldWithExtractorAndLookup(
            "status",
            "Status and error flags.",
            PrintProperty::JSON | PrintProperty::FIELD | PrintProperty::IMPORTANT |
            PrintProperty::STATUS | PrintProperty::JOIN_TPL_STATUS,
            FieldMatcher::build()
            .set(DifVifKey("04FF22")),
            Translate::Lookup(
            {
                {
                    {
                        "ERROR_FLAGS",
                        Translate::Type::BitToString,
                        0xffffffff,
                        "OK",
                        {
                            { 0x00000001, "VOLTAGE_INTERRUPTED" },
                            { 0x00000002, "LOW_BATTERY_LEVEL" },
                            { 0x00000004, "SENSOR_ERROR" },
                            { 0x00000008, "SENSOR_T1_ABOVE_MEASURING_RANGE" },
                            { 0x00000010, "SENSOR_T2_ABOVE_MEASURING_RANGE" },
                            { 0x00000020, "SENSOR_T1_BELOW_MEASURING_RANGE" },
                            { 0x00000040, "SENSOR_T2_BELOW_MEASURING_RANGE" },
                            { 0x00000080, "TEMP_DIFF_WRONG_POLARITY" },
                            { 0x00000100, "FLOW_SENSOR_WEAK_OR_AIR" },
                            { 0x00000200, "WRONG_FLOW_DIRECTION" },
                            { 0x00000400, "RESERVED_BIT_10" },
                            { 0x00000800, "FLOW_INCREASED" },
                            { 0x00001000, "IN_A1_LEAKAGE_IN_THE_SYSTEM" },
                            { 0x00002000, "IN_B1_LEAKAGE_IN_THE_SYSTEM" },
                            { 0x00004000, "IN-A1_A2_EXTERNAL_ALARM" },
                            { 0x00008000, "IN-B1_B2_EXTERNAL_ALARM" },
                            { 0x00010000, "V1_COMMUNICATION_ERROR" },
                            { 0x00020000, "V1_WRONG_PULSE_FIGURE" },
                            { 0x00040000, "IN_A2_LEAKAGE_IN_THE_SYSTEM" },
                            { 0x00080000, "IN_B2_LEAKAGE_IN_THE_SYSTEM" },
                            { 0x00100000, "T3_ABOVE_MEASURING_RANGE_OR_SWITCHED_OFF" },
                            { 0x00200000, "T3_BELOW_MEASURING_RANGE_OR_SHORT_CIRCUITED" },
                            { 0x00400000, "V2_COMMUNICATION_ERROR" },
                            { 0x00800000, "V2_WRONG_PULSE_FIGURE" },
                            { 0x01000000, "V2_AIR" },
                            { 0x02000000, "V2_WRONG_FLOW_DIRECTION" },
                            { 0x04000000, "RESERVED_BIT_26" },
                            { 0x08000000, "V2_INCREASED_FLOW" },
                            { 0x10000000, "V1_V2_BURST_WATER_LOSS" },
                            { 0x20000000, "V1_V2_BURST_WATER_PENETRATION" },
                            { 0x40000000, "V1_V2_LEAKAGE_WATER_LOSS" },
                            { 0x80000000, "V1_V2_LEAKAGE_WATER_PENETRATION" }
                        }
                    },
                },
            }));

        addNumericFieldWithExtractor(
            "total_energy_consumption",
            "The total energy consumption recorded by this meter.",
            PrintProperty::JSON | PrintProperty::FIELD | PrintProperty::IMPORTANT,
            Quantity::Energy,
            VifScaling::Auto,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::AnyEnergyVIF)
            );

        addNumericFieldWithExtractor(
            "total_volume",
            "The volume of water (3/68/Volume V1).",
            PrintProperty::JSON | PrintProperty::OPTIONAL,
            Quantity::Volume,
            VifScaling::Auto,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::Volume)
            );

        addNumericFieldWithExtractor(
            "volume_flow",
            "The actual amount of water that pass through this meter (8/74/Flow V1 actual).",
            PrintProperty::JSON | PrintProperty::OPTIONAL,
            Quantity::Flow,
            VifScaling::Auto,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::VolumeFlow)
            );

        addNumericFieldWithExtractor(
            "power",
            "The current power flowing.",
            PrintProperty::JSON | PrintProperty::OPTIONAL,
            Quantity::Power,
            VifScaling::Auto,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::AnyPowerVIF)
            );

        addNumericFieldWithExtractor(
            "max_power",
            "The maximum power supplied.",
            PrintProperty::JSON | PrintProperty::OPTIONAL,
            Quantity::Power,
            VifScaling::Auto,
            FieldMatcher::build()
            .set(MeasurementType::Maximum)
            .set(VIFRange::AnyPowerVIF)
            );

        addNumericFieldWithExtractor(
            "t1_temperature",
            "The forward temperature of the water (6/86/t2 actual 2 decimals).",
            PrintProperty::JSON | PrintProperty::OPTIONAL,
            Quantity::Temperature,
            VifScaling::Auto,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::FlowTemperature)
            );

        addNumericFieldWithExtractor(
            "t2_temperature",
            "The return temperature of the water (7/87/t2 actual 2 decimals).",
            PrintProperty::JSON | PrintProperty::OPTIONAL,
            Quantity::Temperature,
            VifScaling::Auto,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::ReturnTemperature)
            );


        addNumericFieldWithExtractor(
            "max_flow",
            "The maximum flow of water that passed through this meter.",
            PrintProperty::JSON | PrintProperty::OPTIONAL,
            Quantity::Flow,
            VifScaling::Auto,
            FieldMatcher::build()
            .set(MeasurementType::Maximum)
            .set(VIFRange::VolumeFlow)
            );

        // Backwards compatible current_status to be removed.
        addStringFieldWithExtractorAndLookup(
            "current_status",
            "Status and error flags (9/369/ Info Bits).",
            PrintProperty::JSON | PrintProperty::FIELD | PrintProperty::DEPRECATED,
            FieldMatcher::build()
            .set(DifVifKey("04FF22")),
            Translate::Lookup(
            {
                {
                    {
                        "ERROR_FLAGS",
                        Translate::Type::BitToString,
                        0xffffffff,
                        "",
                        {
                            { 0x00000001, "VOLTAGE_INTERRUPTED" },
                            { 0x00000002, "LOW_BATTERY_LEVEL" },
                            { 0x00000004, "SENSOR_ERROR" },
                            { 0x00000008, "SENSOR_T1_ABOVE_MEASURING_RANGE" },
                            { 0x00000010, "SENSOR_T2_ABOVE_MEASURING_RANGE" },
                            { 0x00000020, "SENSOR_T1_BELOW_MEASURING_RANGE" },
                            { 0x00000040, "SENSOR_T2_BELOW_MEASURING_RANGE" },
                            { 0x00000080, "TEMP_DIFF_WRONG_POLARITY" },
                            { 0x00000100, "FLOW_SENSOR_WEAK_OR_AIR" },
                            { 0x00000200, "WRONG_FLOW_DIRECTION" },
                            { 0x00000400, "RESERVED_BIT_10" },
                            { 0x00000800, "FLOW_INCREASED" },
                            { 0x00001000, "IN_A1_LEAKAGE_IN_THE_SYSTEM" },
                            { 0x00002000, "IN_B1_LEAKAGE_IN_THE_SYSTEM" },
                            { 0x00004000, "IN-A1_A2_EXTERNAL_ALARM" },
                            { 0x00008000, "IN-B1_B2_EXTERNAL_ALARM" },
                            { 0x00010000, "V1_COMMUNICATION_ERROR" },
                            { 0x00020000, "V1_WRONG_PULSE_FIGURE" },
                            { 0x00040000, "IN_A2_LEAKAGE_IN_THE_SYSTEM" },
                            { 0x00080000, "IN_B2_LEAKAGE_IN_THE_SYSTEM" },
                            { 0x00100000, "T3_ABOVE_MEASURING_RANGE_OR_SWITCHED_OFF" },
                            { 0x00200000, "T3_BELOW_MEASURING_RANGE_OR_SHORT_CIRCUITED" },
                            { 0x00400000, "V2_COMMUNICATION_ERROR" },
                            { 0x00800000, "V2_WRONG_PULSE_FIGURE" },
                            { 0x01000000, "V2_AIR" },
                            { 0x02000000, "V2_WRONG_FLOW_DIRECTION" },
                            { 0x04000000, "RESERVED_BIT_26" },
                            { 0x08000000, "V2_INCREASED_FLOW" },
                            { 0x10000000, "V1_V2_BURST_WATER_LOSS" },
                            { 0x20000000, "V1_V2_BURST_WATER_PENETRATION" },
                            { 0x40000000, "V1_V2_LEAKAGE_WATER_LOSS" },
                            { 0x80000000, "V1_V2_LEAKAGE_WATER_PENETRATION" }
                        }
                    },
                },
            }));

        addNumericFieldWithExtractor(
            "forward_energy",
            "The forward energy of the water (4/97/Energy E8).",
            PrintProperty::JSON | PrintProperty::OPTIONAL,
            Quantity::Energy,
            VifScaling::None,
            FieldMatcher::build()
            .set(DifVifKey("04FF07")),
            Unit::M3C);

        addNumericFieldWithExtractor(
            "return_energy",
            "The return energy of the water (5/110/Energy E9).",
            PrintProperty::JSON | PrintProperty::OPTIONAL,
            Quantity::Energy,
            VifScaling::None,
            FieldMatcher::build()
            .set(DifVifKey("04FF08")),
            Unit::M3C);

        /* Deprecated kwh version where unit should be m3c. */
        addNumericFieldWithExtractor(
            "energy_forward",
            "Deprecated! The forward energy of the water but in wrong unit! Should be m3c!",
            PrintProperty::JSON | PrintProperty::OPTIONAL | PrintProperty::DEPRECATED,
            Quantity::Energy,
            VifScaling::None,
            FieldMatcher::build()
            .set(DifVifKey("04FF07")),
            Unit::KWH);

        /* Deprecated kwh version where unit should be m3c. */
        addNumericFieldWithExtractor(
            "energy_returned",
            "Deprecated! The return energy of the water but in wrong unit! Should be m3c!",
            PrintProperty::JSON | PrintProperty::OPTIONAL | PrintProperty::DEPRECATED,
            Quantity::Energy,
            VifScaling::None,
            FieldMatcher::build()
            .set(DifVifKey("04FF08")),
            Unit::KWH);

        addStringFieldWithExtractor(
            "meter_date",
            "The date and time (10/348/Date and time).",
            PrintProperty::JSON | PrintProperty::OPTIONAL,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::Date)
            );

        addNumericFieldWithExtractor(
            "target_energy",
            "The energy consumption recorded by this meter at the set date (11/60/Heat energy E1/026C).",
            PrintProperty::JSON | PrintProperty::FIELD | PrintProperty::OPTIONAL,
            Quantity::Energy,
            VifScaling::Auto,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::AnyEnergyVIF)
            .set(StorageNr(1))
            );

        addNumericFieldWithExtractor(
            "target_volume",
            "The amount of water that had passed through this meter at the set date (13/68/Volume V1).",
            PrintProperty::JSON | PrintProperty::FIELD | PrintProperty::OPTIONAL,
            Quantity::Volume,
            VifScaling::Auto,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::Volume)
            .set(StorageNr(1))
            );

        addStringFieldWithExtractor(
            "target_date",
            "The most recent billing period date and time (14/348/Date and Time logged).",
            PrintProperty::JSON | PrintProperty::FIELD | PrintProperty::OPTIONAL,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::Date)
            .set(StorageNr(1))
            );
    }
}
// Test: Heat multical603 36363636 NOKEY
// Comment:
// telegram=|42442D2C3636363635048D20E18025B62087D078_0406A500000004FF072B01000004FF089C000000041421020000043B120000000259D014025D000904FF2200000000|
// {"media":"heat","meter":"multical603","name":"Heat","id":"36363636","status":"OK","total_energy_consumption_kwh":165,"total_volume_m3":5.45,"volume_flow_m3h":0.018,"t1_temperature_c":53.28,"t2_temperature_c":23.04,"current_status":"","forward_energy_m3c":299,"return_energy_m3c":156,"energy_forward_kwh":299,"energy_returned_kwh":156,"timestamp":"1111-11-11T11:11:11Z"}
// |Heat;36363636;165;5.45;0.018;53.28;23.04;;1111-11-11 11:11.11

// Test: HeatInlet multical603 66666666 NOKEY
// telegram=|5A442D2C66666666350C8D2066D0E16420C6A178_0406051C000004FF07393D000004FF08AE2400000414F7680000043B47000000042D1600000002596D14025DFD0804FF22000000000422E61A0000143B8C010000142D7C000000|
// {"media":"heat volume at inlet","meter":"multical603","name":"HeatInlet","id":"66666666","on_time_h":6886,"status":"OK","total_energy_consumption_kwh":7173,"total_volume_m3":268.71,"volume_flow_m3h":0.071,"power_kw":2.2,"max_power_kw":12.4,"t1_temperature_c":52.29,"t2_temperature_c":23.01,"max_flow_m3h":0.396,"current_status":"","forward_energy_m3c":15673,"return_energy_m3c":9390,"energy_forward_kwh":15673,"energy_returned_kwh":9390,"timestamp":"1111-11-11T11:11:11Z"}
// |HeatInlet;66666666;7173;268.71;0.071;52.29;23.01;;1111-11-11 11:11.11
