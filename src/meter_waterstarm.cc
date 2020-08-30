/*
 Copyright (C) 2020 Fredrik Öhrström

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

struct MeterWaterstarM : public virtual WaterMeter, public virtual MeterCommonImplementation {
    MeterWaterstarM(MeterInfo &mi);

    // Total water counted through the meter
    double totalWaterConsumption(Unit u);
    bool  hasTotalWaterConsumption();
    double totalWaterBackwards(Unit u);

private:
    void processContent(Telegram *t);
    string status();

    string meter_timestamp_;
    double total_water_consumption_m3_ {};
    uint16_t info_codes_ {};
    double total_water_backwards_m3_ {};
    string meter_version_ {};
    string parameter_set_ {};
};

unique_ptr<WaterMeter> createWaterstarM(MeterInfo &mi)
{
    return unique_ptr<WaterMeter>(new MeterWaterstarM(mi));
}

MeterWaterstarM::MeterWaterstarM(MeterInfo &mi) :
    MeterCommonImplementation(mi, MeterType::WATERSTARM, MANUFACTURER_DWZ)
{
    setExpectedTPLSecurityMode(TPLSecurityMode::AES_CBC_IV);

    addMedia(0x06);

    addLinkMode(LinkMode::T1);
    addLinkMode(LinkMode::C1);

    addExpectedVersion(0x02);

    addPrint("meter_timestamp", Quantity::Text,
             [&](){ return meter_timestamp_; },
             "Date time for this reading.",
             false, true);

    addPrint("total", Quantity::Volume,
             [&](Unit u){ return totalWaterConsumption(u); },
             "The total water consumption recorded by this meter.",
             true, true);

    addPrint("total_backwards", Quantity::Volume,
             [&](Unit u){ return totalWaterBackwards(u); },
             "The total amount of water running backwards through meter.",
             true, true);

    addPrint("current_status", Quantity::Text,
             [&](){ return status(); },
             "Status of meter.",
             true, true);

    addPrint("meter_version", Quantity::Text,
             [&](){ return meter_version_; },
             "Meter version.",
             false, true);

    addPrint("parameter_set", Quantity::Text,
             [&](){ return parameter_set_; },
             "Parameter set.",
             false, true);
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

    if (findKey(MeasurementType::Unknown, ValueInformation::DateTime, 0, 0, &key, &t->values)) {
        struct tm datetime;
        extractDVdate(&t->values, key, &offset, &datetime);
        meter_timestamp_ = strdatetime(&datetime);
        t->addMoreExplanation(offset, " at date (%s)", meter_timestamp_.c_str());
    }

    if(findKey(MeasurementType::Unknown, ValueInformation::Volume, 0, 0, &key, &t->values)) {
        extractDVdouble(&t->values, key, &offset, &total_water_consumption_m3_);
        t->addMoreExplanation(offset, " total consumption (%f m3)", total_water_consumption_m3_);
    }

    extractDVuint16(&t->values, "02FD17", &offset, &info_codes_);
    t->addMoreExplanation(offset, " info codes (%s)", status().c_str());

    extractDVdouble(&t->values, "04933C", &offset, &total_water_backwards_m3_);
    t->addMoreExplanation(offset, " total water backwards (%f m3)", total_water_backwards_m3_);

    uint32_t tmp32;
    extractDVuint24(&t->values, "03FD0C", &offset, &tmp32);
    strprintf(meter_version_, "%06x", tmp32);
    t->addMoreExplanation(offset, " meter version (%s)", meter_version_.c_str());

    uint16_t tmp16;
    extractDVuint16(&t->values, "02FD0B", &offset, &tmp16);
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

string MeterWaterstarM::status()
{
    if (info_codes_ == 0) return "OK";
    string s;
    // We would like to know what the bits mean, but currently we do not!
    strprintf(s, "ERROR(%04x)", info_codes_);
    return s;
}
