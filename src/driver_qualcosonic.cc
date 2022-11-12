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
        di.setName("qualcosonic");
        di.setMeterType(MeterType::HeatCoolingMeter);
        di.addLinkMode(LinkMode::C1);
        di.addDetection(MANUFACTURER_AXI, 0x0d,  0x0b);
        di.setConstructor([](MeterInfo& mi, DriverInfo& di){ return shared_ptr<Meter>(new Driver(mi, di)); });
    });

    Driver::Driver(MeterInfo &mi, DriverInfo &di) :  MeterCommonImplementation(mi, di)
    {
        addOptionalCommonFields("fabrication_no,operating_time_h,on_time_h,meter_datetime,meter_datetime_at_error");
        addOptionalFlowRelatedFields("total_m3,flow_temperature_c,return_temperature_c,flow_return_temperature_difference_c,volume_flow_m3h");

        addStringFieldWithExtractorAndLookup(
            "status",
            "Meter status. Includes both meter error field and tpl status field.",
            PrintProperty::JSON | PrintProperty::FIELD | PrintProperty::IMPORTANT |
            PrintProperty::STATUS | PrintProperty::JOIN_TPL_STATUS,
            FieldMatcher::build()
            .set(VIFRange::ErrorFlags),
            Translate::Lookup(
            {
                {
                    {
                        "ERROR_FLAGS",
                        Translate::Type::BitToString,
                        0xffffffff,
                        "OK",
                        {
                            /* What do these bits mean? There are a lot of them...
                            */
                        }
                    },
                },
            }));

        addNumericFieldWithExtractor(
            "total_heat_energy",
            "The total heating energy consumption recorded by this meter.",
            PrintProperty::FIELD | PrintProperty::JSON | PrintProperty::IMPORTANT,
            Quantity::Energy,
            VifScaling::Auto,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::AnyEnergyVIF)
            .add(VIFCombinable::ForwardFlow)
            );

        addNumericFieldWithExtractor(
            "total_cooling_energy",
            "The total cooling energy consumption recorded by this meter.",
            PrintProperty::FIELD | PrintProperty::JSON | PrintProperty::IMPORTANT,
            Quantity::Energy,
            VifScaling::Auto,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::AnyEnergyVIF)
            .add(VIFCombinable::BackwardFlow)
            );

        addNumericFieldWithExtractor(
            "power",
            "The current power consumption.",
            PrintProperty::JSON | PrintProperty::FIELD,
            Quantity::Power,
            VifScaling::AutoSigned,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::AnyPowerVIF)
            );

        addStringFieldWithExtractor(
            "target_datetime",
            "Date and time when the previous billing period ended.",
            PrintProperty::FIELD | PrintProperty::JSON,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::DateTime)
            .set(StorageNr(16))
            );

        addNumericFieldWithExtractor(
            "target_heat_energy",
            "The heating energy consumption recorded at the end of the previous billing period.",
            PrintProperty::FIELD | PrintProperty::JSON,
            Quantity::Energy,
            VifScaling::Auto,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::AnyEnergyVIF)
            .add(VIFCombinable::ForwardFlow)
            .set(StorageNr(16))
            );

        addNumericFieldWithExtractor(
            "target_cooling_energy",
            "The cooling energy consumption recorded at the end of the previous billing period.",
            PrintProperty::FIELD | PrintProperty::JSON,
            Quantity::Energy,
            VifScaling::Auto,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::AnyEnergyVIF)
            .add(VIFCombinable::BackwardFlow)
            .set(StorageNr(16))
            );

    }
}

// Test: qualco qualcosonic 03016408 NOKEY
// telegram=|76440907086401030B0d7a78000000046d030fB726346d0000010134fd170000000004200f13cf0104240f13cf0104863B0000000004863cdc0000000413B5150600042B86f1ffff043B030200000259c002025d2c05026194fd0c780864010384086d3B17Bf258408863B000000008408863c0B000000|
// {"media":"heat/cooling load","meter":"qualcosonic","name":"qualco","id":"03016408","fabrication_no":"03016408","operating_time_h":8430.013056,"on_time_h":8430.013056,"meter_datetime":"2021-06-23 15:03","meter_datetime_at_error":"2000-01-01 00:00","total_m3":398.773,"flow_temperature_c":7.04,"return_temperature_c":13.24,"flow_return_temperature_difference_c":-6.2,"volume_flow_m3h":0.515,"status":"OK","total_heat_energy_kwh":0,"total_cooling_energy_kwh":220,"power_kw":-3.706,"target_datetime":"2021-05-31 23:59","target_heat_energy_kwh":0,"target_cooling_energy_kwh":11,"timestamp":"1111-11-11T11:11:11Z"}
// |qualco;03016408;OK;0.000000;220.000000;-3.706000;2021-05-31 23:59;0.000000;11.000000;1111-11-11 11:11.11
