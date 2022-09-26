/*
 Copyright (C) 2020 Fredrik Öhrström (gpl-3.0-or-later)

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

struct MeterWaterstarM : public virtual MeterCommonImplementation {
    MeterWaterstarM(MeterInfo &mi);

    // Total water counted through the meter
    double totalWaterConsumption(Unit u);
    bool  hasTotalWaterConsumption();
    double totalWaterBackwards(Unit u);

private:
    void processContent(Telegram *t);

    string meter_timestamp_;
    double total_water_consumption_m3_ {};
    uint16_t info_codes_ {};
    double total_water_backwards_m3_ {};
    string meter_version_ {};
    string parameter_set_ {};

    string status_;
    Translate::Lookup error_codes_;
};

shared_ptr<Meter> createWaterstarM(MeterInfo &mi)
{
    return shared_ptr<Meter>(new MeterWaterstarM(mi));
}

MeterWaterstarM::MeterWaterstarM(MeterInfo &mi) :
    MeterCommonImplementation(mi, "waterstarm")
{
    setMeterType(MeterType::WaterMeter);

    setExpectedTPLSecurityMode(TPLSecurityMode::AES_CBC_IV);

    addLinkMode(LinkMode::T1);
    addLinkMode(LinkMode::C1);

    error_codes_ = Translate::Lookup(
        {
            {
                {
                    "ERROR_FLAGS",
                    Translate::Type::BitToString,
                    0xffff,
                    "OK",
                    {
                        { 0x01, "SW_ERROR" },
                        { 0x02, "CRC_ERROR" },
                        { 0x04, "SENSOR_ERROR" },
                        { 0x08, "MEASUREMENT_ERROR" },
                        { 0x10, "BATTERY_VOLTAGE_ERROR" },
                        { 0x20, "MANIPULATION" },
                        { 0x40, "LEAKAGE_OR_NO_USAGE" },
                        { 0x80, "REVERSE_FLOW" },
                        { 0x100, "OVERLOAD" },
                    }
                }
            }
        });

    addPrint("meter_timestamp", Quantity::Text,
             [&](){ return meter_timestamp_; },
             "Date time for this reading.",
             PrintProperty::JSON);

    addPrint("total", Quantity::Volume,
             [&](Unit u){ return totalWaterConsumption(u); },
             "The total water consumption recorded by this meter.",
             PrintProperty::FIELD | PrintProperty::JSON);

    addPrint("total_backwards", Quantity::Volume,
             [&](Unit u){ return totalWaterBackwards(u); },
             "The total amount of water running backwards through meter.",
             PrintProperty::FIELD | PrintProperty::JSON);

    addPrint("current_status", Quantity::Text,
             [&](){ return status_; },
             "The status is OK or some error condition.",
             PrintProperty::FIELD | PrintProperty::JSON);

    addPrint("meter_version", Quantity::Text,
             [&](){ return meter_version_; },
             "Meter version.",
             PrintProperty::JSON);

    addPrint("parameter_set", Quantity::Text,
             [&](){ return parameter_set_; },
             "Parameter set.",
             PrintProperty::JSON);
}

void MeterWaterstarM::processContent(Telegram *t)
{
    /*
      (waterstarm) 11: 04 dif (32 Bit Integer/Binary Instantaneous value)
      (waterstarm) 12: 6D vif (Date and time type)
      (waterstarm) 13: 282A9E27
      (waterstarm) 17: 04 dif (32 Bit Integer/Binary Instantaneous value)
      (waterstarm) 18: 13 vif (Volume l)
      (waterstarm) 19: * 6A000000 total consumption (0.106000 m3)
      (waterstarm) 1d: 02 dif (16 Bit Integer/Binary Instantaneous value)
      (waterstarm) 1e: FD vif (Second extension of VIF-codes)
      (waterstarm) 1f: 17 vife (Error flags (binary))
      (waterstarm) 20: 0000
      (waterstarm) 22: 04 dif (32 Bit Integer/Binary Instantaneous value)
      (waterstarm) 23: 93 vif (Volume l)
      (waterstarm) 24: 3C vife (backward flow)
      (waterstarm) 25: 00000000
      (waterstarm) 29: 2F skip
      (waterstarm) 2a: 2F skip
      (waterstarm) 2b: 2F skip
      (waterstarm) 2c: 2F skip
      (waterstarm) 2d: 2F skip
      (waterstarm) 2e: 2F skip
      (waterstarm) 2f: 03 dif (24 Bit Integer/Binary Instantaneous value)
      (waterstarm) 30: FD vif (Second extension of VIF-codes)
      (waterstarm) 31: 0C vife (Model/Version)
      (waterstarm) 32: 080000
      (waterstarm) 35: 02 dif (16 Bit Integer/Binary Instantaneous value)
      (waterstarm) 36: FD vif (Second extension of VIF-codes)
      (waterstarm) 37: 0B vife (Parameter set identification)
      (waterstarm) 38: 0011
    */
    int offset;
    string key;

    if (findKey(MeasurementType::Instantaneous, VIFRange::DateTime, 0, 0, &key, &t->dv_entries)) {
        struct tm datetime;
        extractDVdate(&t->dv_entries, key, &offset, &datetime);
        meter_timestamp_ = strdatetime(&datetime);
        t->addMoreExplanation(offset, " at date (%s)", meter_timestamp_.c_str());
    }

    if(findKey(MeasurementType::Instantaneous, VIFRange::Volume, 0, 0, &key, &t->dv_entries)) {
        extractDVdouble(&t->dv_entries, key, &offset, &total_water_consumption_m3_);
        t->addMoreExplanation(offset, " total consumption (%f m3)", total_water_consumption_m3_);
    }

    extractDVuint16(&t->dv_entries, "02FD17", &offset, &info_codes_);
    status_ = error_codes_.translate(info_codes_);
    t->addMoreExplanation(offset, " error flags (%s)", status_.c_str());

    extractDVdouble(&t->dv_entries, "04933C", &offset, &total_water_backwards_m3_);
    t->addMoreExplanation(offset, " total water backwards (%f m3)", total_water_backwards_m3_);

    uint32_t tmp32;
    extractDVuint24(&t->dv_entries, "03FD0C", &offset, &tmp32);
    strprintf(meter_version_, "%06x", tmp32);
    t->addMoreExplanation(offset, " meter version (%s)", meter_version_.c_str());

    uint16_t tmp16;
    extractDVuint16(&t->dv_entries, "02FD0B", &offset, &tmp16);
    strprintf(parameter_set_, "%04x", tmp16);
    t->addMoreExplanation(offset, " parameter set (%s)", parameter_set_.c_str());
}

double MeterWaterstarM::totalWaterConsumption(Unit u)
{
    assertQuantity(u, Quantity::Volume);
    return convert(total_water_consumption_m3_, Unit::M3, u);
}

double MeterWaterstarM::totalWaterBackwards(Unit u)
{
    assertQuantity(u, Quantity::Volume);
    return convert(total_water_backwards_m3_, Unit::M3, u);
}

bool MeterWaterstarM::hasTotalWaterConsumption()
{
    return true;
}
