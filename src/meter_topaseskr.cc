/*
 Copyright (C) 2017-2020 Fredrik Öhrström
                    2020 Avandorp

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

/*
 AquaMetro / Integra water meter "TOPAS ES KR"
 Models TOPAS ES KR 95077 95056 95345 95490 95373 95059 95065 95068 95071 95074 should be compatible. Only 95059 in one configuration tested.
 Identifies itself with Manufacturer "AMT" and Version "f1"
 Product leaflet and observation says the following values are sent:
 Current total volume
 Total volume at end of year-period day (that means: current total volume - total volume at end of year-period day = current year-periods volume up until now)
 Total backward volume on end of year-period day or total backward volume in current year-period. Backward volume remains untested (luckily only 0 values encountered).
 Date of end of last year-period day
 Total volume at end of last month-period dateTime
 DateTime of end of last month-period
 Current flow rate
 Battery life (days left)
 Water temperature

 Example telegram:
 telegram=|4E44B40512345678F1077A310040052F2F|01FD08040C13991848004C1359423500CC101300000000CC201359423500426C7F2C0B3B00000002FD74DA10025AD300C4016D3B179F27CC011387124600|+2
*/
struct MeterTopasEsKr : public virtual WaterMeter, public virtual MeterCommonImplementation {
    MeterTopasEsKr(MeterInfo &mi);

    double totalWaterConsumption(Unit u);
    bool  hasTotalWaterConsumption();

    double flowTemperature(Unit u);
    bool  hasFlowTemperature();

private:
    void processContent(Telegram *t);

    double total_water_consumption_m3_ {};
    double flow_temperature_ {};
    double current_flow_m3h_ {};
    string battery_life_days_remaining_ {};
    double volume_year_period_m3_ {};
    double reverse_volume_year_period_m3_ {};
    double volume_month_period_m3_ {};
    string meter_yearly_period_date_;
    string meter_month_period_datetime_;
};

shared_ptr<Meter> createTopasEsKr(MeterInfo &mi)
{
    return shared_ptr<WaterMeter>(new MeterTopasEsKr(mi));
}

MeterTopasEsKr::MeterTopasEsKr(MeterInfo &mi) :
    MeterCommonImplementation(mi, MeterDriver::TOPASESKR)
{
    setExpectedTPLSecurityMode(TPLSecurityMode::AES_CBC_IV);

    // media 0x06 specified temperature range is 0°C to 50 °C, not sure it ever reports 0x06 for warm water, possibly configurable
    // media 0x07 used

    addLinkMode(LinkMode::T1);

    addPrint("total", Quantity::Volume,
             [&](Unit u){ return totalWaterConsumption(u); },
             "The total water consumption recorded by this meter.",
             true, true);
    addPrint("temperature", Quantity::Temperature,
             [&](Unit u){ return flowTemperature(u); },
             "Current water temperature recorded by this meter.",
             true, true);
    addPrint("current_flow", Quantity::Flow,
             [&](Unit u){ return current_flow_m3h_; },
             "Current flow.",
             true, true);
    addPrint("battery_life_days_remaining_remaining", Quantity::Text,
             [&](){ return battery_life_days_remaining_; },
             "Battery life [days remaining].",
             false, true);
    addPrint("volume_year_period", Quantity::Volume,
             [&](Unit u){ return volume_year_period_m3_; },
             "Volume up to end of last year-period.",
             true, true);
    addPrint("reverse_volume_year_period", Quantity::Volume,
             [&](Unit u){ return reverse_volume_year_period_m3_; },
             "Reverse volume in this year-period (?).",
             true, true);
    addPrint("meter_year_period_start_date", Quantity::Text,
             [&](){ return meter_yearly_period_date_; },
             "Meter date for year-period start.",
             true, true);
    addPrint("volume_month_period", Quantity::Volume,
             [&](Unit u){ return volume_month_period_m3_; },
             "Volume up to end of last month-period.",
             true, true);
    addPrint("meter_month_period_start_datetime", Quantity::Text,
             [&](){ return meter_month_period_datetime_; },
             "Meter timestamp for month-period start.",
             true, true);

}

void MeterTopasEsKr::processContent(Telegram *t)
{
    int offset;
    string key;

    if(findKey(MeasurementType::Unknown, ValueInformation::Volume, 0, 0, &key, &t->values)) {
        extractDVdouble(&t->values, key, &offset, &total_water_consumption_m3_);
        t->addMoreExplanation(offset, " total consumption (%f m3)", total_water_consumption_m3_);
    }
    if(findKey(MeasurementType::Unknown, ValueInformation::FlowTemperature, 0, 0, &key, &t->values)) {
        extractDVdouble(&t->values, key, &offset, &flow_temperature_);
        t->addMoreExplanation(offset, " water temperature (%f °C)", flow_temperature_);
    }
    if(findKey(MeasurementType::Unknown, ValueInformation::VolumeFlow, 0, 0, &key, &t->values)) {
        extractDVdouble(&t->values, key, &offset, &current_flow_m3h_);
        t->addMoreExplanation(offset, " current flow (%f m3/h)", current_flow_m3h_);
    }

    extractDVdouble(&t->values, "4C13", &offset, &volume_year_period_m3_);
    t->addMoreExplanation(offset, " volume up to end of last year-period (%f m3)", volume_year_period_m3_);

    extractDVdouble(&t->values, "CC1013", &offset, &reverse_volume_year_period_m3_);
    t->addMoreExplanation(offset, " reverse volume in this year-period (?) (%f m3)", reverse_volume_year_period_m3_);

    struct tm date;
    extractDVdate(&t->values, "426C", &offset, &date);
    meter_yearly_period_date_ = strdate(&date);
    t->addMoreExplanation(offset, " meter_start_year_period_date (%s)", meter_yearly_period_date_.c_str());

    extractDVdouble(&t->values, "CC0113", &offset, &volume_month_period_m3_);
    t->addMoreExplanation(offset, " volume up to end of last month-period (%f m3)", volume_month_period_m3_);

    struct tm datetime;
    extractDVdate(&t->values, "C4016D", &offset, &datetime);
    meter_month_period_datetime_ = strdatetime(&datetime);
    t->addMoreExplanation(offset, " meter_start_month_period_datetime (%s)", meter_month_period_datetime_.c_str());

    uint16_t tmp16;
    extractDVuint16(&t->values, "02FD74", &offset, &tmp16);
    strprintf(battery_life_days_remaining_, "%u", (unsigned int)tmp16);
    t->addMoreExplanation(offset, " battery life (%s days remaining)", battery_life_days_remaining_.c_str());

    vector<uchar> data;
    t->extractMfctData(&data);
}

double MeterTopasEsKr::totalWaterConsumption(Unit u)
{
    assertQuantity(u, Quantity::Volume);
    return convert(total_water_consumption_m3_, Unit::M3, u);
}

bool MeterTopasEsKr::hasTotalWaterConsumption()
{
    return true;
}

double MeterTopasEsKr::flowTemperature(Unit u)
{
    assertQuantity(u, Quantity::Temperature);
    return convert(flow_temperature_, Unit::C, u);
}

bool MeterTopasEsKr::hasFlowTemperature()
{
    return true;
}
