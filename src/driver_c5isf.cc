/*
 Copyright (C) 2022 Fredrik Öhrström

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

struct MeterC5isf : public virtual MeterCommonImplementation
{
    MeterC5isf(MeterInfo &mi, DriverInfo &di);

private:
    double total_energy_kwh_ {};
    double total_volume_m3_ {};
    double due_energy_kwh_ {};
    string due_date_;
    double volume_flow_m3h_ {};
    double power_kw_ {};
    double total_energy_last_month_kwh_ {};
    string last_month_date_;
    double max_power_last_month_kw_ {};
    double flow_temperature_c_ {};
    double return_temperature_c_ {};
    string status_;
};

static bool ok = registerDriver([](DriverInfo&di)
{
    di.setName("c5isf");
    di.setMeterType(MeterType::HeatMeter);
    di.setExpectedTPLSecurityMode(TPLSecurityMode::AES_CBC_IV);
    di.addLinkMode(LinkMode::T1);
    di.addDetection(MANUFACTURER_ZRI, 0x04, 0x88);
    di.setConstructor([](MeterInfo& mi, DriverInfo& di){ return shared_ptr<Meter>(new MeterC5isf(mi, di)); });
});

MeterC5isf::MeterC5isf(MeterInfo &mi, DriverInfo &di) : MeterCommonImplementation(mi, di)
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
        "total_volume",
        Quantity::Volume,
        NoDifVifKey,
        VifScaling::Auto,
        MeasurementType::Instantaneous,
        ValueInformation::Volume,
        StorageNr(0),
        TariffNr(0),
        IndexNr(1),
        PrintProperty::JSON,
        "The total heating media volume recorded by this meter.",
        SET_FUNC(total_volume_m3_, Unit::M3),
        GET_FUNC(total_volume_m3_, Unit::M3));

    addFieldWithExtractor(
        "due_energy_consumption",
        Quantity::Energy,
        NoDifVifKey,
        VifScaling::Auto,
        MeasurementType::Instantaneous,
        ValueInformation::EnergyWh,
        StorageNr(8),
        TariffNr(0),
        IndexNr(1),
        PrintProperty::JSON,
        "The total heat energy consumption at the due date.",
        SET_FUNC(due_energy_kwh_, Unit::KWH),
        GET_FUNC(due_energy_kwh_, Unit::KWH));

    addStringFieldWithExtractor(
        "due_date",
        Quantity::Text,
        NoDifVifKey,
        MeasurementType::Instantaneous,
        ValueInformation::Date,
        StorageNr(8),
        TariffNr(0),
        IndexNr(1),
        PrintProperty::JSON,
        "The due date.",
        SET_STRING_FUNC(due_date_),
        GET_STRING_FUNC(due_date_));

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
        PrintProperty::JSON,
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
        "total_energy_consumption_last_month",
        Quantity::Energy,
        NoDifVifKey,
        VifScaling::Auto,
        MeasurementType::Instantaneous,
        ValueInformation::EnergyWh,
        StorageNr(32),
        TariffNr(0),
        IndexNr(1),
        PrintProperty::JSON,
        "The total heat energy consumption recorded at end of last month.",
        SET_FUNC(total_energy_last_month_kwh_, Unit::KWH),
        GET_FUNC(total_energy_last_month_kwh_, Unit::KWH));

    addStringFieldWithExtractor(
        "last_month_date",
        Quantity::Text,
        NoDifVifKey,
        MeasurementType::Instantaneous,
        ValueInformation::Date,
        StorageNr(32),
        TariffNr(0),
        IndexNr(1),
        PrintProperty::JSON,
        "The due date.",
        SET_STRING_FUNC(last_month_date_),
        GET_STRING_FUNC(last_month_date_));

    addFieldWithExtractor(
        "max_power_last_month",
        Quantity::Power,
        NoDifVifKey,
        VifScaling::Auto,
        MeasurementType::Maximum,
        ValueInformation::PowerW,
        StorageNr(32),
        TariffNr(0),
        IndexNr(1),
        PrintProperty::JSON,
        "Maximum power consumption last month.",
        SET_FUNC(max_power_last_month_kw_, Unit::KW),
        GET_FUNC(max_power_last_month_kw_, Unit::KW));

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
        PrintProperty::JSON,
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

    addStringFieldWithExtractorAndLookup(
        "status",
        Quantity::Text,
        DifVifKey("02FD17"),
        MeasurementType::Unknown,
        ValueInformation::Any,
        AnyStorageNr,
        AnyTariffNr,
        IndexNr(1),
        PrintProperty::JSON | PrintProperty::FIELD,
        "Status and error flags.",
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

}

// Test: Heat c5isf ANYID NOKEY
// telegram=|5E44496A4420003288047A0A0050052F2F#04061A0000000413C20800008404060000000082046CC121043BA4000000042D1900000002591216025DE21002FD17000084800106000000008280016CC121948001AE25000000002F2F2F2F2F2F|
// {"media":"heat","meter":"c5isf","name":"Heat","id":"32002044","total_energy_consumption_kwh":26,"total_volume_m3":2.242,"due_energy_consumption_kwh":0,"due_date":"2022-01-01","volume_flow_m3h":0.164,"power_kw":2.5,"total_energy_consumption_last_month_kwh":0,"last_month_date":"2022-01-01","max_power_last_month_kw":0,"flow_temperature_c":56.5,"return_temperature_c":43.22,"status":"OK","timestamp":"1111-11-11T11:11:11Z"}
// |Heat;32002044;26.000000;2.500000;43.220000;OK;1111-11-11 11:11.11
