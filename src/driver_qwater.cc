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

using namespace std;

struct MeterQWater : public virtual MeterCommonImplementation
{
    MeterQWater(MeterInfo &mi, DriverInfo &di);

    double total_water_consumption_m3_ {};

    double due_date_water_consumption_m3_ {};
    string due_date_ {};

    double due_date_17_water_consumption_m3_ {};
    string due_date_17_ {};

    string error_code_ {};
    string error_date_ {};

    string device_date_time_ {};
};

static bool ok = registerDriver([](DriverInfo&di)
{
    di.setName("qwater");
    di.setMeterType(MeterType::WaterMeter);
    di.setExpectedTPLSecurityMode(TPLSecurityMode::AES_CBC_IV);
    di.addLinkMode(LinkMode::S1);
    di.addDetection(MANUFACTURER_QDS, 0x37,  0x33);
    di.addDetection(MANUFACTURER_QDS, 0x06,  0x18);

    di.setConstructor([](MeterInfo& mi, DriverInfo& di){ return shared_ptr<Meter>(new MeterQWater(mi, di)); });
});

MeterQWater::MeterQWater(MeterInfo &mi, DriverInfo &di) :
    MeterCommonImplementation(mi, di)
{
    addNumericFieldWithExtractor(
        "total",
        Quantity::Volume,
        NoDifVifKey,
        VifScaling::Auto,
        MeasurementType::Instantaneous,
        VIFRange::Volume,
        StorageNr(0),
        TariffNr(0),
        IndexNr(1),
        PrintProperty::JSON | PrintProperty::FIELD | PrintProperty::IMPORTANT,
        "The total water consumption recorded by this meter.",
        SET_FUNC(total_water_consumption_m3_, Unit::M3),
        GET_FUNC(total_water_consumption_m3_, Unit::M3));

    addNumericFieldWithExtractor(
        "due_date",
        Quantity::Volume,
        NoDifVifKey,
        VifScaling::Auto,
        MeasurementType::Instantaneous,
        VIFRange::Volume,
        StorageNr(1),
        TariffNr(0),
        IndexNr(1),
        PrintProperty::JSON | PrintProperty::FIELD | PrintProperty::IMPORTANT,
        "The water consumption at the due date.",
        SET_FUNC(due_date_water_consumption_m3_, Unit::M3),
        GET_FUNC(due_date_water_consumption_m3_, Unit::M3));

    addStringFieldWithExtractor(
        "due_date",
        Quantity::Text,
        NoDifVifKey,
        MeasurementType::Instantaneous,
        VIFRange::Date,
        StorageNr(1),
        TariffNr(0),
        IndexNr(1),
        PrintProperty::JSON | PrintProperty::FIELD,
        "The due date configured on the meter.",
        SET_STRING_FUNC(due_date_),
        GET_STRING_FUNC(due_date_));

    addNumericFieldWithExtractor(
        "due_date_17",
        Quantity::Volume,
        NoDifVifKey,
        VifScaling::Auto,
        MeasurementType::Instantaneous,
        VIFRange::Volume,
        StorageNr(17),
        TariffNr(0),
        IndexNr(1),
        PrintProperty::JSON,
        "The water consumption at the 17th historical due date.",
        SET_FUNC(due_date_17_water_consumption_m3_, Unit::M3),
        GET_FUNC(due_date_17_water_consumption_m3_, Unit::M3));

    addStringFieldWithExtractor(
        "due_date_17",
        Quantity::Text,
        NoDifVifKey,
        MeasurementType::Instantaneous,
        VIFRange::Date,
        StorageNr(17),
        TariffNr(0),
        IndexNr(1),
        PrintProperty::JSON,
        "The due date configured for the 17th historical value.",
        SET_STRING_FUNC(due_date_17_),
        GET_STRING_FUNC(due_date_17_));

    addStringFieldWithExtractorAndLookup(
        "error_code",
        Quantity::Text,
        DifVifKey("02BB56"),
        MeasurementType::Instantaneous,
        VIFRange::Any,
        AnyStorageNr,
        AnyTariffNr,
        IndexNr(1),
        PrintProperty::JSON | PrintProperty::FIELD,
        "Error code of the Meter, 0 means no error.",
        SET_STRING_FUNC(error_code_),
        GET_STRING_FUNC(error_code_),
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
        "error_date",
        Quantity::Text,
        NoDifVifKey,
        MeasurementType::AtError,
        VIFRange::Date,
        StorageNr(0),
        TariffNr(0),
        IndexNr(1),
        PrintProperty::JSON,
        "The date the error occured at. If no error, reads 2127-15-31 (FFFF).",
        SET_STRING_FUNC(error_date_),
        GET_STRING_FUNC(error_date_));

    addStringFieldWithExtractor(
        "device_date_time",
        Quantity::Text,
        NoDifVifKey,
        MeasurementType::Instantaneous,
        VIFRange::DateTime,
        StorageNr(0),
        TariffNr(0),
        IndexNr(1),
        PrintProperty::JSON,
        "Date when measurement was recorded.",
        SET_STRING_FUNC(device_date_time_),
        GET_STRING_FUNC(device_date_time_));
}

// Test: MyQWater qwater 12353648 NOKEY
// telegram=|374493444836351218067ac70000200c13911900004c1391170000426cbf2ccc081391170000c2086cbf2c02bb560000326cffff046d1e02de21fed0|
// {"media":"warm water","meter":"qwater","name":"MyQWater","id":"12353648","total_m3":1.991,"due_date_m3":1.791,"due_date":"2021-12-31","due_date_17_m3":1.791,"due_date_17":"2021-12-31","error_code":"OK","error_date":"2127-15-31","device_date_time":"2022-01-30 02:30","timestamp":"1111-11-11T11:11:11Z"}
// |MyQWater;12353648;1.991000;1.791000;2021-12-31;OK;1111-11-11 11:11.11

// And a second telegram that only updates the device date time.
// telegram=|47449344483635121806780dff5f350082da0000600107c113ffff48200000bf2c91170000df2120200000008001000000060019001000160018000d001300350017002f046d370cc422c759|
// {"media":"warm water","meter":"qwater","name":"MyQWater","id":"12353648","total_m3":1.991,"due_date_m3":1.791,"due_date":"2021-12-31","due_date_17_m3":1.791,"due_date_17":"2021-12-31","error_code":"OK","error_date":"2127-15-31","device_date_time":"2022-02-04 12:55","timestamp":"1111-11-11T11:11:11Z"}
// |MyQWater;12353648;1.991000;1.791000;2021-12-31;OK;1111-11-11 11:11.11
