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
        di.setName("elf");
        di.setDefaultFields("name,id,total_energy_consumption_kwh,current_power_consumption_kw,total_volume_m3,flow_temperature_c,return_temperature_c,external_temperature_c,status,timestamp");
        di.setMeterType(MeterType::HeatMeter);
        di.addDetection(MANUFACTURER_APA, 0x04, 0x40);
        di.addMfctTPLStatusBits(
            {
                {
                    {
                        "TPL_STS",
                        Translate::Type::BitToString,
                        0xe0, // Always use 0xe0 for tpl mfct status bits.
                        "OK",
                        {
                        }
                    },
                },
            });
        di.setConstructor([](MeterInfo& mi, DriverInfo& di){ return shared_ptr<Meter>(new Driver(mi, di)); });
    });

    Driver::Driver(MeterInfo &mi, DriverInfo &di) : MeterCommonImplementation(mi, di)
    {
        addStringFieldWithExtractorAndLookup(
            "status",
            "Meter status from manufacturer status and tpl status field.",
            PrintProperty::JSON | PrintProperty::FIELD | PrintProperty::IMPORTANT |
            PrintProperty::STATUS | PrintProperty::JOIN_TPL_STATUS,
            FieldMatcher::build()
            .set(DifVifKey("047F")),
            {
                {
                    {
                        "ERROR_FLAGS",
                        Translate::Type::BitToString,
                        0xffffffff,
                        "OK",
                        {
                        }
                    },
                },
            });


        addStringFieldWithExtractor(
            "meter_date",
            "The meter date.",
            PrintProperty::JSON | PrintProperty::OPTIONAL,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::Date)
            );

        addNumericFieldWithExtractor(
            "total_energy_consumption",
            "The total energy consumption recorded by this meter.",
            PrintProperty::FIELD | PrintProperty::JSON | PrintProperty::IMPORTANT,
            Quantity::Energy,
            VifScaling::Auto,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::EnergyWh)
            );

        addNumericFieldWithExtractor(
            "current_power_consumption",
             "Current power consumption.",
            PrintProperty::FIELD | PrintProperty::JSON,
            Quantity::Power,
            VifScaling::Auto,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::PowerW)
            );

        addNumericFieldWithExtractor(
            "total_volume",
            "Total volume of heat media.",
            PrintProperty::FIELD | PrintProperty::JSON,
            Quantity::Volume,
            VifScaling::Auto,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::Volume)
            );

        addNumericFieldWithExtractor(
            "total_energy_consumption_at_date",
            "The total energy consumption recorded at the target date.",
            PrintProperty::FIELD | PrintProperty::JSON,
            Quantity::Energy,
            VifScaling::Auto,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::EnergyWh)
            .set(StorageNr(1))
            );

        addNumericFieldWithExtractor(
            "flow_temperature",
             "The flow temperature.",
            PrintProperty::FIELD | PrintProperty::JSON,
            Quantity::Temperature,
            VifScaling::Auto,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::FlowTemperature)
            );

        addNumericFieldWithExtractor(
            "return_temperature",
             "The return temperature.",
            PrintProperty::FIELD | PrintProperty::JSON,
            Quantity::Temperature,
            VifScaling::Auto,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::ReturnTemperature)
            );

        addNumericFieldWithExtractor(
            "external_temperature",
             "The external temperature.",
            PrintProperty::FIELD | PrintProperty::JSON,
            Quantity::Temperature,
            VifScaling::Auto,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::ExternalTemperature)
            );

        addNumericFieldWithExtractor(
            "operating_time",
            "How long the meter has been collecting data.",
            PrintProperty::JSON | PrintProperty::OPTIONAL,
            Quantity::Time,
            VifScaling::Auto,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::OperatingTime)
            );

        addStringFieldWithExtractor(
            "version",
            "version.",
            PrintProperty::JSON | PrintProperty::OPTIONAL,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::ModelVersion)
            );

        addNumericFieldWithExtractor(
            "battery",
            "Battery voltage.",
            PrintProperty::JSON | PrintProperty::OPTIONAL,
            Quantity::Voltage,
            VifScaling::Auto,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::Voltage)
            );

    }
}

// Test: Hetta elf 01885619 NOKEY
// telegram=|51440186010905001837721956880101064004DA000020026CA9220E017799241103000C13641320000A2D00000A5A90060A5E800544050E77000001FD0C010A6564370AFD4731030A274907047F00000002|
// {"media":"heat","meter":"elf","name":"Hetta","id":"01885619","status":"ERROR_FLAGS_2000000","meter_date":"2021-02-09","total_energy_consumption_kwh":3112.49977,"current_power_consumption_kw":0,"total_volume_m3":201.364,"total_energy_consumption_at_date_kwh":3047.8,"flow_temperature_c":69,"return_temperature_c":58,"external_temperature_c":37.64,"operating_time_h":4.993333,"version":"01","battery_v":3.31,"timestamp":"1111-11-11T11:11:11Z"}
// |Hetta;01885619;3112.49977;0;201.364;69;58;37.64;ERROR_FLAGS_2000000;1111-11-11 11:11.11
