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

struct MeterQ400 : public virtual MeterCommonImplementation {
    MeterQ400(MeterInfo &mi);

    // Total water counted through the meter
    double totalWaterConsumption(Unit u);
    bool  hasTotalWaterConsumption();

    string setDate();
    double consumptionAtSetDate(Unit u);

private:
    void processContent(Telegram *t);

    string meter_datetime_;
    double total_water_consumption_m3_ {};
    string set_date_;
    double consumption_at_set_date_m3_ {};

    double flow_m3h_ {};
    // What is flow really? The sum of forward and backward flow? Or the same as forward flow?
    double forward_flow_m3h_ {};
    double backward_flow_m3h_ {};
    double flow_temperature_c_ {};

    // Historical flow, perhaps over the last month?
    double set_forward_flow_m3h_ {};
    double set_backward_flow_m3h_ {};
};

shared_ptr<Meter> createQ400(MeterInfo &mi)
{
    return shared_ptr<Meter>(new MeterQ400(mi));
}

MeterQ400::MeterQ400(MeterInfo &mi) :
    MeterCommonImplementation(mi, "q400")
{
    setMeterType(MeterType::WaterMeter);

    setExpectedTPLSecurityMode(TPLSecurityMode::AES_CBC_IV);

    addLinkMode(LinkMode::T1);

    addPrint("total", Quantity::Volume,
             [&](Unit u){ return totalWaterConsumption(u); },
             "The total water consumption recorded by this meter.",
             true, true);

    addPrint("set_date", Quantity::Text,
             [&](){ return setDate(); },
             "The most recent billing period date.",
             false, true);

    addPrint("consumption_at_set_date", Quantity::Volume,
             [&](Unit u){ return consumptionAtSetDate(u); },
             "The total water consumption at the most recent billing period date.",
             false, true);

    addPrint("meter_datetime", Quantity::Text,
             [&](){ return meter_datetime_; },
             "Meter timestamp for measurement.",
             false, true);

    addPrint("flow", Quantity::Flow,
             [&](Unit u){ assertQuantity(u, Quantity::Flow); return convert(flow_m3h_, Unit::M3H, u); },
             "Water flow?",
             false, true);

    addPrint("forward_flow", Quantity::Flow,
             [&](Unit u){ assertQuantity(u, Quantity::Flow); return convert(forward_flow_m3h_, Unit::M3H, u); },
             "Forward flow.",
             false, true);

    addPrint("backward_flow", Quantity::Flow,
             [&](Unit u){ assertQuantity(u, Quantity::Flow); return convert(backward_flow_m3h_, Unit::M3H, u); },
             "Backward flow.",
             false, true);

    addPrint("flow_temperature", Quantity::Temperature,
             [&](Unit u){ assertQuantity(u, Quantity::Temperature); return convert(flow_temperature_c_, Unit::C, u); },
             "The water temperature.",
             false, true);

    addPrint("set_forward_flow", Quantity::Flow,
             [&](Unit u){ assertQuantity(u, Quantity::Flow); return convert(set_forward_flow_m3h_, Unit::M3H, u); },
             "Historical forward flow.",
             false, true);

    addPrint("set_backward_flow", Quantity::Flow,
             [&](Unit u){ assertQuantity(u, Quantity::Flow); return convert(set_backward_flow_m3h_, Unit::M3H, u); },
             "Historical backward flow.",
             false, true);

}

void MeterQ400::processContent(Telegram *t)
{
    /*
      This is the first q400 meter telegram content:

      (q400) 0f: 2f2f decrypt check bytes
      (q400) 11: 04 dif (32 Bit Integer/Binary Instantaneous value)
      (q400) 12: 6D vif (Date and time type)
      (q400) 13: 040D742C
      (q400) 17: 04 dif (32 Bit Integer/Binary Instantaneous value)
      (q400) 18: 13 vif (Volume l)
      (q400) 19: * 00000000 total consumption (0.000000 m3)
      (q400) 1d: 44 dif (32 Bit Integer/Binary Instantaneous value storagenr=1)
      (q400) 1e: 6D vif (Date and time type)
      (q400) 1f: * 0000612C set date (2019-12-01)
      (q400) 23: 44 dif (32 Bit Integer/Binary Instantaneous value storagenr=1)
      (q400) 24: 13 vif (Volume l)
      (q400) 25: * 00000000 consumption at set date (0.000000 m3)
      (q400) 29: 2F skip
      (q400) 2a: 2F skip
      (q400) 2b: 2F skip
      (q400) 2c: 2F skip
      (q400) 2d: 2F skip
      (q400) 2e: 2F skip
    */

    /* And here is the Axioma W1 meter which reports identical
       version and type and manufacturer as the old q400 meter.
       But it contains a lot more data.....silly,
       they should have a different meter type.

    (q400) 0f: 2f2f decrypt check bytes
    (q400) 11: 04 dif (32 Bit Integer/Binary Instantaneous value)
    (q400) 12: 6D vif (Date and time type)
    (q400) 13: 0110A927
    (q400) 17: 04 dif (32 Bit Integer/Binary Instantaneous value)
    (q400) 18: 13 vif (Volume l)
    (q400) 19: * 00000000 total consumption (0.000000 m3)
    (q400) 1d: 04 dif (32 Bit Integer/Binary Instantaneous value)
    (q400) 1e: 93 vif (Volume l)
    (q400) 1f: 3B vife (forward flow)
    (q400) 20: 00000000
    (q400) 24: 04 dif (32 Bit Integer/Binary Instantaneous value)
    (q400) 25: 93 vif (Volume l)
    (q400) 26: 3C vife (backward flow)
    (q400) 27: 00000000
    (q400) 2b: 02 dif (16 Bit Integer/Binary Instantaneous value)
    (q400) 2c: 3B vif (Volume flow l/h)
    (q400) 2d: 0000
    (q400) 2f: 02 dif (16 Bit Integer/Binary Instantaneous value)
    (q400) 30: 59 vif (Flow temperature 10⁻² °C)
    (q400) 31: 2A0A
    (q400) 33: 44 dif (32 Bit Integer/Binary Instantaneous value storagenr=1)
    (q400) 34: 6D vif (Date and time type)
    (q400) 35: * 0000A127 set date (2021-07-01)
    (q400) 39: 44 dif (32 Bit Integer/Binary Instantaneous value storagenr=1)
    (q400) 3a: 13 vif (Volume l)
    (q400) 3b: * 00000000 consumption at set date (0.000000 m3)
    (q400) 3f: 44 dif (32 Bit Integer/Binary Instantaneous value storagenr=1)
    (q400) 40: 93 vif (Volume l)
    (q400) 41: 3B vife (forward flow)
    (q400) 42: 00000000
    (q400) 46: 44 dif (32 Bit Integer/Binary Instantaneous value storagenr=1)
    (q400) 47: 93 vif (Volume l)
    (q400) 48: 3C vife (backward flow)
    (q400) 49: 00000000
    (q400) 4d: 01 dif (8 Bit Integer/Binary Instantaneous value)
    (q400) 4e: FD vif (Second extension FD of VIF-codes)
    (q400) 4f: 74 vife (Reserved)
    (q400) 50: 62
    (q400) 51: 2F skip
    (q400) 52: 2F skip
    (q400) 53: 2F skip
    (q400) 54: 2F skip
    (q400) 55: 2F skip
    (q400) 56: 2F skip
    (q400) 57: 2F skip
    (q400) 58: 2F skip
    (q400) 59: 2F skip
    (q400) 5a: 2F skip
    (q400) 5b: 2F skip
    (q400) 5c: 2F skip
    (q400) 5d: 2F skip
    (q400) 5e: 2F skip
    */
    int offset;
    string key;

    // Find keys common to both q400 and axioma.

    if(findKey(MeasurementType::Unknown, ValueInformation::Volume, 0, 0, &key, &t->values)) {
        extractDVdouble(&t->values, key, &offset, &total_water_consumption_m3_);
        t->addMoreExplanation(offset, " total consumption (%f m3)", total_water_consumption_m3_);
    }

    if(findKey(MeasurementType::Unknown, ValueInformation::Volume, 1, 0, &key, &t->values)) {
        extractDVdouble(&t->values, key, &offset, &consumption_at_set_date_m3_);
        t->addMoreExplanation(offset, " consumption at set date (%f m3)", consumption_at_set_date_m3_);
    }

    if (findKey(MeasurementType::Unknown, ValueInformation::DateTime, 1, 0, &key, &t->values)) {
        struct tm date;
        extractDVdate(&t->values, key, &offset, &date);
        set_date_ = strdate(&date);
        t->addMoreExplanation(offset, " set date (%s)", set_date_.c_str());
    }

    // Now the axioma values:
    if (findKey(MeasurementType::Instantaneous, ValueInformation::DateTime, 0, 0, &key, &t->values))
    {
        struct tm date;
        extractDVdate(&t->values, key, &offset, &date);
        meter_datetime_ = strdate(&date);
        t->addMoreExplanation(offset, " meter datetime (%s)", meter_datetime_.c_str());
    }

    extractDVdouble(&t->values, "04933B", &offset, &forward_flow_m3h_);
    t->addMoreExplanation(offset, " forward flow (%f m3/h)", forward_flow_m3h_);

    extractDVdouble(&t->values, "04933C", &offset, &backward_flow_m3h_);
    t->addMoreExplanation(offset, " backward flow (%f m3/h)", backward_flow_m3h_);

    // Why does the meter send both forward flow and flow? Aren't they the same?
    if(findKey(MeasurementType::Instantaneous, ValueInformation::VolumeFlow, 0, 0, &key, &t->values)) {
        extractDVdouble(&t->values, key, &offset, &flow_m3h_);
        t->addMoreExplanation(offset, " flow (%f m3/h)", flow_m3h_);
    }

    if(findKey(MeasurementType::Instantaneous, ValueInformation::FlowTemperature, 0, 0, &key, &t->values)) {
        extractDVdouble(&t->values, key, &offset, &flow_temperature_c_);
        t->addMoreExplanation(offset, " flow temperature (%f °C)", flow_temperature_c_);
    }

    extractDVdouble(&t->values, "44933B", &offset, &set_forward_flow_m3h_);
    t->addMoreExplanation(offset, " set forward flow (%f m3/h)", set_forward_flow_m3h_);

    extractDVdouble(&t->values, "44933C", &offset, &set_backward_flow_m3h_);
    t->addMoreExplanation(offset, " set backward flow (%f m3/h)", set_backward_flow_m3h_);
}

double MeterQ400::totalWaterConsumption(Unit u)
{
    assertQuantity(u, Quantity::Volume);
    return convert(total_water_consumption_m3_, Unit::M3, u);
}

bool MeterQ400::hasTotalWaterConsumption()
{
    return true;
}

string MeterQ400::setDate()
{
    return set_date_;
}

double MeterQ400::consumptionAtSetDate(Unit u)
{
    assertQuantity(u, Quantity::Volume);
    return convert(consumption_at_set_date_m3_, Unit::M3, u);
}
