/*
 Copyright (C) 2021-2022 Fredrik Öhrström

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

struct MeterLSE_07_17 : public virtual MeterCommonImplementation
{
    MeterLSE_07_17(MeterInfo &mi, DriverInfo &di);

    double total_water_consumption_m3_ {};

    double due_date_water_consumption_m3_ {};
    string due_date_ {};

    string error_code_ {};
    string error_date_ {};

    string device_date_time_ {};
};

static bool ok = registerDriver([](DriverInfo&di)
{
    di.setName("lse_07_17");
    di.setMeterType(MeterType::WaterMeter);
    di.setExpectedTPLSecurityMode(TPLSecurityMode::AES_CBC_IV);
    di.addLinkMode(LinkMode::S1);
    di.addDetection(MANUFACTURER_LSE, 0x06,  0x18);
    di.addDetection(MANUFACTURER_LSE, 0x07,  0x18);
    di.addDetection(MANUFACTURER_LSE, 0x07,  0x16);
    di.addDetection(MANUFACTURER_LSE, 0x07,  0x17);
    di.addDetection(MANUFACTURER_QDS, 0x37,  0x33);

    di.setConstructor([](MeterInfo& mi, DriverInfo& di){ return shared_ptr<Meter>(new MeterLSE_07_17(mi, di)); });
});

MeterLSE_07_17::MeterLSE_07_17(MeterInfo &mi, DriverInfo &di) :
    MeterCommonImplementation(mi, di)
{
    addFieldWithExtractor(
        "total",
        Quantity::Volume,
        NoDifVifKey,
        VifScaling::Auto,
        MeasurementType::Instantaneous,
        ValueInformation::Volume,
        StorageNr(0),
        TariffNr(0),
        IndexNr(1),
        PrintProperty::JSON | PrintProperty::FIELD | PrintProperty::IMPORTANT,
        "The total water consumption recorded by this meter.",
        SET_FUNC(total_water_consumption_m3_, Unit::M3),
        GET_FUNC(total_water_consumption_m3_, Unit::M3));

    addFieldWithExtractor(
        "due_date",
        Quantity::Volume,
        NoDifVifKey,
        VifScaling::Auto,
        MeasurementType::Instantaneous,
        ValueInformation::Volume,
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
        ValueInformation::Date,
        StorageNr(1),
        TariffNr(0),
        IndexNr(1),
        PrintProperty::JSON | PrintProperty::FIELD,
        "The due date configured on the meter.",
        SET_STRING_FUNC(due_date_),
        GET_STRING_FUNC(due_date_));

    addStringFieldWithExtractorAndLookup(
        "error_code",
        Quantity::Text,
        DifVifKey("02BB56"),
        MeasurementType::Unknown,
        ValueInformation::Any,
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
        ValueInformation::Date,
        StorageNr(0),
        TariffNr(0),
        IndexNr(1),
        PrintProperty::JSON | PrintProperty::FIELD,
        "The date the error occured at. If no error, reads 2127-15-31 (FFFF).",
        SET_STRING_FUNC(error_date_),
        GET_STRING_FUNC(error_date_));

    addStringFieldWithExtractor(
        "device_date_time",
        Quantity::Text,
        NoDifVifKey,
        MeasurementType::Instantaneous,
        ValueInformation::DateTime,
        StorageNr(0),
        TariffNr(0),
        IndexNr(1),
        PrintProperty::JSON | PrintProperty::FIELD,
        "Date when measurement was recorded.",
        SET_STRING_FUNC(device_date_time_),
        GET_STRING_FUNC(device_date_time_));
}

    /*
    The following telegram corresponds to the Qundis QWater5.5 cold water meters I have here.
    From the device display it states that it is set to S-mode operation, sending a telegram
    every 4 h.
    Another option of this device is the C mode operation, sending telegrams every
    7.5 s.

    Even though my meters are definitely Qundis QWater5.5, the meters do not identify with
    manufacturer code QDS but with LSE.

    (lse_07_17) 0f: 0C dif (8 digit BCD Instantaneous value)
    (lse_07_17) 10: 13 vif (Volume l)
    (lse_07_17) 11: * 04400100 total consumption (14.004000 m3)
    (lse_07_17) 15: 4C dif (8 digit BCD Instantaneous value storagenr=1)
    (lse_07_17) 16: 13 vif (Volume l)
    (lse_07_17) 17: * 40620000 due date consumption (6.240000 m3)
    (lse_07_17) 1b: 42 dif (16 Bit Integer/Binary Instantaneous value storagenr=1)
    (lse_07_17) 1c: 6C vif (Date type G)
    (lse_07_17) 1d: * 9F2C due date (2020-12-31)
    (lse_07_17) 1f: 02 dif (16 Bit Integer/Binary Instantaneous value)
    (lse_07_17) 20: BB vif (Volume flow l/h)
    (lse_07_17) 21: 56 vife (duration of limit exceed last lower  is 2)
    (lse_07_17) 22: * 0000 error code (0)
    (lse_07_17) 24: 32 dif (16 Bit Integer/Binary Value during error state)
    (lse_07_17) 25: 6C vif (Date type G)
    (lse_07_17) 26: * FFFF error date (2127-15-31)
    (lse_07_17) 28: 04 dif (32 Bit Integer/Binary Instantaneous value)
    (lse_07_17) 29: 6D vif (Date and time type)
    (lse_07_17) 2a: * 180DA924 device datetime (2021-04-09 13:24)

    if (findKey(MeasurementType::Instantaneous, ValueInformation::DateTime, 0, 0, &key, &t->values)) {
        struct tm datetime;
        extractDVdate(&t->values, key, &offset, &datetime);
        device_date_time_ = strdatetime(&datetime);
        t->addMoreExplanation(offset, " device datetime (%s)", device_date_time_.c_str());
    }



    // How do the following error codes on the display map to the code in the telegram?
    // According to the datasheet, these errors can appear on the display:
    // LEAC Leak in the system (no associated error code)
    // 0    Negative direction of flow.
    // 2    Operating hours expired.
    // 3    Hardware error.
    // 4    Permanently stored error.
    // b    Communication via OPTO too often per month.
    // c    Communication via M-Bus too often per month.
    // d    Flow too high.
    // f    Device was without voltage supply briefly. All parameter settings are lost.
    */
