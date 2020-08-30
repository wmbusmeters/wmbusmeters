/*
 Copyright (C) 2017-2020 Fredrik Öhrström

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

struct MeterApator162 : public virtual WaterMeter, public virtual MeterCommonImplementation {
    MeterApator162(MeterInfo &mi);

    // Total water counted through the meter
    double totalWaterConsumption(Unit u);
    bool  hasTotalWaterConsumption();

private:

    void processContent(Telegram *t);

    double total_water_consumption_m3_ {};
};

unique_ptr<WaterMeter> createApator162(MeterInfo &mi)
{
    return unique_ptr<WaterMeter>(new MeterApator162(mi));
}

MeterApator162::MeterApator162(MeterInfo &mi) :
    MeterCommonImplementation(mi, MeterType::APATOR162, MANUFACTURER_APA)
{
    setExpectedTPLSecurityMode(TPLSecurityMode::AES_CBC_IV);

    addMedia(0x06);
    addMedia(0x07);

    addLinkMode(LinkMode::T1);
    addLinkMode(LinkMode::C1);

    addExpectedVersion(0x05);

    addPrint("total", Quantity::Volume,
             [&](Unit u){ return totalWaterConsumption(u); },
             "The total water consumption recorded by this meter.",
             true, true);
}

double MeterApator162::totalWaterConsumption(Unit u)
{
    assertQuantity(u, Quantity::Volume);
    return convert(total_water_consumption_m3_, Unit::M3, u);
}

bool MeterApator162::hasTotalWaterConsumption()
{
    return true;
}

void MeterApator162::processContent(Telegram *t)
{
    // Unfortunately, the at-wmbus-16-2 is mostly a proprietary protocol
    // simple wrapped inside a wmbus telegram. Naughty!

    vector<uchar> content;

    t->extractPayload(&content);

    map<string,pair<int,DVEntry>> vendor_values;

    string total;
    // Current assumption of this proprietary protocol is that byte 13 tells
    // us where the current total water consumption is located.
    int o = 0;
    uchar guess10 = content[10];
    uchar guess11 = content[11];
    uchar guess12 = content[12];
    if ((guess11 & 0x84) == 0x84)
    {
        o = 23;
    }
    else
    if ((guess11 & 0x83) == 0x83)
    {
        o = 23;
    }
    else
    if ((guess11 & 0x81) == 0x81)
    {
        if (guess10 == 02)
        {
            o = 23;
        }
        else
        {
            o = 20;
        }
    }
    else
    if ((guess11 & 0x40) == 0x40)
    {
        o = 20;
    }
    else
    if ((guess11 & 0x10) == 0x10)
    {
        o = 12;
    }
    else
    if ((guess11 & 0x01) == 0x01)
    {
        o = 9;
    }
    else
    {
        warning("(apator162) Unknown value in proprietary(unknown) apator162 protocol. Ignoring telegram. Found 0x%02x expected bit 0x01, 0x10, 0x40 or 0x80 to be set.\n", guess11);
        return;
    }

    uint32_t o9 = content[9] | content[9+1] <<8 | content[9+2] << 16 | content[9+3] << 24;
    uint32_t o12 = content[12] | content[12+1] <<8 | content[12+2] << 16 | content[12+3] << 24;
    uint32_t o20 = content[20] | content[20+1] <<8 | content[20+2] << 16 | content[20+3] << 24;
    uint32_t o23 = content[23] | content[23+1] <<8 | content[23+2] << 16 | content[23+3] << 24;
    uint32_t guess = content[o] | content[o+1] <<8 | content[o+2] << 16 | content[o+3] << 24;

    strprintf(total, "%02x%02x%02x%02x", content[o], content[o+1], content[o+2], content[o+3]);
    debug("(apator162) Guessing offset to be %d from byte >10=%02x 11=%02x 12=%02x<: total %s\n",
          o, guess10, guess11, guess12, total.c_str());

    debug("(apator162) other potential values o9=%u o12=%u o20=%u o23=%u guess=%u\n", o9, o12, o20, o23, guess);

    // Ok, the guess might not good enough. Lets do a sanity check and revert to another offset
    // that has a reasonable value.....at least it should be less than 58400000 liters.....why?
    // Let us assume a 16 year lifetime of the apator meter, 10 m3 per day for 16 years = 16*365*10000 = 58400000
#define MAX 58400000
    if (guess > MAX)
    {
        if (o9 < MAX)
        {
            o = 9;
        }
        else if (o12 < MAX)
        {
            o = 12;
        }
        else if (o20 < MAX)
        {
            o = 20;
        }
        else if (o23 < MAX)
        {
            o = 23;
        }
        strprintf(total, "%02x%02x%02x%02x", content[o], content[o+1], content[o+2], content[o+3]);
        debug("(apator162) adjusting to offset %d instead\n", o);
    }

    vendor_values["0413"] = { 25, DVEntry(MeasurementType::Instantaneous, 0x13, 0, 0, 0, total) };
    int offset;
    string key;
    if(findKey(MeasurementType::Unknown, ValueInformation::Volume, 0, 0, &key, &vendor_values))
    {
        extractDVdouble(&vendor_values, key, &offset, &total_water_consumption_m3_);
        //Adding explanation have to wait since it assumes that the dvparser could do something, but it could not here.
        //t->addMoreExplanation(offset, " total consumption (%f m3)", total_water_consumption_m3_);
    }
}
