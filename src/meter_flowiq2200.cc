/*
 Copyright (C) 2017-2020 Fredrik Öhrström (gpl-3.0-or-later)

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

#include<assert.h>

using namespace std;

// Are these bits really correct for this meter?

#define INFO_CODE_DRY 0x01
#define INFO_CODE_DRY_SHIFT (4+0)

#define INFO_CODE_REVERSE 0x02
#define INFO_CODE_REVERSE_SHIFT (4+3)

#define INFO_CODE_LEAK 0x04
#define INFO_CODE_LEAK_SHIFT (4+6)

#define INFO_CODE_BURST 0x08
#define INFO_CODE_BURST_SHIFT (4+9)

struct MeterFlowIQ2200 : public virtual MeterCommonImplementation {
    MeterFlowIQ2200(MeterInfo &mi, string mt);

    // Total water counted through the meter
    double totalWaterConsumption(Unit u);
    bool  hasTotalWaterConsumption();

    // Meter sends target water consumption or max flow, depending on meter configuration
    // We can see which was sent inside the wmbus message!

    // Target water consumption: The total consumption at the start of the previous 30 day period.
    double targetWaterConsumption(Unit u);
    bool  hasTargetWaterConsumption();

    double currentFlow(Unit u);

    // Max flow during last month or last 24 hours depending on meter configuration.
    double maxFlow(Unit u);
    bool hasMaxFlow();
    double minFlow(Unit u);

    // Water temperature
    double flowTemperature(Unit u);
    bool  hasFlowTemperature();
    double minFlowTemperature(Unit u);
    double maxFlowTemperature(Unit u);
    // Surrounding temperature
    double externalTemperature(Unit u);
    bool  hasExternalTemperature();

    // statusHumanReadable: DRY,REVERSED,LEAK,BURST if that status is detected right now, followed by
    //                      (dry 15-21 days) which means that, even it DRY is not active right now,
    //                      DRY has been active for 15-21 days during the last 30 days.
    string statusHumanReadable();
    string status();
    string timeDry();
    string timeReversed();
    string timeLeaking();
    string timeBursting();

private:

    void processContent(Telegram *t);
    string decodeTime(int time);

    uint16_t info_codes_ {};
    double total_water_consumption_m3_ {};
    bool has_total_water_consumption_ {};
    double target_water_consumption_m3_ {};
    bool has_target_water_consumption_ {};

    double current_flow_m3h_ {};
    double max_flow_m3h_ {};
    double min_flow_m3h_ {};

    double min_flow_temperature_c_ { 127 };
    double max_flow_temperature_c_ { 127 };

    double external_temperature_c_ { 127 };
    bool has_external_temperature_ {};

    string target_datetime_;
};

MeterFlowIQ2200::MeterFlowIQ2200(MeterInfo &mi, string mt) :
    MeterCommonImplementation(mi, mt)
{
    setMeterType(MeterType::WaterMeter);

    setExpectedELLSecurityMode(ELLSecurityMode::AES_CTR);

    addLinkMode(LinkMode::C1);

    addPrint("total", Quantity::Volume,
             [&](Unit u){ return totalWaterConsumption(u); },
             "The total water consumption recorded by this meter.",
             PrintProperty::FIELD | PrintProperty::JSON);

    addPrint("target", Quantity::Volume,
             [&](Unit u){ return targetWaterConsumption(u); },
             "The total water consumption recorded at the beginning of this month.",
             PrintProperty::FIELD | PrintProperty::JSON);

    addPrint("target_datetime", Quantity::Text,
             [&](){ return target_datetime_; },
             "Timestamp for water consumption recorded at the beginning of this month.",
             PrintProperty::FIELD | PrintProperty::JSON);

    addPrint("current_flow", Quantity::Flow,
             [&](Unit u){ return currentFlow(u); },
             "The current flow of water.",
             PrintProperty::FIELD | PrintProperty::JSON);

    addPrint("max_flow", Quantity::Flow,
             [&](Unit u){ return maxFlow(u); },
             "The maxium flow recorded during previous period.",
             PrintProperty::FIELD | PrintProperty::JSON);

    addPrint("min_flow", Quantity::Flow,
             [&](Unit u){ return currentFlow(u); },
             "The minimum flow recorded during previous period.",
             PrintProperty::FIELD | PrintProperty::JSON);

    addPrint("min_flow_temperature", Quantity::Temperature,
             [&](Unit u){ return minFlowTemperature(u); },
             "The minimum water temperature during previous period.",
             PrintProperty::FIELD | PrintProperty::JSON);

    addPrint("max_flow_temperature", Quantity::Temperature,
             [&](Unit u){ return maxFlowTemperature(u); },
             "The maximum water temperature during previous period.",
             PrintProperty::FIELD | PrintProperty::JSON);

    addPrint("external_temperature", Quantity::Temperature,
             [&](Unit u){ return externalTemperature(u); },
             "The external temperature outside of the meter.",
             PrintProperty::FIELD | PrintProperty::JSON);

    addPrint("", Quantity::Text,
             [&](){ return statusHumanReadable(); },
             "Status of meter.",
             PrintProperty::FIELD);

    addPrint("current_status", Quantity::Text,
             [&](){ return status(); },
             "Status of meter.",
             PrintProperty::JSON);

    addPrint("time_dry", Quantity::Text,
             [&](){ return timeDry(); },
             "Amount of time the meter has been dry.",
             PrintProperty::JSON);

    addPrint("time_reversed", Quantity::Text,
             [&](){ return timeReversed(); },
             "Amount of time the meter has been reversed.",
             PrintProperty::JSON);

    addPrint("time_leaking", Quantity::Text,
             [&](){ return timeLeaking(); },
             "Amount of time the meter has been leaking.",
             PrintProperty::JSON);

    addPrint("time_bursting", Quantity::Text,
             [&](){ return timeBursting(); },
             "Amount of time the meter has been bursting.",
             PrintProperty::JSON);
}

double MeterFlowIQ2200::totalWaterConsumption(Unit u)
{
    assertQuantity(u, Quantity::Volume);
    return convert(total_water_consumption_m3_, Unit::M3, u);
}

bool MeterFlowIQ2200::hasTotalWaterConsumption()
{
    return has_total_water_consumption_;
}

double MeterFlowIQ2200::targetWaterConsumption(Unit u)
{
    assertQuantity(u, Quantity::Volume);
    return convert(target_water_consumption_m3_, Unit::M3, u);
}

bool MeterFlowIQ2200::hasTargetWaterConsumption()
{
    return has_target_water_consumption_;
}

double MeterFlowIQ2200::currentFlow(Unit u)
{
    assertQuantity(u, Quantity::Flow);
    return convert(current_flow_m3h_, Unit::M3H, u);
}

double MeterFlowIQ2200::maxFlow(Unit u)
{
    assertQuantity(u, Quantity::Flow);
    return convert(max_flow_m3h_, Unit::M3H, u);
}

double MeterFlowIQ2200::minFlow(Unit u)
{
    assertQuantity(u, Quantity::Flow);
    return convert(min_flow_m3h_, Unit::M3H, u);
}

bool MeterFlowIQ2200::hasMaxFlow()
{
    return true;
}

double MeterFlowIQ2200::flowTemperature(Unit u)
{
    assertQuantity(u, Quantity::Temperature);
    return convert(127, Unit::C, u);
}

double MeterFlowIQ2200::minFlowTemperature(Unit u)
{
    assertQuantity(u, Quantity::Temperature);
    return convert(min_flow_temperature_c_, Unit::C, u);
}

double MeterFlowIQ2200::maxFlowTemperature(Unit u)
{
    assertQuantity(u, Quantity::Temperature);
    return convert(max_flow_temperature_c_, Unit::C, u);
}

bool MeterFlowIQ2200::hasFlowTemperature()
{
    // Actually no, it has only min and max.
    return false;
}

double MeterFlowIQ2200::externalTemperature(Unit u)
{
    assertQuantity(u, Quantity::Temperature);
    return convert(external_temperature_c_, Unit::C, u);
}

bool MeterFlowIQ2200::hasExternalTemperature()
{
    return has_external_temperature_;
}

shared_ptr<Meter> createFlowIQ2200(MeterInfo &mi)
{
    return shared_ptr<Meter>(new MeterFlowIQ2200(mi, "flowiq2200"));
}

void MeterFlowIQ2200::processContent(Telegram *t)
{
    /*
(flowiq2200) 14: 04 dif (32 Bit Integer/Binary Instantaneous value)
(flowiq2200) 15: FF vif (Vendor extension)
(flowiq2200) 16: 23 vife (per day)
(flowiq2200) 17: * 00000000 info codes (OK)
(flowiq2200) 1b: 04 dif (32 Bit Integer/Binary Instantaneous value)
(flowiq2200) 1c: 13 vif (Volume l)
(flowiq2200) 1d: * AEAC0000 total consumption (44.206000 m3)
(flowiq2200) 21: 44 dif (32 Bit Integer/Binary Instantaneous value storagenr=1)
(flowiq2200) 22: 13 vif (Volume l)
(flowiq2200) 23: * 64A80000 target consumption (43.108000 m3)
(flowiq2200) 27: 42 dif (16 Bit Integer/Binary Instantaneous value storagenr=1)
(flowiq2200) 28: 6C vif (Date type G)
(flowiq2200) 29: * 812A target_datetime (2020-10-01 00:00)
(flowiq2200) 2b: 02 dif (16 Bit Integer/Binary Instantaneous value)
(flowiq2200) 2c: 3B vif (Volume flow l/h)
(flowiq2200) 2d: * 0000 current flow (0.000000 m3/h)
(flowiq2200) 2f: 92 dif (16 Bit Integer/Binary Maximum value)
(flowiq2200) 30: 01 dife (subunit=0 tariff=0 storagenr=2)
(flowiq2200) 31: 3B vif (Volume flow l/h)
(flowiq2200) 32: * EF01 max flow (0.495000 m3/h)
(flowiq2200) 34: A2 dif (16 Bit Integer/Binary Minimum value)
(flowiq2200) 35: 01 dife (subunit=0 tariff=0 storagenr=2)
(flowiq2200) 36: 3B vif (Volume flow l/h)
(flowiq2200) 37: * 0000 min flow (0.000000 m3/h)
(flowiq2200) 39: 06 dif (48 Bit Integer/Binary Instantaneous value)
(flowiq2200) 3a: FF vif (Vendor extension)
(flowiq2200) 3b: 1B vife (?)
(flowiq2200) 3c: 067000097000
(flowiq2200) 42: A1 dif (8 Bit Integer/Binary Minimum value)
(flowiq2200) 43: 01 dife (subunit=0 tariff=0 storagenr=2)
(flowiq2200) 44: 5B vif (Flow temperature °C)
(flowiq2200) 45: * 0C min flow temperature (12.000000 °C)
(flowiq2200) 46: 91 dif (8 Bit Integer/Binary Maximum value)
(flowiq2200) 47: 01 dife (subunit=0 tariff=0 storagenr=2)
(flowiq2200) 48: 5B vif (Flow temperature °C)
(flowiq2200) 49: * 14 max flow temperature (20.000000 °C)
(flowiq2200) 4a: A1 dif (8 Bit Integer/Binary Minimum value)
(flowiq2200) 4b: 01 dife (subunit=0 tariff=0 storagenr=2)
(flowiq2200) 4c: 67 vif (External temperature °C)
(flowiq2200) 4d: * 13 external temperature (19.000000 °C)
    */
    string meter_name = toString(driver()).c_str();

    int offset;
    string key;

    extractDVuint16(&t->dv_entries, "04FF23", &offset, &info_codes_);
    t->addMoreExplanation(offset, " info codes (%s)", statusHumanReadable().c_str());

    if(findKey(MeasurementType::Instantaneous, VIFRange::Volume, 0, 0, &key, &t->dv_entries)) {
        extractDVdouble(&t->dv_entries, key, &offset, &total_water_consumption_m3_);
        has_total_water_consumption_ = true;
        t->addMoreExplanation(offset, " total consumption (%f m3)", total_water_consumption_m3_);
    }

    if(findKey(MeasurementType::Unknown, VIFRange::Volume, 1, 0, &key, &t->dv_entries)) {
        extractDVdouble(&t->dv_entries, key, &offset, &target_water_consumption_m3_);
        has_target_water_consumption_ = true;
        t->addMoreExplanation(offset, " target consumption (%f m3)", target_water_consumption_m3_);
    }

    if (findKey(MeasurementType::Unknown, VIFRange::Date, 1, 0, &key, &t->dv_entries)) {
        struct tm datetime;
        extractDVdate(&t->dv_entries, key, &offset, &datetime);
        target_datetime_ = strdatetime(&datetime);
        t->addMoreExplanation(offset, " target_datetime (%s)", target_datetime_.c_str());
    }

    if(findKey(MeasurementType::Instantaneous, VIFRange::VolumeFlow, 0, 0, &key, &t->dv_entries)) {
        extractDVdouble(&t->dv_entries, key, &offset, &current_flow_m3h_);
        t->addMoreExplanation(offset, " current flow (%f m3/h)", current_flow_m3h_);
    }

    if(findKey(MeasurementType::Maximum, VIFRange::VolumeFlow, 2, 0, &key, &t->dv_entries)) {
        extractDVdouble(&t->dv_entries, key, &offset, &max_flow_m3h_);
        t->addMoreExplanation(offset, " max flow (%f m3/h)", max_flow_m3h_);
    }

    if(findKey(MeasurementType::Minimum, VIFRange::VolumeFlow, 2, 0, &key, &t->dv_entries)) {
        extractDVdouble(&t->dv_entries, key, &offset, &min_flow_m3h_);
        t->addMoreExplanation(offset, " min flow (%f m3/h)", min_flow_m3h_);
    }

    if(findKey(MeasurementType::Minimum, VIFRange::FlowTemperature, 2, 0, &key, &t->dv_entries)) {
        extractDVdouble(&t->dv_entries, key, &offset, &min_flow_temperature_c_);
        t->addMoreExplanation(offset, " min flow temperature (%f °C)", min_flow_temperature_c_);
    }

    if(findKey(MeasurementType::Maximum, VIFRange::FlowTemperature, 2, 0, &key, &t->dv_entries)) {
        extractDVdouble(&t->dv_entries, key, &offset, &max_flow_temperature_c_);
        t->addMoreExplanation(offset, " max flow temperature (%f °C)", max_flow_temperature_c_);
    }

    if(findKey(MeasurementType::Unknown, VIFRange::ExternalTemperature, AnyStorageNr, 0, &key, &t->dv_entries)) {
        has_external_temperature_ = extractDVdouble(&t->dv_entries, key, &offset, &external_temperature_c_);
        t->addMoreExplanation(offset, " external temperature (%f °C)", external_temperature_c_);
    }

}

string MeterFlowIQ2200::status()
{
    string s;
    if (info_codes_ & INFO_CODE_DRY) s.append("DRY ");
    if (info_codes_ & INFO_CODE_REVERSE) s.append("REVERSED ");
    if (info_codes_ & INFO_CODE_LEAK) s.append("LEAK ");
    if (info_codes_ & INFO_CODE_BURST) s.append("BURST ");
    if (s.length() > 0) {
        s.pop_back(); // Remove final space
        return s;
    }
    return s;
}

string MeterFlowIQ2200::timeDry()
{
    int time_dry = (info_codes_ >> INFO_CODE_DRY_SHIFT) & 7;
    if (time_dry) {
        return decodeTime(time_dry);
    }
    return "";
}

string MeterFlowIQ2200::timeReversed()
{
    int time_reversed = (info_codes_ >> INFO_CODE_REVERSE_SHIFT) & 7;
    if (time_reversed) {
        return decodeTime(time_reversed);
    }
    return "";
}

string MeterFlowIQ2200::timeLeaking()
{
    int time_leaking = (info_codes_ >> INFO_CODE_LEAK_SHIFT) & 7;
    if (time_leaking) {
        return decodeTime(time_leaking);
    }
    return "";
}

string MeterFlowIQ2200::timeBursting()
{
    int time_bursting = (info_codes_ >> INFO_CODE_BURST_SHIFT) & 7;
    if (time_bursting) {
        return decodeTime(time_bursting);
    }
    return "";
}

string MeterFlowIQ2200::statusHumanReadable()
{
    string s;
    bool dry = info_codes_ & INFO_CODE_DRY;
    int time_dry = (info_codes_ >> INFO_CODE_DRY_SHIFT) & 7;
    if (dry || time_dry) {
        if (dry) s.append("DRY");
        s.append("(dry ");
        s.append(decodeTime(time_dry));
        s.append(") ");
    }

    bool reversed = info_codes_ & INFO_CODE_REVERSE;
    int time_reversed = (info_codes_ >> INFO_CODE_REVERSE_SHIFT) & 7;
    if (reversed || time_reversed) {
        if (dry) s.append("REVERSED");
        s.append("(rev ");
        s.append(decodeTime(time_reversed));
        s.append(") ");
    }

    bool leak = info_codes_ & INFO_CODE_LEAK;
    int time_leak = (info_codes_ >> INFO_CODE_LEAK_SHIFT) & 7;
    if (leak || time_leak) {
        if (dry) s.append("LEAK");
        s.append("(leak ");
        s.append(decodeTime(time_leak));
        s.append(") ");
    }

    bool burst = info_codes_ & INFO_CODE_BURST;
    int time_burst = (info_codes_ >> INFO_CODE_BURST_SHIFT) & 7;
    if (burst || time_burst) {
        if (dry) s.append("BURST");
        s.append("(burst ");
        s.append(decodeTime(time_burst));
        s.append(") ");
    }
    if (s.length() > 0) {
        s.pop_back();
        return s;
    }
    return "OK";
}

string MeterFlowIQ2200::decodeTime(int time)
{
    if (time>7) {
        string meter_name = toString(driver()).c_str();
        warning("(%s) warning: Cannot decode time %d should be 0-7.\n", meter_name.c_str(), time);
    }
    switch (time) {
    case 0:
        return "0 hours";
    case 1:
        return "1-8 hours";
    case 2:
        return "9-24 hours";
    case 3:
        return "2-3 days";
    case 4:
        return "4-7 days";
    case 5:
        return "8-14 days";
    case 6:
        return "15-21 days";
    case 7:
        return "22-31 days";
    default:
        return "?";
    }
}
