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

struct MeterMicroClima : public virtual MeterCommonImplementation
{
    MeterMicroClima(MeterInfo &mi, DriverInfo &di);

private:
    double total_energy_kwh_ {};
    double total_energy_tariff1_kwh_ {};
    double total_volume_m3_ {};
    double total_volume_tariff2_m3_ {};
    double volume_flow_m3h_ {};
    double power_kw_ {};
    double flow_temperature_c_ {};
    double return_temperature_c_ {};
    double temperature_difference_k_ {};
    string status_;
    string device_date_time_;
};

static bool ok = registerDriver([](DriverInfo&di)
{
    di.setName("microclima");
    di.setMeterType(MeterType::HeatMeter);
    di.setExpectedTPLSecurityMode(TPLSecurityMode::AES_CBC_IV);
    di.addLinkMode(LinkMode::T1);
    di.addDetection(MANUFACTURER_MAD, 0x04, 0x00);
    di.setConstructor([](MeterInfo& mi, DriverInfo& di){ return shared_ptr<Meter>(new MeterMicroClima(mi, di)); });
});

MeterMicroClima::MeterMicroClima(MeterInfo &mi, DriverInfo &di) : MeterCommonImplementation(mi, di)
{
    addFieldWithExtractor(
        "total_energy_consumption",
        Quantity::Energy,
        NoDifVifKey,
        VifScaling::Auto,
        MeasurementType::Instantaneous,
        ValueInformation::EnergyWh,
        StorageNr(0),
        TariffNr(0),
        IndexNr(1),
        PrintProperty::JSON | PrintProperty::FIELD | PrintProperty::IMPORTANT,
        "The total heat energy consumption recorded by this meter.",
        SET_FUNC(total_energy_kwh_, Unit::KWH),
        GET_FUNC(total_energy_kwh_, Unit::KWH));

    addFieldWithExtractor(
        "total_energy_consumption_tariff1",
        Quantity::Energy,
        NoDifVifKey,
        VifScaling::Auto,
        MeasurementType::Instantaneous,
        ValueInformation::EnergyWh,
        StorageNr(0),
        TariffNr(1),
        IndexNr(1),
        PrintProperty::JSON | PrintProperty::FIELD,
        "The total heat energy consumption recorded by this meter on tariff 1.",
        SET_FUNC(total_energy_tariff1_kwh_, Unit::KWH),
        GET_FUNC(total_energy_tariff1_kwh_, Unit::KWH));

    addFieldWithExtractor(
        "total_volume",
        Quantity::Volume,
        NoDifVifKey,
        VifScaling::Auto,
        MeasurementType::Instantaneous,
        ValueInformation::Volume,
        StorageNr(0),
        TariffNr(0),
        IndexNr(1),
        PrintProperty::JSON | PrintProperty::FIELD,
        "The total heating media volume recorded by this meter.",
        SET_FUNC(total_volume_m3_, Unit::M3),
        GET_FUNC(total_volume_m3_, Unit::M3));

    addFieldWithExtractor(
        "total_volume_tariff2",
        Quantity::Volume,
        NoDifVifKey,
        VifScaling::Auto,
        MeasurementType::Instantaneous,
        ValueInformation::Volume,
        StorageNr(0),
        TariffNr(2),
        IndexNr(1),
        PrintProperty::JSON | PrintProperty::FIELD,
        "The total heating media volume recorded by this meter on tariff 2.",
        SET_FUNC(total_volume_tariff2_m3_, Unit::M3),
        GET_FUNC(total_volume_tariff2_m3_, Unit::M3));

    addFieldWithExtractor(
        "volume_flow",
        Quantity::Flow,
        NoDifVifKey,
        VifScaling::Auto,
        MeasurementType::Instantaneous,
        ValueInformation::VolumeFlow,
        StorageNr(0),
        TariffNr(0),
        IndexNr(1),
        PrintProperty::JSON | PrintProperty::FIELD,
        "The current heat media volume flow.",
        SET_FUNC(volume_flow_m3h_, Unit::M3H),
        GET_FUNC(volume_flow_m3h_, Unit::M3H));

    addFieldWithExtractor(
        "power",
        Quantity::Power,
        NoDifVifKey,
        VifScaling::Auto,
        MeasurementType::Instantaneous,
        ValueInformation::PowerW,
        StorageNr(0),
        TariffNr(0),
        IndexNr(1),
        PrintProperty::JSON | PrintProperty::FIELD,
        "The current power consumption.",
        SET_FUNC(power_kw_, Unit::KW),
        GET_FUNC(power_kw_, Unit::KW));

    addFieldWithExtractor(
        "flow_temperature",
        Quantity::Temperature,
        NoDifVifKey,
        VifScaling::Auto,
        MeasurementType::Instantaneous,
        ValueInformation::FlowTemperature,
        StorageNr(0),
        TariffNr(0),
        IndexNr(1),
        PrintProperty::JSON | PrintProperty::FIELD,
        "The current forward heat media temperature.",
        SET_FUNC(flow_temperature_c_, Unit::C),
        GET_FUNC(flow_temperature_c_, Unit::C));

    addFieldWithExtractor(
        "return_temperature",
        Quantity::Temperature,
        NoDifVifKey,
        VifScaling::Auto,
        MeasurementType::Instantaneous,
        ValueInformation::ReturnTemperature,
        StorageNr(0),
        TariffNr(0),
        IndexNr(1),
        PrintProperty::JSON | PrintProperty::FIELD,
        "The current return heat media temperature.",
        SET_FUNC(return_temperature_c_, Unit::C),
        GET_FUNC(return_temperature_c_, Unit::C));

    addFieldWithExtractor(
        "temperature_difference",
        Quantity::Temperature,
        NoDifVifKey,
        VifScaling::AutoSigned,
        MeasurementType::Instantaneous,
        ValueInformation::TemperatureDifference,
        StorageNr(0),
        TariffNr(0),
        IndexNr(1),
        PrintProperty::JSON | PrintProperty::FIELD,
        "The current return heat media temperature.",
        SET_FUNC(temperature_difference_k_, Unit::K),
        GET_FUNC(temperature_difference_k_, Unit::K));

    addStringFieldWithExtractorAndLookup(
        "status",
        Quantity::Text,
        DifVifKey("01FD17"),
        MeasurementType::Unknown,
        ValueInformation::Any,
        AnyStorageNr,
        AnyTariffNr,
        IndexNr(1),
        PrintProperty::JSON | PrintProperty::FIELD,
        "Error flags.",
        SET_STRING_FUNC(status_),
        GET_STRING_FUNC(status_),
         {
            {
                {
                    "ERROR_FLAGS",
                    Translate::Type::BitToString,
                    0xffff,
                    "OK",
                    {
                        { 0x01, "?" },
                    }
                },
            },
         });

    addStringFieldWithExtractor(
        "device_date_time",
        Quantity::Text,
        NoDifVifKey,
        MeasurementType::Instantaneous,
        ValueInformation::DateTime,
        StorageNr(0),
        TariffNr(0),
        IndexNr(1),
        PrintProperty::JSON,
        "Device date time.",
        SET_STRING_FUNC(device_date_time_),
        GET_STRING_FUNC(device_date_time_));

}

// The meter sends two types of telegrams a shorter with current values.

// Test: Heat microclima 93572431 NOKEY
// telegram=|494424343124579300047a5a0000202f2f046d2720b62c04060d07000001fd170004130a8c0400043b00000000042b00000000025b1500025f15000261d0ff03fd0c05000002fd0b1011|
// {"media":"heat","meter":"microclima","name":"Heat","id":"93572431","total_energy_consumption_kwh":1805,"total_energy_consumption_tariff1_kwh":0,"total_volume_m3":297.994,"total_volume_tariff2_m3":0,"volume_flow_m3h":0,"power_kw":0,"flow_temperature_c":21,"return_temperature_c":21,"temperature_difference_c":-0.48,"status":"OK","device_date_time":"2021-12-22 00:39","timestamp":"1111-11-11T11:11:11Z"}
// |Heat;93572431;1805.000000;0.000000;297.994000;0.000000;0.000000;0.000000;21.000000;21.000000;-0.480000;OK;1111-11-11 11:11.11


// And a longer with historical values. This telegram is not yet properly decoded.

// Test: Heat microclima 93573086 NOKEY
// telegram=|A44424348630579300047ADD0000202F2F046D0721B62C04064708000004135DB0030001FD1700426C9F2C4406C6040000C40106C0070000C4020637070000C4030611070000C404060B070000C405060B070000C406060B070000C407060B070000C40806A5060000C40906F7050000C40A067A050000C40B060F050000C40C06C6040000C40D063F040000C40E06BB030000C40F06A502000003FD0C05000002FD0B1111|
// {"media":"heat","meter":"microclima","name":"Heat","id":"93573086","total_energy_consumption_kwh":2119,"total_energy_consumption_tariff1_kwh":0,"total_volume_m3":241.757,"total_volume_tariff2_m3":0,"volume_flow_m3h":0,"power_kw":0,"flow_temperature_c":0,"return_temperature_c":0,"temperature_difference_c":-273.15,"status":"OK","device_date_time":"2021-12-22 01:07","timestamp":"1111-11-11T11:11:11Z"}
// |Heat;93573086;2119.000000;0.000000;241.757000;0.000000;0.000000;0.000000;0.000000;0.000000;-273.150000;OK;1111-11-11 11:11.11
