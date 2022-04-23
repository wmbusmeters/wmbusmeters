/*
 Copyright (C) 2021 Vincent Privat (gpl-3.0-or-later)
               2022 Fredrik Öhrström (gpl-3.0-or-later)

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

struct MeterSharky : public virtual MeterCommonImplementation
{
    MeterSharky(MeterInfo &mi, DriverInfo &di);
};

static bool ok = registerDriver([](DriverInfo&di)
{
    di.setName("sharky");
    di.setMeterType(MeterType::HeatMeter);
    di.setExpectedTPLSecurityMode(TPLSecurityMode::AES_CBC_IV);
    di.addLinkMode(LinkMode::T1);
    di.addDetection(MANUFACTURER_HYD, 0x04, 0x20);
    di.setConstructor([](MeterInfo& mi, DriverInfo& di){ return shared_ptr<Meter>(new MeterSharky(mi, di)); });
});

MeterSharky::MeterSharky(MeterInfo &mi, DriverInfo &di) : MeterCommonImplementation(mi, di)
{
    addNumericFieldWithExtractor(
        "total_energy_consumption",
        "The total heat energy consumption recorded by this meter.",
        PrintProperty::JSON | PrintProperty::FIELD | PrintProperty::IMPORTANT,
        Quantity::Energy,
        VifScaling::Auto,
        FieldMatcher::build()
        .set(MeasurementType::Instantaneous)
        .set(VIFRange::EnergyWh)
        );

    addNumericFieldWithExtractor(
        "total_energy_consumption_tariff1",
        "The total heat energy consumption recorded by this meter on tariff 1.",
        PrintProperty::JSON | PrintProperty::FIELD,
        Quantity::Energy,
        VifScaling::Auto,
        FieldMatcher::build()
        .set(MeasurementType::Instantaneous)
        .set(VIFRange::EnergyWh)
        .set(TariffNr(1))
        );

    addNumericFieldWithExtractor(
        "total_volume",
        "The total heating media volume recorded by this meter.",
        PrintProperty::JSON | PrintProperty::FIELD,
        Quantity::Volume,
        VifScaling::Auto,
        FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::Volume)
        );

    addNumericFieldWithExtractor(
        "total_volume_tariff2",
        "The total heating media volume recorded by this meter on tariff 2.",
        PrintProperty::JSON | PrintProperty::FIELD,
        Quantity::Volume,
        VifScaling::Auto,
        FieldMatcher::build()
        .set(MeasurementType::Instantaneous)
        .set(VIFRange::Volume)
        .set(TariffNr(2))
        );

    addNumericFieldWithExtractor(
        "volume_flow",
        "The current heat media volume flow.",
        PrintProperty::JSON | PrintProperty::FIELD,
        Quantity::Flow,
        VifScaling::Auto,
        FieldMatcher::build()
        .set(MeasurementType::Instantaneous)
        .set(VIFRange::VolumeFlow)
        );

    addNumericFieldWithExtractor(
        "power",
        "The current power consumption.",
        PrintProperty::JSON | PrintProperty::FIELD,
        Quantity::Power,
        VifScaling::Auto,
        FieldMatcher::build()
        .set(MeasurementType::Instantaneous)
        .set(VIFRange::PowerW)
        );

    addNumericFieldWithExtractor(
        "flow_temperature",
        "The current forward heat media temperature.",
        PrintProperty::JSON | PrintProperty::FIELD,
        Quantity::Temperature,
        VifScaling::Auto,
        FieldMatcher::build()
        .set(MeasurementType::Instantaneous)
        .set(VIFRange::FlowTemperature)
        );

    addNumericFieldWithExtractor(
        "return_temperature",
        "The current return heat media temperature.",
        PrintProperty::JSON | PrintProperty::FIELD,
        Quantity::Temperature,
        VifScaling::Auto,
        FieldMatcher::build()
        .set(MeasurementType::Instantaneous)
        .set(VIFRange::ReturnTemperature)
        );

    addNumericFieldWithExtractor(
        "temperature_difference",
        "The current return heat media temperature.",
        PrintProperty::JSON | PrintProperty::FIELD,
        Quantity::Temperature,
        VifScaling::Auto,
        FieldMatcher::build()
        .set(MeasurementType::Instantaneous)
        .set(VIFRange::TemperatureDifference)
        );
}

// Test: Heat sharky ANYID NOKEY
// telegram=|534424232004256092687A370045752235854DEEEA5939FAD81C25FEEF5A23C38FB9168493C563F08DB10BAF87F660FBA91296BA2397E8F4220B86D3A192FB51E0BFCF24DCE72118E0C75A9E89F43BDFE370824B|
// {"media":"heat","meter":"sharky","name":"Heat","id":"68926025","total_energy_consumption_kwh":2651,"total_energy_consumption_tariff1_kwh":0,"total_volume_m3":150.347,"total_volume_tariff2_m3":0.018,"volume_flow_m3h":0,"power_kw":0,"flow_temperature_c":42.3,"return_temperature_c":28.1,"temperature_difference_c":14.1,"timestamp":"1111-11-11T11:11:11Z"}
// |Heat;68926025;2651.000000;0.000000;150.347000;0.018000;0.000000;0.000000;42.300000;28.100000;14.100000;1111-11-11 11:11.11
