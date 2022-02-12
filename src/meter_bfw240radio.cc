/*
 Copyright (C) 2021 Fredrik Öhrström (gpl-3.0-or-later)

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
#include"units.h"
#include"util.h"
#include <ctime>

struct MeterBFW240RADIO : public virtual MeterCommonImplementation
{
    MeterBFW240RADIO(MeterInfo &mi);

    double currentPeriodEnergyConsumption(Unit u);
    string currentPeriodDate();
    double previousPeriodEnergyConsumption(Unit u);
    string previousPeriodDate();
    double currentRoomTemperature(Unit u);
    double currentRadiatorTemperature(Unit u);

    private:

    void processContent(Telegram *t);

    string leadingZeroString(int num);

    double curr_energy_hca_ {};
    string curr_energy_hca_date {};
    double prev_energy_hca_ {};
    string prev_energy_hca_date {};
    double temp_room_ {};
    double temp_radiator_ {};
};

shared_ptr<Meter> createBFW240Radio(MeterInfo &mi)
{
    return shared_ptr<Meter>(new MeterBFW240RADIO(mi));
}


MeterBFW240RADIO::MeterBFW240RADIO(MeterInfo &mi) :
    MeterCommonImplementation(mi, "bfw240radio")
{
    setMeterType(MeterType::HeatCostAllocationMeter);

    // media 0x80 T telegrams
    addLinkMode(LinkMode::T1);

    addPrint("current", Quantity::HCA,
             [&](Unit u){ return currentPeriodEnergyConsumption(u); },
             "Energy consumption so far in this billing period.",
             true, true);

/*    addPrint("current_date", Quantity::Text,
             [&](){ return currentPeriodDate(); },
             "Date of current billing period.",
             true, true);
*/
    addPrint("previous", Quantity::HCA,
             [&](Unit u){ return previousPeriodEnergyConsumption(u); },
             "Energy consumption in previous billing period.",
             true, true);

    /*
    addPrint("previous_date", Quantity::Text,
             [&](){ return previousPeriodDate(); },
             "Date of last billing period.",
             true, true);

    addPrint("temp_room", Quantity::Temperature,
             [&](Unit u){ return currentRoomTemperature(u); },
             "Current room temperature.",
             true, true);

    addPrint("temp_radiator", Quantity::Temperature,
             [&](Unit u){ return currentRadiatorTemperature(u); },
             "Current radiator temperature.",
             true, true);
    */
}

double MeterBFW240RADIO::currentPeriodEnergyConsumption(Unit u)
{
    return curr_energy_hca_;
}

string MeterBFW240RADIO::currentPeriodDate()
{
    return curr_energy_hca_date;
}

double MeterBFW240RADIO::previousPeriodEnergyConsumption(Unit u)
{
    return prev_energy_hca_;
}

string MeterBFW240RADIO::previousPeriodDate()
{
    return prev_energy_hca_date;
}

double MeterBFW240RADIO::currentRoomTemperature(Unit u)
{
    assertQuantity(u, Quantity::Temperature);
    return convert(temp_room_, Unit::C, u);
}

double MeterBFW240RADIO::currentRadiatorTemperature(Unit u)
{
    assertQuantity(u, Quantity::Temperature);
    return convert(temp_radiator_, Unit::C, u);
}

void MeterBFW240RADIO::processContent(Telegram *t)
{
    // Unfortunately, the Techem FHKV data ii/iii is mostly a proprieatary protocol
    // simple wrapped inside a wmbus telegram since the ci-field is 0xa0.
    // Which means that the entire payload is manufacturer specific.

    vector<uchar> content;

    t->extractPayload(&content);

    debugPayload("THE PAYLOAD", content);

    if (content.size() < 2) return;
    int offset = 0;
    if (content[0] == 0x2f && content[0] == 0x2f) offset = 2;

    // Consumption
    // Previous Consumption
    uchar prev_hi = content[offset+2];
    uchar prev_lo = content[offset+3];
    double prev = (256.0*prev_hi+prev_lo);
    prev_energy_hca_ = prev;

    string prevs;
    strprintf(prevs, "%02x%02x", prev_lo, prev_hi);

    /*
    // Previous Date
    uchar date_prev_lo = content[1];
    uchar date_prev_hi = content[2];
    int date_prev = (256.0*date_prev_hi+date_prev_lo);

    int day_prev = (date_prev >> 0) & 0x1F;
    int month_prev = (date_prev >> 5) & 0x0F;
    int year_prev = (date_prev >> 9) & 0x3F;
    prev_energy_hca_date = std::to_string((year_prev + 2000)) + "-" + leadingZeroString(month_prev) + "-" + leadingZeroString(day_prev) + "T02:00:00Z";
*/
    //t->addMoreExplanation(offset, " last date of previous billing period (%s)", prev_energy_hca_date);

    // Current Consumption
    uchar curr_hi = content[offset+4];
    uchar curr_lo = content[offset+5];
    double curr = (256.0*curr_hi+curr_lo);
    curr_energy_hca_ = curr;

    string currs;
    strprintf(currs, "%02x%02x", curr_lo, curr_hi);

    /*
    // Current Date
    uchar date_curr_lo = content[5];
    uchar date_curr_hi = content[6];
    int date_curr = (256.0*date_curr_hi+date_curr_lo);

    time_t now = time(0);
    tm *ltm = localtime(&now);
    int year_curr = 1900 + ltm->tm_year;

    int day_curr = (date_curr >> 4) & 0x1F;
    if (day_curr <= 0) day_curr = 1;
    int month_curr = (date_curr >> 9) & 0x0F;
    if (month_curr <= 0) month_curr = 12;
    curr_energy_hca_date = to_string(year_curr) + "-" + leadingZeroString(month_curr) + "-" + leadingZeroString(day_curr) + "T02:00:00Z";

    // t->addMoreExplanation(offset, " last date of current billing period (%s)", curr_energy_hca_date);

    // Temperature
    uchar room_tlo;
    uchar room_thi;
    uchar radiator_tlo;
    uchar radiator_thi;
    if(t->dll_version == 0x94)
    {
        room_tlo = content[10];
        room_thi = content[11];
        radiator_tlo = content[12];
        radiator_thi = content[13];
    } else
    {
        room_tlo = content[9];
        room_thi = content[10];
        radiator_tlo = content[11];
        radiator_thi = content[12];
    }

    // Room Temperature
    double room_t = (256.0*room_thi+room_tlo)/100;
    temp_room_ = room_t;

    string room_ts;
    strprintf(room_ts, "%02x%02x", room_tlo, room_thi);
    // t->addMoreExplanation(offset, " current room temparature (%f °C)", room_ts);

    // Radiator Temperature
    double radiator_t = (256.0*radiator_thi+radiator_tlo)/100;
    temp_radiator_ = radiator_t;

    string radiator_ts;
    strprintf(radiator_ts, "%02x%02x", radiator_tlo, radiator_thi);
    // t->addMoreExplanation(offset, " current radiator temparature (%f °C)", radiator_ts);
    */
}

string MeterBFW240RADIO::leadingZeroString(int num) {
    string new_num = (num < 10 ? "0": "") + std::to_string(num);
    return new_num;
}
