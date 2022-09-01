/*
 Copyright (C) 2020-2022 thecem (gpl-3.0-or-later)

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
    struct Multical303 : public virtual MeterCommonImplementation {
        Multical303(MeterInfo &mi, DriverInfo &di);
    };

    static bool ok = registerDriver([](DriverInfo&di)
    {
        di.setName("multical303");
        di.setDefaultFields("name,id,status,total_energy_kwh,target_energy_kwh,timestamp");
        di.setMeterType(MeterType::HeatMeter);
        di.addLinkMode(LinkMode::C1);
        di.addLinkMode(LinkMode::T1);
        di.addDetection(MANUFACTURER_KAM, 0x04,  0x40);
        di.setConstructor([](MeterInfo& mi, DriverInfo& di){ return shared_ptr<Meter>(new Multical303(mi, di)); });
    });

    Multical303::Multical303(MeterInfo &mi, DriverInfo &di) : MeterCommonImplementation(mi, di)
    {
        addNumericFieldWithExtractor(
            "total_energy",
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
            "forward_energy",
            "The forward energy of the water (4/97/Energy E8).",
            PrintProperty::JSON,
            Quantity::Energy,
            VifScaling::None,
            FieldMatcher::build()
            .set(DifVifKey("04FF07")),
            Unit::KWH);

        addNumericFieldWithExtractor(
            "return_energy",
            "The return energy of the water (5/110/Energy E9).",
            PrintProperty::JSON,
            Quantity::Energy,
            VifScaling::None,
            FieldMatcher::build()
            .set(DifVifKey("04FF08")),
            Unit::KWH);

        addNumericFieldWithExtractor(
            "forward",
            "The forward temperature of the water (6/86/t2 actial 2 decimals).",
            PrintProperty::JSON | PrintProperty::OPTIONAL,
            Quantity::Temperature,
            VifScaling::Auto,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::FlowTemperature)
            );

        addNumericFieldWithExtractor(
            "return",
            "The return temperature of the water (7/87/t2 actial 2 decimals).",
            PrintProperty::JSON | PrintProperty::OPTIONAL,
            Quantity::Temperature,
            VifScaling::Auto,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::ReturnTemperature)
            );

        addNumericFieldWithExtractor(
            "actual_flow",
            "The actual amount of water that pass through this meter (8/74/Flow V1 actual).",
            PrintProperty::JSON,
            Quantity::Flow,
            VifScaling::Auto,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::VolumeFlow)
            );

        addStringFieldWithExtractorAndLookup(
            "status",
            "Status and error flags (9/369/ Info Bits).",
            PrintProperty::JSON | PrintProperty::FIELD | PrintProperty::IMPORTANT |
            PrintProperty::STATUS | PrintProperty::JOIN_TPL_STATUS,
            FieldMatcher::build()
            .set(DifVifKey("02FF22")),
            Translate::Lookup(
            {
                {
                    {
                        "ERROR_FLAGS",
                        Translate::Type::BitToString,
                        0xffff,
                        "OK",
                        {
                            { 0x0000, "OK" },
                            { 0x0001, "VOLTAGE_INTERRUPTED" },
                            { 0x0002, "LOW_BATTERY_LEVEL" },
                            { 0x0004, "SENSOR_ERROR" },
                            { 0x0008, "SENSOR_T1_ABOVE_MEASURING_RANGE" },
                            { 0x0010, "SENSOR_T2_ABOVE_MEASURING_RANGE" },
                            { 0x0020, "SENSOR_T1_BELOW_MEASURING_RANGE" },
                            { 0x0040, "SENSOR_T2_BELOW_MEASURING_RANGE" },
                            { 0x0080, "TEMP_DIFF_WRONG_POLARITY" },
                            { 0x0100, "FLOW_SENSOR_WEAK_OR_AIR" },
                            { 0x0200, "WRONG_FLOW_DIRECTION" },
                            { 0x0400, "UNKNOWN" },
                            { 0x0800, "FLOW_INCREASED" },
                        }
                    },
                },
            }));

        addStringFieldWithExtractor(
            "date_time",
            "The date and time (10/348/Date and time).",
            PrintProperty::JSON | PrintProperty::OPTIONAL,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::Date)
            );

        addNumericFieldWithExtractor(
            "target_energy",
            "The energy consumption recorded by this meter at the set date (11/60/Heat energy E1/026C).",
            PrintProperty::JSON | PrintProperty::FIELD | PrintProperty::IMPORTANT,
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
            PrintProperty::JSON | PrintProperty::FIELD,
            Quantity::Volume,
            VifScaling::Auto,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::Volume)
            .set(StorageNr(1))
            );

        addStringFieldWithExtractor(
            "target_date_time",
            "The most recent billing period date and time (14/348/Date and Time logged).",
            PrintProperty::JSON | PrintProperty::FIELD,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::Date)
            .set(StorageNr(1))
            );
    }
}
// Test: Heat multical303 82788281 75EDE0CBBB6E126764898645AA366568
// Comment:
// telegram=|_5E442D2C8182788240047A83005025186E9C6D9815EBFC04CBE8E4B8C8A6B9949C9DAA629CD96D920F321CFBEE7AE104DD8532C5C0EE79B4CFACCFA75D3A5EB6D4493DFAFE91B15C3A3DCFCE899138B8EA02CDB609D31CF019F9E4FD04559E|
// {"media":"heat","meter":"multical303","name":"Heat","id":"82788281","total_energy_kwh":0,"total_volume_m3":2.38,"forward_energy_kwh":61,"return_energy_kwh":61,"forward_c":26.07,"return_c":26.22,"actual_flow_m3h":0,"status":"OK","date_time":"2022-08-18","target_energy_kwh":0,"target_volume_m3":0,"target_date_time":"2022-08-01","timestamp":"1111-11-11T11:11:11Z"}
// |Heat;82788281;OK;0;0;1111-11-11 11:11.11
