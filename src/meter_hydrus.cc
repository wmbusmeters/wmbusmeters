/*
 Copyright (C) 2019 Fredrik Öhrström

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

using namespace std;

struct MeterHydrus : public virtual WaterMeter, public virtual MeterCommonImplementation {
    MeterHydrus(WMBus *bus, MeterInfo &mi);

    // Total water counted through the meter
    double totalWaterConsumption(Unit u);
    double totalWaterConsumptionAtDate(Unit u);
    bool  hasTotalWaterConsumption();
    double maxFlow(Unit u);
    bool  hasMaxFlow();
    double flowTemperature(Unit u);

private:
    void processContent(Telegram *t);

    double total_water_consumption_m3_ {};
    double total_water_consumption_at_date_m3_ {};
    string at_date_;
    double max_flow_m3h_ {};
    double flow_temperature_c_ { 127 };
};

MeterHydrus::MeterHydrus(WMBus *bus, MeterInfo &mi) :
    MeterCommonImplementation(bus, mi, MeterType::HYDRUS, MANUFACTURER_DME)
{
    setExpectedTPLSecurityMode(TPLSecurityMode::AES_CBC_IV);

    addMedia(0x07);

    addLinkMode(LinkMode::T1);

    addExpectedVersion(0x70);

    addPrint("total", Quantity::Volume,
             [&](Unit u){ return totalWaterConsumption(u); },
             "The total water consumption recorded by this meter.",
             true, true);

    addPrint("max_flow", Quantity::Flow,
             [&](Unit u){ return maxFlow(u); },
             "The maxium flow recorded during previous period.",
             true, true);

    addPrint("flow_temperature", Quantity::Temperature,
             [&](Unit u){ return flowTemperature(u); },
             "The water temperature.",
             false, true);

    addPrint("total_at_date", Quantity::Volume,
             [&](Unit u){ return totalWaterConsumptionAtDate(u); },
             "The total water consumption recorded at date.",
             false, true);

    addPrint("at_date", Quantity::Text,
             [&](){ return at_date_; },
             "Date when total water consumption was recorded.",
             false, true);
}

unique_ptr<WaterMeter> createHydrus(WMBus *bus, MeterInfo &mi)
{
    return unique_ptr<WaterMeter>(new MeterHydrus(bus, mi));
}

void MeterHydrus::processContent(Telegram *t)
{
    /*
    (hydrus) 0f: 2F skip
    (hydrus) 10: 2F skip
    (hydrus) 11: 01 dif (8 Bit Integer/Binary Instantaneous value)
    (hydrus) 12: FD vif (Second extension of VIF-codes)
    (hydrus) 13: 08 vife (Access Number (transmission count))
    (hydrus) 14: 30
    (hydrus) 15: 0C dif (8 digit BCD Instantaneous value)
    (hydrus) 16: 13 vif (Volume l)
    (hydrus) 17: * 74110000 total consumption (1.174000 m3)
    (hydrus) 1b: 7C dif (8 digit BCD Value during error state storagenr=1)
    (hydrus) 1c: 13 vif (Volume l)
    (hydrus) 1d: 00000000
    (hydrus) 21: FC dif (8 digit BCD Value during error state storagenr=1)
    (hydrus) 22: 10 dife (subunit=0 tariff=1 storagenr=1)
    (hydrus) 23: 13 vif (Volume l)
    (hydrus) 24: 00000000
    (hydrus) 28: FC dif (8 digit BCD Value during error state storagenr=1)
    (hydrus) 29: 20 dife (subunit=0 tariff=2 storagenr=1)
    (hydrus) 2a: 13 vif (Volume l)
    (hydrus) 2b: 00000000
    (hydrus) 2f: 72 dif (16 Bit Integer/Binary Value during error state storagenr=1)
    (hydrus) 30: 6C vif (Date type G)
    (hydrus) 31: 0000
    (hydrus) 33: 0B dif (6 digit BCD Instantaneous value)
    (hydrus) 34: 3B vif (Volume flow l/h)
    (hydrus) 35: * 000000 max flow (0.000000 m3/h)
    (hydrus) 38: 02 dif (16 Bit Integer/Binary Instantaneous value)
    (hydrus) 39: FD vif (Second extension of VIF-codes)
    (hydrus) 3a: 74 vife (Reserved)
    (hydrus) 3b: 8713
    (hydrus) 3d: 02 dif (16 Bit Integer/Binary Instantaneous value)
    (hydrus) 3e: 5A vif (Flow temperature 10⁻¹ °C)
    (hydrus) 3f: 6800
    (hydrus) 41: C4 dif (32 Bit Integer/Binary Instantaneous value storagenr=1)
    (hydrus) 42: 01 dife (subunit=0 tariff=0 storagenr=3)
    (hydrus) 43: 6D vif (Date and time type)
    (hydrus) 44: 3B177F2A
    (hydrus) 48: CC dif (8 digit BCD Instantaneous value storagenr=1)
    (hydrus) 49: 01 dife (subunit=0 tariff=0 storagenr=3)
    (hydrus) 4a: 13 vif (Volume l)
    (hydrus) 4b: 00020000
    */
    int offset;
    string key;

    if(findKey(MeasurementType::Unknown, ValueInformation::Volume, 0, 0, &key, &t->values)) {
        extractDVdouble(&t->values, key, &offset, &total_water_consumption_m3_);
        t->addMoreExplanation(offset, " total consumption (%f m3)", total_water_consumption_m3_);
    }

    if(findKey(MeasurementType::Unknown, ValueInformation::VolumeFlow, 0, 0, &key, &t->values)) {
        extractDVdouble(&t->values, key, &offset, &max_flow_m3h_);
        t->addMoreExplanation(offset, " max flow (%f m3/h)", max_flow_m3h_);
    }

    if(findKey(MeasurementType::Unknown, ValueInformation::FlowTemperature, 0, 0, &key, &t->values)) {
        extractDVdouble(&t->values, key, &offset, &flow_temperature_c_);
        t->addMoreExplanation(offset, " flow temperature (%f °C)", flow_temperature_c_);
    }

    if(findKey(MeasurementType::Unknown, ValueInformation::Volume, 3, 0, &key, &t->values)) {
        extractDVdouble(&t->values, key, &offset, &total_water_consumption_at_date_m3_);
        t->addMoreExplanation(offset, " total consumption at date (%f m3)", total_water_consumption_at_date_m3_);
    }

    if (findKey(MeasurementType::Unknown, ValueInformation::DateTime, 3, 0, &key, &t->values)) {
        struct tm datetime;
        extractDVdate(&t->values, key, &offset, &datetime);
        at_date_ = strdatetime(&datetime);
        t->addMoreExplanation(offset, " at date (%s)", at_date_.c_str());
    }

}

double MeterHydrus::totalWaterConsumption(Unit u)
{
    assertQuantity(u, Quantity::Volume);
    return convert(total_water_consumption_m3_, Unit::M3, u);
}

double MeterHydrus::totalWaterConsumptionAtDate(Unit u)
{
    assertQuantity(u, Quantity::Volume);
    return convert(total_water_consumption_at_date_m3_, Unit::M3, u);
}

bool MeterHydrus::hasTotalWaterConsumption()
{
    return true;
}

double MeterHydrus::maxFlow(Unit u)
{
    assertQuantity(u, Quantity::Flow);
    return convert(max_flow_m3h_, Unit::M3H, u);
}

bool MeterHydrus::hasMaxFlow()
{
    return true;
}

double MeterHydrus::flowTemperature(Unit u)
{
    assertQuantity(u, Quantity::Temperature);
    return convert(flow_temperature_c_, Unit::C, u);
}
