/*
 Copyright (C) 2021 Vincent Privat (gpl-3.0-or-later)
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

struct MeterSharky774 : public virtual MeterCommonImplementation
{
    MeterSharky774(MeterInfo &mi, DriverInfo &di);

private:

    double total_energy_consumption_kwh_ {};
    double total_volume_m3_ {};
    double volume_flow_m3h_ {};
    double power_kw_ {};
    double flow_temperature_c_ {};
    double return_temperature_c_ {};
    double energy_at_set_date_kwh_ {};
    double operating_time_h_ {};
    string set_date_txt_;
};

static bool ok = registerDriver([](DriverInfo&di)
{
    di.setName("sharky774");
    di.setMeterType(MeterType::HeatMeter);
    di.setExpectedTPLSecurityMode(TPLSecurityMode::AES_CBC_IV);
    di.addLinkMode(LinkMode::T1);
    di.addDetection(MANUFACTURER_DME, 0x04,  0x41);
    di.setConstructor([](MeterInfo& mi, DriverInfo& di){ return shared_ptr<Meter>(new MeterSharky774(mi, di)); });
});

MeterSharky774::MeterSharky774(MeterInfo &mi, DriverInfo &di) :  MeterCommonImplementation(mi, di)
{
    addNumericFieldWithExtractor(
        "total_energy_consumption",
        Quantity::Energy,
        NoDifVifKey,
        VifScaling::Auto,
        MeasurementType::Instantaneous,
        VIFRange::AnyEnergyVIF,
        StorageNr(0),
        TariffNr(0),
        IndexNr(1),
        PrintProperty::JSON | PrintProperty::FIELD | PrintProperty::IMPORTANT,
        "The total energy consumption recorded by this meter.",
        SET_FUNC(total_energy_consumption_kwh_, Unit::KWH),
        GET_FUNC(total_energy_consumption_kwh_, Unit::KWH));

    addNumericFieldWithExtractor(
        "total_volume",
        Quantity::Volume,
        NoDifVifKey,
        VifScaling::Auto,
        MeasurementType::Instantaneous,
        VIFRange::AnyVolumeVIF,
        StorageNr(0),
        TariffNr(0),
        IndexNr(1),
        PrintProperty::JSON | PrintProperty::FIELD,
        "The total volume recorded by this meter.",
        SET_FUNC(total_volume_m3_, Unit::M3),
        GET_FUNC(total_volume_m3_, Unit::M3));

    addNumericFieldWithExtractor(
        "volume_flow",
        Quantity::Flow,
        NoDifVifKey,
        VifScaling::Auto,
        MeasurementType::Instantaneous,
        VIFRange::VolumeFlow,
        StorageNr(0),
        TariffNr(0),
        IndexNr(1),
        PrintProperty::JSON | PrintProperty::FIELD,
        "The current flow.",
        SET_FUNC(volume_flow_m3h_, Unit::M3H),
        GET_FUNC(volume_flow_m3h_, Unit::M3H));

    addNumericFieldWithExtractor(
        "power",
        Quantity::Power,
        NoDifVifKey,
        VifScaling::AutoSigned,
        MeasurementType::Instantaneous,
        VIFRange::AnyPowerVIF,
        StorageNr(0),
        TariffNr(0),
        IndexNr(1),
        PrintProperty::JSON | PrintProperty::FIELD,
        "The power.",
        SET_FUNC(power_kw_, Unit::KW),
        GET_FUNC(power_kw_, Unit::KW));

    addNumericFieldWithExtractor(
        "flow_temperature",
        Quantity::Temperature,
        NoDifVifKey,
        VifScaling::Auto,
        MeasurementType::Instantaneous,
        VIFRange::FlowTemperature,
        StorageNr(0),
        TariffNr(0),
        IndexNr(1),
        PrintProperty::JSON | PrintProperty::FIELD,
        "The flow temperature.",
        SET_FUNC(flow_temperature_c_, Unit::C),
        GET_FUNC(flow_temperature_c_, Unit::C));

    addNumericFieldWithExtractor(
        "return_temperature",
        Quantity::Temperature,
        NoDifVifKey,
        VifScaling::Auto,
        MeasurementType::Instantaneous,
        VIFRange::ReturnTemperature,
        StorageNr(0),
        TariffNr(0),
        IndexNr(1),
        PrintProperty::JSON | PrintProperty::FIELD,
        "The return temperature.",
        SET_FUNC(return_temperature_c_, Unit::C),
        GET_FUNC(return_temperature_c_, Unit::C));

    addNumericField("temperature_difference",
             Quantity::Temperature,
             PrintProperty::JSON | PrintProperty::FIELD,
             "The temperature difference.",
             [](Unit u, double v) {},
             [this](Unit u) { return flow_temperature_c_ - return_temperature_c_; });

    addNumericFieldWithExtractor(
        "operating_time",
        Quantity::Time,
        DifVifKey("0AA618"),
        VifScaling::Auto,
        MeasurementType::Instantaneous,
        VIFRange::None,
        StorageNr(0),
        TariffNr(0),
        IndexNr(1),
        PrintProperty::JSON,
        "The operating time.",
        SET_FUNC(operating_time_h_, Unit::Hour),
        GET_FUNC(operating_time_h_, Unit::Hour));

    addNumericFieldWithExtractor(
        "energy_at_set_date",
        Quantity::Energy,
        NoDifVifKey,
        VifScaling::Auto,
        MeasurementType::Instantaneous,
        VIFRange::AnyEnergyVIF,
        StorageNr(1),
        TariffNr(0),
        IndexNr(1),
        PrintProperty::JSON | PrintProperty::FIELD,
        "The total energy consumption recorded by this meter at the set date.",
        SET_FUNC(energy_at_set_date_kwh_, Unit::KWH),
        GET_FUNC(energy_at_set_date_kwh_, Unit::KWH));

    addStringFieldWithExtractor(
        "set_date",
        Quantity::Text,
        NoDifVifKey,
        MeasurementType::Instantaneous,
        VIFRange::Date,
        StorageNr(1),
        TariffNr(0),
        IndexNr(1),
        PrintProperty::JSON,
        "The last billing set date.",
        SET_STRING_FUNC(set_date_txt_),
        GET_STRING_FUNC(set_date_txt_));

}

// Test: Heato sharky774 58496405 NOKEY
// telegram=3E44A5110564495841047A700030052F2F#0C06846800000C13195364000B3B0400000C2B110100000A5A17050A5E76020AA61800004C0647630000426CBF25
// {"media":"heat","meter":"sharky774","name":"Heato","id":"58496405","total_energy_consumption_kwh":6884,"total_volume_m3":645.319,"volume_flow_m3h":0.004,"power_kw":0.111,"flow_temperature_c":51.7,"return_temperature_c":27.6,"temperature_difference_c":24.1,"operating_time_h":0,"energy_at_set_date_kwh":6347,"set_date":"2021-05-31","timestamp":"1111-11-11T11:11:11Z"}
// |Heato;58496405;6884.000000;645.319000;0.004000;0.111000;51.700000;27.600000;24.100000;6347.000000;1111-11-11 11:11.11

// This test telegram has more historical data!
// Test: Heatoo sharky774 72615127 NOKEY
// telegram=|5E44A5112751617241047A8B0050052F2F0C0E000000000C13010000000B3B0000000C2B000000000A5A26020A5E18020B260321000AA6180000C2026CBE2BCC020E00000000CC021301000000DB023B000000DC022B000000002F2F2F2F2F|
// {"media":"heat","meter":"sharky774","name":"Heatoo","id":"72615127","total_energy_consumption_kwh":0,"total_volume_m3":0.001,"volume_flow_m3h":0,"power_kw":0,"flow_temperature_c":22.6,"return_temperature_c":21.8,"temperature_difference_c":0.8,"operating_time_h":0,"energy_at_set_date_kwh":0,"set_date":"","timestamp":"1111-11-11T11:11:11Z"}
// |Heatoo;72615127;0.000000;0.001000;0.000000;0.000000;22.600000;21.800000;0.800000;0.000000;1111-11-11 11:11.11

// This telegram contains a negative power value encoded in bcd.
// Test: Heatooo sharky774 61243590 NOKEY
// telegram=3E44A5119035246141047A1A0030052F2F#0C06026301000C13688609040B3B0802000C2B220000F00A5A71020A5E72020AA61800004C0636370100426CBF25
// {"media":"heat","meter":"sharky774","name":"Heatooo","id":"61243590","total_energy_consumption_kwh":16302,"total_volume_m3":4098.668,"volume_flow_m3h":0.208,"power_kw":-0.022,"flow_temperature_c":27.1,"return_temperature_c":27.2,"temperature_difference_c":-0.1,"operating_time_h":0,"energy_at_set_date_kwh":13736,"set_date":"2021-05-31","timestamp":"1111-11-11T11:11:11Z"}
// |Heatooo;61243590;16302.000000;4098.668000;0.208000;-0.022000;27.100000;27.200000;-0.100000;13736.000000;1111-11-11 11:11.11
