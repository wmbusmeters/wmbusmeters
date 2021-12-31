/*
 Copyright (C) 2021 Fredrik Öhrström

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

#include"dvparser.h"
#include"meters.h"
#include"meters_common_implementation.h"
#include"wmbus.h"
#include"wmbus_utils.h"
#include"util.h"

using namespace std;

struct MeterLSE_07_17 : public virtual MeterCommonImplementation {
    MeterLSE_07_17(MeterInfo &mi);

    // Total water counted through the meter
    double totalWaterConsumption(Unit u);
    bool hasTotalWaterConsumption();

    // Water conumption at due date
    double dueDateWaterConsumption(Unit u);
    // The configured due date
    string dueDate();

    // Not shure about this one, should be error codes and 0 otherwise
    string errorCode();
    // Date of the error, if no error occured it is 2127-15-31 (FFFF)
    string errorDate();

    // Date and time of the device, presumably in UTC
    string deviceDateTime();

private:
    void processContent(Telegram *t);

    double total_water_consumption_m3_ {};

    double due_date_water_consumption_m3_ {};
    string due_date_ {};

    ushort error_code_ {};
    string error_date_ {};

    string device_date_time_ {};

};

shared_ptr<Meter> createLSE_07_17(MeterInfo &mi)
{
    return shared_ptr<Meter>(new MeterLSE_07_17(mi));
}

MeterLSE_07_17::MeterLSE_07_17(MeterInfo &mi) :
    MeterCommonImplementation(mi, MeterDriver::LSE_07_17)
{
    setMeterType(MeterType::WaterMeter);

    setExpectedTPLSecurityMode(TPLSecurityMode::AES_CBC_IV);

    addLinkMode(LinkMode::S1);

    addPrint("total", Quantity::Volume,
             [&](Unit u){ return totalWaterConsumption(u); },
             "The total water consumption recorded by this meter.",
             true, true);
    addPrint("due_date", Quantity::Volume,
             [&](Unit u){ return dueDateWaterConsumption(u); },
             "The water consumption at the due date.",
             true, true);
    addPrint("due_date", Quantity::Text,
             [&](){ return dueDate(); },
             "The due date configured on the meter.",
             true, true);
    addPrint("error_code", Quantity::Text,
             [&](){ return errorCode(); },
             "Error code of the Meter, 0 means no error.",
             true, true);
    addPrint("error_date", Quantity::Text,
             [&](){ return errorDate(); },
             "The date the error occured at. If no error, reads 2127-15-31 (FFFF).",
             true, true);
    addPrint("device_date_time", Quantity::Text,
             [&](){ return deviceDateTime(); },
             "Error code of the Meter, 0 means no error.",
             true, true);

}

void MeterLSE_07_17::processContent(Telegram *t)
{
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
    */

    int offset;
    string key;

    if (findKey(MeasurementType::Instantaneous, ValueInformation::Volume, 0, 0, &key, &t->values)) {
        extractDVdouble(&t->values, key, &offset, &total_water_consumption_m3_);
        t->addMoreExplanation(offset, " total consumption (%f m3)", total_water_consumption_m3_);
    }
    if (findKey(MeasurementType::Instantaneous, ValueInformation::Volume, 1, 0, &key, &t->values)) {
        extractDVdouble(&t->values, key, &offset, &due_date_water_consumption_m3_);
        t->addMoreExplanation(offset, " due date consumption (%f m3)", due_date_water_consumption_m3_);
    }
    if (findKey(MeasurementType::Instantaneous, ValueInformation::Date, 1, 0, &key, &t->values)) {
        struct tm date;
        extractDVdate(&t->values, key, &offset, &date);
        due_date_ = strdate(&date);
        t->addMoreExplanation(offset, " due date (%s)", due_date_.c_str());
    }

    key = "02BB56";
    if (hasKey(&t->values, key)) {
        extractDVuint16(&t->values, key, &offset, &error_code_);
        // Not sure about this one, is it error codes or sth else?
        t->addMoreExplanation(offset, " error code (%s)", errorCode().c_str());
    }

    if (findKey(MeasurementType::AtError, ValueInformation::Date, 0, 0, &key, &t->values)) {
        struct tm date;
        extractDVdate(&t->values, key, &offset, &date);
        error_date_ = strdate(&date);
        t->addMoreExplanation(offset, " error date (%s)", error_date_.c_str());
    }

    if (findKey(MeasurementType::Instantaneous, ValueInformation::DateTime, 0, 0, &key, &t->values)) {
        struct tm datetime;
        extractDVdate(&t->values, key, &offset, &datetime);
        device_date_time_ = strdatetime(&datetime);
        t->addMoreExplanation(offset, " device datetime (%s)", device_date_time_.c_str());
    }

}

double MeterLSE_07_17::totalWaterConsumption(Unit u)
{
    assertQuantity(u, Quantity::Volume);
    return convert(total_water_consumption_m3_, Unit::M3, u);
}

bool MeterLSE_07_17::hasTotalWaterConsumption()
{
    return true;
}

double MeterLSE_07_17::dueDateWaterConsumption(Unit u)
{
    assertQuantity(u, Quantity::Volume);
    return convert(due_date_water_consumption_m3_, Unit::M3, u);
}

string MeterLSE_07_17::dueDate()
{
    return due_date_;
}

string MeterLSE_07_17::errorCode()
{
    if (error_code_ == 0) return "OK";

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
    //

    return tostrprintf("ERR %04x", error_code_);
}

string MeterLSE_07_17::errorDate()
{
    return error_date_;
}

string MeterLSE_07_17::deviceDateTime()
{
    return device_date_time_;
}
