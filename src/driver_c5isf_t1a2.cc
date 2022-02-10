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

struct MeterC5isf_t1a2 : public virtual MeterCommonImplementation
{
MeterC5isf_t1a2(MeterInfo &mi, DriverInfo &di);

private:
    double total_volume_m3_ {};
    double total_volume_last_month_m3_ {};
    string last_month_date_;
    double total_volume_last_month_2_m3_ {};
    string last_month_2_date_;
    double total_volume_last_month_3_m3_ {};
    string last_month_3_date_;
    double total_volume_last_month_4_m3_ {};
    string last_month_4_date_;
    double total_volume_last_month_5_m3_ {};
    string last_month_5_date_;
    double total_volume_last_month_6_m3_ {};
    string last_month_6_date_;
    double total_volume_last_month_7_m3_ {};
    string last_month_7_date_;
    double total_volume_last_month_8_m3_ {};
    string last_month_8_date_;
    double total_volume_last_month_9_m3_ {};
    string last_month_9_date_;
    double total_volume_last_month_10_m3_ {};
    string last_month_10_date_;
    double total_volume_last_month_11_m3_ {};
    string last_month_11_date_;
    double total_volume_last_month_12_m3_ {};
    string last_month_12_date_;
    double total_volume_last_month_13_m3_ {};
    string last_month_13_date_;
    double total_volume_last_month_14_m3_ {};
    string last_month_14_date_;
};

static bool ok = registerDriver([](DriverInfo&di)
{
    di.setName("c5isf_t1a2");
    di.setMeterType(MeterType::WaterMeter);
    di.setExpectedTPLSecurityMode(TPLSecurityMode::AES_CBC_IV);
    di.addLinkMode(LinkMode::T1);
    di.addDetection(MANUFACTURER_ZRI, 0x07, 0x88);
    di.setConstructor([](MeterInfo& mi, DriverInfo& di){ return shared_ptr<Meter>(new MeterC5isf_t1a2(mi, di)); });
});

MeterC5isf_t1a2::MeterC5isf_t1a2(MeterInfo &mi, DriverInfo &di) : MeterCommonImplementation(mi, di)
{
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
        "total_volume_consumption_last_month",
        Quantity::Volume,
        NoDifVifKey,
        VifScaling::Auto,
        MeasurementType::Instantaneous,
        ValueInformation::Volume,
        StorageNr(32),
        TariffNr(0),
        IndexNr(1),
        PrintProperty::JSON,
        "The total m3 volume consumption recorded at end of last month.",
        SET_FUNC(total_volume_last_month_m3_, Unit::M3),
        GET_FUNC(total_volume_last_month_m3_, Unit::M3));

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
        "total_volume_consumption_month-2",
        Quantity::Volume,
        NoDifVifKey,
        VifScaling::Auto,
        MeasurementType::Instantaneous,
        ValueInformation::Volume,
        StorageNr(33),
        TariffNr(0),
        IndexNr(1),
        PrintProperty::JSON,
        "The total heat volume consumption recorded at end of last month.",
        SET_FUNC(total_volume_last_month_2_m3_, Unit::M3),
        GET_FUNC(total_volume_last_month_2_m3_, Unit::M3));

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
        "total_volume_consumption_month-3",
        Quantity::Volume,
        NoDifVifKey,
        VifScaling::Auto,
        MeasurementType::Instantaneous,
        ValueInformation::Volume,
        StorageNr(34),
        TariffNr(0),
        IndexNr(1),
        PrintProperty::JSON,
        "The total heat volume consumption recorded at end of last month.",
        SET_FUNC(total_volume_last_month_3_m3_, Unit::M3),
        GET_FUNC(total_volume_last_month_3_m3_, Unit::M3));

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
        "total_volume_consumption_month-4",
        Quantity::Volume,
        NoDifVifKey,
        VifScaling::Auto,
        MeasurementType::Instantaneous,
        ValueInformation::Volume,
        StorageNr(35),
        TariffNr(0),
        IndexNr(1),
        PrintProperty::JSON,
        "The total heat volume consumption recorded at end of last month.",
        SET_FUNC(total_volume_last_month_4_m3_, Unit::M3),
        GET_FUNC(total_volume_last_month_4_m3_, Unit::M3));

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
        "total_volume_consumption_month-5",
        Quantity::Volume,
        NoDifVifKey,
        VifScaling::Auto,
        MeasurementType::Instantaneous,
        ValueInformation::Volume,
        StorageNr(36),
        TariffNr(0),
        IndexNr(1),
        PrintProperty::JSON,
        "The total heat volume consumption recorded at end of last month.",
        SET_FUNC(total_volume_last_month_5_m3_, Unit::M3),
        GET_FUNC(total_volume_last_month_5_m3_, Unit::M3));

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
        "total_volume_consumption_month-6",
        Quantity::Volume,
        NoDifVifKey,
        VifScaling::Auto,
        MeasurementType::Instantaneous,
        ValueInformation::Volume,
        StorageNr(37),
        TariffNr(0),
        IndexNr(1),
        PrintProperty::JSON,
        "The total heat volume consumption recorded at end of last month.",
        SET_FUNC(total_volume_last_month_6_m3_, Unit::M3),
        GET_FUNC(total_volume_last_month_6_m3_, Unit::M3));

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
        "total_volume_consumption_month-7",
        Quantity::Volume,
        NoDifVifKey,
        VifScaling::Auto,
        MeasurementType::Instantaneous,
        ValueInformation::Volume,
        StorageNr(38),
        TariffNr(0),
        IndexNr(1),
        PrintProperty::JSON,
        "The total heat volume consumption recorded at end of last month.",
        SET_FUNC(total_volume_last_month_7_m3_, Unit::M3),
        GET_FUNC(total_volume_last_month_7_m3_, Unit::M3));

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
        "total_volume_consumption_month-8",
        Quantity::Volume,
        NoDifVifKey,
        VifScaling::Auto,
        MeasurementType::Instantaneous,
        ValueInformation::Volume,
        StorageNr(39),
        TariffNr(0),
        IndexNr(1),
        PrintProperty::JSON,
        "The total heat volume consumption recorded at end of last month.",
        SET_FUNC(total_volume_last_month_8_m3_, Unit::M3),
        GET_FUNC(total_volume_last_month_8_m3_, Unit::M3));

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
        "total_volume_consumption_month-9",
        Quantity::Volume,
        NoDifVifKey,
        VifScaling::Auto,
        MeasurementType::Instantaneous,
        ValueInformation::Volume,
        StorageNr(40),
        TariffNr(0),
        IndexNr(1),
        PrintProperty::JSON,
        "The total heat volume consumption recorded at end of last month.",
        SET_FUNC(total_volume_last_month_9_m3_, Unit::M3),
        GET_FUNC(total_volume_last_month_9_m3_, Unit::M3));

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
        "total_volume_consumption_month-10",
        Quantity::Volume,
        NoDifVifKey,
        VifScaling::Auto,
        MeasurementType::Instantaneous,
        ValueInformation::Volume,
        StorageNr(41),
        TariffNr(0),
        IndexNr(1),
        PrintProperty::JSON,
        "The total heat volume consumption recorded at end of last month.",
        SET_FUNC(total_volume_last_month_10_m3_, Unit::M3),
        GET_FUNC(total_volume_last_month_10_m3_, Unit::M3));

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
        "total_volume_consumption_month-11",
        Quantity::Volume,
        NoDifVifKey,
        VifScaling::Auto,
        MeasurementType::Instantaneous,
        ValueInformation::Volume,
        StorageNr(42),
        TariffNr(0),
        IndexNr(1),
        PrintProperty::JSON,
        "The total heat volume consumption recorded at end of last month.",
        SET_FUNC(total_volume_last_month_11_m3_, Unit::M3),
        GET_FUNC(total_volume_last_month_11_m3_, Unit::M3));

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
        "total_volume_consumption_month-12",
        Quantity::Volume,
        NoDifVifKey,
        VifScaling::Auto,
        MeasurementType::Instantaneous,
        ValueInformation::Volume,
        StorageNr(43),
        TariffNr(0),
        IndexNr(1),
        PrintProperty::JSON,
        "The total heat volume consumption recorded at end of last month.",
        SET_FUNC(total_volume_last_month_12_m3_, Unit::M3),
        GET_FUNC(total_volume_last_month_12_m3_, Unit::M3));

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
        "total_volume_consumption_month-13",
        Quantity::Volume,
        NoDifVifKey,
        VifScaling::Auto,
        MeasurementType::Instantaneous,
        ValueInformation::Volume,
        StorageNr(44),
        TariffNr(0),
        IndexNr(1),
        PrintProperty::JSON,
        "The total heat volume consumption recorded at end of last month.",
        SET_FUNC(total_volume_last_month_13_m3_, Unit::M3),
        GET_FUNC(total_volume_last_month_13_m3_, Unit::M3));

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
        "total_volume_consumption_month-14",
        Quantity::Volume,
        NoDifVifKey,
        VifScaling::Auto,
        MeasurementType::Instantaneous,
        ValueInformation::Volume,
        StorageNr(45),
        TariffNr(0),
        IndexNr(1),
        PrintProperty::JSON,
        "The total heat volume consumption recorded at end of last month.",
        SET_FUNC(total_volume_last_month_14_m3_, Unit::M3),
        GET_FUNC(total_volume_last_month_14_m3_, Unit::M3));

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

// Test: Heat c5isf_t1a2 ANYID NOKEY
// telegram=|DA44496A5555445588077A320200002F2F04140000000084800114000000008280016C2124C480011400000080C280016CFFFF84810114000000808281016CFFFFC481011400000080C281016CFFFF84820114000000808282016CFFFFC482011400000080C282016CFFFF84830114000000808283016CFFFFC483011400000080C283016CFFFF84840114000000808284016CFFFFC484011400000080C284016CFFFF84850114000000808285016CFFFFC485011400000080C285016CFFFF84860114000000808286016CFFFFC486011400000080C286016CFFFF|
// {"media":"water","meter":"c5isf_t1a2","name":"Heat","id":"55445555","total_volume_m3":0,"total_volume_consumption_last_month_m3":0,"last_month_date":"2017-04-01","total_volume_consumption_month-2_m3":21474836.48,"month-2_date":"2127-15-31","total_volume_consumption_month-3_m3":21474836.48,"month-3_date":"2127-15-31","total_volume_consumption_month-4_m3":21474836.48,"month-4_date":"2127-15-31","total_volume_consumption_month-5_m3":21474836.48,"month-5_date":"2127-15-31","total_volume_consumption_month-6_m3":21474836.48,"month-6_date":"2127-15-31","total_volume_consumption_month-7_m3":21474836.48,"month-7_date":"2127-15-31","total_volume_consumption_month-8_m3":21474836.48,"month-8_date":"2127-15-31","total_volume_consumption_month-9_m3":21474836.48,"month-9_date":"2127-15-31","total_volume_consumption_month-10_m3":21474836.48,"month-10_date":"2127-15-31","total_volume_consumption_month-11_m3":21474836.48,"month-11_date":"2127-15-31","total_volume_consumption_month-12_m3":21474836.48,"month-12_date":"2127-15-31","total_volume_consumption_month-13_m3":21474836.48,"month-13_date":"2127-15-31","total_volume_consumption_month-14_m3":21474836.48,"month-14_date":"2127-15-31","timestamp":"1111-11-11T11:11:11Z"}
// |Heat;55445555;1111-11-11 11:11.11
