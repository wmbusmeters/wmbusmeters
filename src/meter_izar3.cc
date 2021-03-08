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

#include"meters.h"
#include"meters_common_implementation.h"
#include"wmbus.h"
#include"wmbus_utils.h"

#include<algorithm>
#include<stdbool.h>

using namespace std;

struct MeterIzar3 : public virtual WaterMeter, public virtual MeterCommonImplementation {
    MeterIzar3(MeterInfo &mi);

    double totalWaterConsumption(Unit u);
    bool  hasTotalWaterConsumption();

private:

    void processContent(Telegram *t);
    double total_water_consumption_l_ {};
};

shared_ptr<WaterMeter> createIzar3(MeterInfo &mi)
{
    return shared_ptr<WaterMeter>(new MeterIzar3(mi));
}

MeterIzar3::MeterIzar3(MeterInfo &mi) :
    MeterCommonImplementation(mi, MeterDriver::IZAR3)
{
    // We do not know how to decode the IZAR r3 aka Diehl AQUARIUS!
    addLinkMode(LinkMode::T1);

    addPrint("total", Quantity::Volume,
             [&](Unit u){ return totalWaterConsumption(u); },
             "The total water consumption recorded by this meter.",
             true, true);
}

double MeterIzar3::totalWaterConsumption(Unit u)
{
    assertQuantity(u, Quantity::Volume);
    return convert(total_water_consumption_l_, Unit::L, u);
}

bool MeterIzar3::hasTotalWaterConsumption()
{
    return true;
}

void MeterIzar3::processContent(Telegram *t)
{
    vector<uchar> frame;
    t->extractFrame(&frame);

    warning("(izar3) cannot decode content of telegram!\n");
    total_water_consumption_l_ = 123456789;
}
