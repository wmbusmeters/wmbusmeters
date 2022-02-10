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

struct MeterC5isf_t1a1 : public virtual MeterCommonImplementation
{
MeterC5isf_t1a1(MeterInfo &mi, DriverInfo &di);

private:
    double total_energy_kwh_ {};
    double total_volume_m3_ {};
    string status_;
    double total_energy_last_month_kwh_ {};
    string last_month_date_;
    double total_energy_last_month_2_kwh_ {};
    string last_month_2_date_;
    double total_energy_last_month_3_kwh_ {};
    string last_month_3_date_;
    double total_energy_last_month_4_kwh_ {};
    string last_month_4_date_;
    double total_energy_last_month_5_kwh_ {};
    string last_month_5_date_;
    double total_energy_last_month_6_kwh_ {};
    string last_month_6_date_;
    double total_energy_last_month_7_kwh_ {};
    string last_month_7_date_;
    double total_energy_last_month_8_kwh_ {};
    string last_month_8_date_;
    double total_energy_last_month_9_kwh_ {};
    string last_month_9_date_;
    double total_energy_last_month_10_kwh_ {};
    string last_month_10_date_;
    double total_energy_last_month_11_kwh_ {};
    string last_month_11_date_;
    double total_energy_last_month_12_kwh_ {};
    string last_month_12_date_;
    double total_energy_last_month_13_kwh_ {};
    string last_month_13_date_;
    double total_energy_last_month_14_kwh_ {};
    string last_month_14_date_;
};

static bool ok = registerDriver([](DriverInfo&di)
{
    di.setName("c5isf_t1a1");
    di.setMeterType(MeterType::HeatMeter);
    di.setExpectedTPLSecurityMode(TPLSecurityMode::AES_CBC_IV);
    di.addLinkMode(LinkMode::T1);
    di.addDetection(MANUFACTURER_ZRI, 0x0d, 0x88);
    di.setConstructor([](MeterInfo& mi, DriverInfo& di){ return shared_ptr<Meter>(new MeterC5isf_t1a1(mi, di)); });
});

MeterC5isf_t1a1::MeterC5isf_t1a1(MeterInfo &mi, DriverInfo &di) : MeterCommonImplementation(mi, di)
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
        "total_energy_consumption_month-2",
        Quantity::Energy,
        NoDifVifKey,
        VifScaling::Auto,
        MeasurementType::Instantaneous,
        ValueInformation::EnergyWh,
        StorageNr(33),
        TariffNr(0),
        IndexNr(1),
        PrintProperty::JSON,
        "The total heat energy consumption recorded at end of last month.",
        SET_FUNC(total_energy_last_month_2_kwh_, Unit::KWH),
        GET_FUNC(total_energy_last_month_2_kwh_, Unit::KWH));

    addStringFieldWithExtractor(
        "month-2_date",
        Quantity::Text,
        NoDifVifKey,
        MeasurementType::Instantaneous,
        ValueInformation::Date,
        StorageNr(33),
        TariffNr(0),
        IndexNr(1),
        PrintProperty::JSON,
        "The due date.",
        SET_STRING_FUNC(last_month_2_date_),
        GET_STRING_FUNC(last_month_2_date_));


    addFieldWithExtractor(
        "total_energy_consumption_month-3",
        Quantity::Energy,
        NoDifVifKey,
        VifScaling::Auto,
        MeasurementType::Instantaneous,
        ValueInformation::EnergyWh,
        StorageNr(34),
        TariffNr(0),
        IndexNr(1),
        PrintProperty::JSON,
        "The total heat energy consumption recorded at end of last month.",
        SET_FUNC(total_energy_last_month_3_kwh_, Unit::KWH),
        GET_FUNC(total_energy_last_month_3_kwh_, Unit::KWH));

    addStringFieldWithExtractor(
        "month-3_date",
        Quantity::Text,
        NoDifVifKey,
        MeasurementType::Instantaneous,
        ValueInformation::Date,
        StorageNr(34),
        TariffNr(0),
        IndexNr(1),
        PrintProperty::JSON,
        "The due date.",
        SET_STRING_FUNC(last_month_3_date_),
        GET_STRING_FUNC(last_month_3_date_));


    addFieldWithExtractor(
        "total_energy_consumption_month-4",
        Quantity::Energy,
        NoDifVifKey,
        VifScaling::Auto,
        MeasurementType::Instantaneous,
        ValueInformation::EnergyWh,
        StorageNr(35),
        TariffNr(0),
        IndexNr(1),
        PrintProperty::JSON,
        "The total heat energy consumption recorded at end of last month.",
        SET_FUNC(total_energy_last_month_4_kwh_, Unit::KWH),
        GET_FUNC(total_energy_last_month_4_kwh_, Unit::KWH));

    addStringFieldWithExtractor(
        "month-4_date",
        Quantity::Text,
        NoDifVifKey,
        MeasurementType::Instantaneous,
        ValueInformation::Date,
        StorageNr(35),
        TariffNr(0),
        IndexNr(1),
        PrintProperty::JSON,
        "The due date.",
        SET_STRING_FUNC(last_month_4_date_),
        GET_STRING_FUNC(last_month_4_date_));


    addFieldWithExtractor(
        "total_energy_consumption_month-5",
        Quantity::Energy,
        NoDifVifKey,
        VifScaling::Auto,
        MeasurementType::Instantaneous,
        ValueInformation::EnergyWh,
        StorageNr(36),
        TariffNr(0),
        IndexNr(1),
        PrintProperty::JSON,
        "The total heat energy consumption recorded at end of last month.",
        SET_FUNC(total_energy_last_month_5_kwh_, Unit::KWH),
        GET_FUNC(total_energy_last_month_5_kwh_, Unit::KWH));

    addStringFieldWithExtractor(
        "month-5_date",
        Quantity::Text,
        NoDifVifKey,
        MeasurementType::Instantaneous,
        ValueInformation::Date,
        StorageNr(36),
        TariffNr(0),
        IndexNr(1),
        PrintProperty::JSON,
        "The due date.",
        SET_STRING_FUNC(last_month_5_date_),
        GET_STRING_FUNC(last_month_5_date_));


    addFieldWithExtractor(
        "total_energy_consumption_month-6",
        Quantity::Energy,
        NoDifVifKey,
        VifScaling::Auto,
        MeasurementType::Instantaneous,
        ValueInformation::EnergyWh,
        StorageNr(37),
        TariffNr(0),
        IndexNr(1),
        PrintProperty::JSON,
        "The total heat energy consumption recorded at end of last month.",
        SET_FUNC(total_energy_last_month_6_kwh_, Unit::KWH),
        GET_FUNC(total_energy_last_month_6_kwh_, Unit::KWH));

    addStringFieldWithExtractor(
        "month-6_date",
        Quantity::Text,
        NoDifVifKey,
        MeasurementType::Instantaneous,
        ValueInformation::Date,
        StorageNr(37),
        TariffNr(0),
        IndexNr(1),
        PrintProperty::JSON,
        "The due date.",
        SET_STRING_FUNC(last_month_6_date_),
        GET_STRING_FUNC(last_month_6_date_));


    addFieldWithExtractor(
        "total_energy_consumption_month-7",
        Quantity::Energy,
        NoDifVifKey,
        VifScaling::Auto,
        MeasurementType::Instantaneous,
        ValueInformation::EnergyWh,
        StorageNr(38),
        TariffNr(0),
        IndexNr(1),
        PrintProperty::JSON,
        "The total heat energy consumption recorded at end of last month.",
        SET_FUNC(total_energy_last_month_7_kwh_, Unit::KWH),
        GET_FUNC(total_energy_last_month_7_kwh_, Unit::KWH));

    addStringFieldWithExtractor(
        "month-7_date",
        Quantity::Text,
        NoDifVifKey,
        MeasurementType::Instantaneous,
        ValueInformation::Date,
        StorageNr(38),
        TariffNr(0),
        IndexNr(1),
        PrintProperty::JSON,
        "The due date.",
        SET_STRING_FUNC(last_month_7_date_),
        GET_STRING_FUNC(last_month_7_date_));


    addFieldWithExtractor(
        "total_energy_consumption_month-8",
        Quantity::Energy,
        NoDifVifKey,
        VifScaling::Auto,
        MeasurementType::Instantaneous,
        ValueInformation::EnergyWh,
        StorageNr(39),
        TariffNr(0),
        IndexNr(1),
        PrintProperty::JSON,
        "The total heat energy consumption recorded at end of last month.",
        SET_FUNC(total_energy_last_month_8_kwh_, Unit::KWH),
        GET_FUNC(total_energy_last_month_8_kwh_, Unit::KWH));

    addStringFieldWithExtractor(
        "month-8_date",
        Quantity::Text,
        NoDifVifKey,
        MeasurementType::Instantaneous,
        ValueInformation::Date,
        StorageNr(39),
        TariffNr(0),
        IndexNr(1),
        PrintProperty::JSON,
        "The due date.",
        SET_STRING_FUNC(last_month_8_date_),
        GET_STRING_FUNC(last_month_8_date_));


    addFieldWithExtractor(
        "total_energy_consumption_month-9",
        Quantity::Energy,
        NoDifVifKey,
        VifScaling::Auto,
        MeasurementType::Instantaneous,
        ValueInformation::EnergyWh,
        StorageNr(40),
        TariffNr(0),
        IndexNr(1),
        PrintProperty::JSON,
        "The total heat energy consumption recorded at end of last month.",
        SET_FUNC(total_energy_last_month_9_kwh_, Unit::KWH),
        GET_FUNC(total_energy_last_month_9_kwh_, Unit::KWH));

    addStringFieldWithExtractor(
        "month-9_date",
        Quantity::Text,
        NoDifVifKey,
        MeasurementType::Instantaneous,
        ValueInformation::Date,
        StorageNr(40),
        TariffNr(0),
        IndexNr(1),
        PrintProperty::JSON,
        "The due date.",
        SET_STRING_FUNC(last_month_9_date_),
        GET_STRING_FUNC(last_month_9_date_));


    addFieldWithExtractor(
        "total_energy_consumption_month-10",
        Quantity::Energy,
        NoDifVifKey,
        VifScaling::Auto,
        MeasurementType::Instantaneous,
        ValueInformation::EnergyWh,
        StorageNr(41),
        TariffNr(0),
        IndexNr(1),
        PrintProperty::JSON,
        "The total heat energy consumption recorded at end of last month.",
        SET_FUNC(total_energy_last_month_10_kwh_, Unit::KWH),
        GET_FUNC(total_energy_last_month_10_kwh_, Unit::KWH));

    addStringFieldWithExtractor(
        "month-10_date",
        Quantity::Text,
        NoDifVifKey,
        MeasurementType::Instantaneous,
        ValueInformation::Date,
        StorageNr(41),
        TariffNr(0),
        IndexNr(1),
        PrintProperty::JSON,
        "The due date.",
        SET_STRING_FUNC(last_month_10_date_),
        GET_STRING_FUNC(last_month_10_date_));


    addFieldWithExtractor(
        "total_energy_consumption_month-11",
        Quantity::Energy,
        NoDifVifKey,
        VifScaling::Auto,
        MeasurementType::Instantaneous,
        ValueInformation::EnergyWh,
        StorageNr(42),
        TariffNr(0),
        IndexNr(1),
        PrintProperty::JSON,
        "The total heat energy consumption recorded at end of last month.",
        SET_FUNC(total_energy_last_month_11_kwh_, Unit::KWH),
        GET_FUNC(total_energy_last_month_11_kwh_, Unit::KWH));

    addStringFieldWithExtractor(
        "month-11_date",
        Quantity::Text,
        NoDifVifKey,
        MeasurementType::Instantaneous,
        ValueInformation::Date,
        StorageNr(42),
        TariffNr(0),
        IndexNr(1),
        PrintProperty::JSON,
        "The due date.",
        SET_STRING_FUNC(last_month_11_date_),
        GET_STRING_FUNC(last_month_11_date_));


    addFieldWithExtractor(
        "total_energy_consumption_month-12",
        Quantity::Energy,
        NoDifVifKey,
        VifScaling::Auto,
        MeasurementType::Instantaneous,
        ValueInformation::EnergyWh,
        StorageNr(43),
        TariffNr(0),
        IndexNr(1),
        PrintProperty::JSON,
        "The total heat energy consumption recorded at end of last month.",
        SET_FUNC(total_energy_last_month_12_kwh_, Unit::KWH),
        GET_FUNC(total_energy_last_month_12_kwh_, Unit::KWH));

    addStringFieldWithExtractor(
        "month-12_date",
        Quantity::Text,
        NoDifVifKey,
        MeasurementType::Instantaneous,
        ValueInformation::Date,
        StorageNr(43),
        TariffNr(0),
        IndexNr(1),
        PrintProperty::JSON,
        "The due date.",
        SET_STRING_FUNC(last_month_12_date_),
        GET_STRING_FUNC(last_month_12_date_));


    addFieldWithExtractor(
        "total_energy_consumption_month-13",
        Quantity::Energy,
        NoDifVifKey,
        VifScaling::Auto,
        MeasurementType::Instantaneous,
        ValueInformation::EnergyWh,
        StorageNr(44),
        TariffNr(0),
        IndexNr(1),
        PrintProperty::JSON,
        "The total heat energy consumption recorded at end of last month.",
        SET_FUNC(total_energy_last_month_13_kwh_, Unit::KWH),
        GET_FUNC(total_energy_last_month_13_kwh_, Unit::KWH));

    addStringFieldWithExtractor(
        "month-13_date",
        Quantity::Text,
        NoDifVifKey,
        MeasurementType::Instantaneous,
        ValueInformation::Date,
        StorageNr(44),
        TariffNr(0),
        IndexNr(1),
        PrintProperty::JSON,
        "The due date.",
        SET_STRING_FUNC(last_month_13_date_),
        GET_STRING_FUNC(last_month_13_date_));


    addFieldWithExtractor(
        "total_energy_consumption_month-14",
        Quantity::Energy,
        NoDifVifKey,
        VifScaling::Auto,
        MeasurementType::Instantaneous,
        ValueInformation::EnergyWh,
        StorageNr(45),
        TariffNr(0),
        IndexNr(1),
        PrintProperty::JSON,
        "The total heat energy consumption recorded at end of last month.",
        SET_FUNC(total_energy_last_month_14_kwh_, Unit::KWH),
        GET_FUNC(total_energy_last_month_14_kwh_, Unit::KWH));

    addStringFieldWithExtractor(
        "month-14_date",
        Quantity::Text,
        NoDifVifKey,
        MeasurementType::Instantaneous,
        ValueInformation::Date,
        StorageNr(45),
        TariffNr(0),
        IndexNr(1),
        PrintProperty::JSON,
        "The due date.",
        SET_STRING_FUNC(last_month_14_date_),
        GET_STRING_FUNC(last_month_14_date_));


}

// Test: Heat c5isf_t1a1 ANYID NOKEY
// telegram=|544496A44554455880D7A320200002F2F04060000000004130000000002FD17240084800106000000008280016C2124C480010600000080C280016CFFFF84810106000000808281016CFFFFC481010600000080C281016CFFFF84820106000000808282016CFFFFC482010600000080C282016CFFFF84830106000000808283016CFFFFC483010600000080C283016CFFFF84840106000000808284016CFFFFC484010600000080C284016CFFFF84850106000000808285016CFFFFC485010600000080C285016CFFFF84860106000000808286016CFFFFC486010600000080C286016CFFFF|
// {"media":"Unknown","meter":"c5isf_t1a1","name":"Heat","id":"58455445","total_energy_consumption_kwh":0,"total_volume_m3":0,"status":"","total_energy_consumption_last_month_kwh":0,"last_month_date":"","total_energy_consumption_month-2_kwh":0,"month-2_date":"","total_energy_consumption_month-3_kwh":0,"month-3_date":"","total_energy_consumption_month-4_kwh":0,"month-4_date":"","total_energy_consumption_month-5_kwh":0,"month-5_date":"","total_energy_consumption_month-6_kwh":0,"month-6_date":"","total_energy_consumption_month-7_kwh":0,"month-7_date":"","total_energy_consumption_month-8_kwh":0,"month-8_date":"","total_energy_consumption_month-9_kwh":0,"month-9_date":"","total_energy_consumption_month-10_kwh":0,"month-10_date":"","total_energy_consumption_month-11_kwh":0,"month-11_date":"","total_energy_consumption_month-12_kwh":0,"month-12_date":"","total_energy_consumption_month-13_kwh":0,"month-13_date":"","total_energy_consumption_month-14_kwh":0,"month-14_date":"","timestamp":"1111-11-11T11:11:11Z"}
// |Heat;58455445;0.000000;;1111-11-11 11:11.11
