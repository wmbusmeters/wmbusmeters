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
    void processExtras(string miExtras);

    double total_water_consumption_m3_ {};

    int registerSize(int c);
};

shared_ptr<WaterMeter> createApator162(MeterInfo &mi)
{
    return shared_ptr<WaterMeter>(new MeterApator162(mi));
}

MeterApator162::MeterApator162(MeterInfo &mi) :
    MeterCommonImplementation(mi, MeterDriver::APATOR162)
{
    processExtras(mi.extras);

    setExpectedTPLSecurityMode(TPLSecurityMode::AES_CBC_IV);

    addLinkMode(LinkMode::T1);
    addLinkMode(LinkMode::C1);

    addPrint("total", Quantity::Volume,
             [&](Unit u){ return totalWaterConsumption(u); },
             "The total water consumption recorded by this meter.",
             true, true);
}

void MeterApator162::processExtras(string miExtras)
{
    map<string,string> extras;
    bool ok = parseExtras(miExtras, &extras);
    if (!ok)
    {
        error("(apator162) invalid extra parameters (%s)\n", miExtras.c_str());
    }
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
    // simply wrapped inside a wmbus telegram. Naughty!

    // Anyway, it seems like telegram is broken up into registers.
    // Each register is identified with a single byte after which the content follows.
    // For example, the total volume is marked by 0x10 followed by 4 bytes.

    vector<uchar> content;
    t->extractPayload(&content);

    map<string,pair<int,DVEntry>> vendor_values;

    size_t i=0;
    while (i < content.size())
    {
        int c = content[i];
        int size = registerSize(c);
        if (c == 0xff) break; // An FF signals end of telegram padded to encryption boundary,
        // FFFFFFF623A where 4 last are perhaps crc or counter?
        i++;
        if (size == -1 || i+size >= content.size())
        {
            vector<uchar> frame;
            t->extractFrame(&frame);
            string hex = bin2hex(frame);
            warning("(apator162) telegram contains a register (%02x) with unknown size \n"
                    "Please open an issue at https://github.com/weetmuts/wmbusmeters/\n"
                    "and report this telegram: %s\n", c, hex.c_str());
            break;
        }
        if (c == 0x10 && size == 4 && i+size < content.size())
        {
            // We found the register representing the total
            string total;
            strprintf(total, "%02x%02x%02x%02x", content[i+0], content[i+1], content[i+2], content[i+3]);
            vendor_values["0413"] = {i-1+t->header_size, DVEntry(MeasurementType::Instantaneous, 0x13, 0, 0, 0, total) };
            int offset;
            extractDVdouble(&vendor_values, "0413", &offset, &total_water_consumption_m3_);
            total = "*** 10|"+total+" total consumption (%f m3)";
            t->addSpecialExplanation(offset, total.c_str(), total_water_consumption_m3_);
        }
        else
        {
            string msg = "*** ";
            msg += bin2hex(content, i-1, 1)+"|"+bin2hex(content, i, size);
            t->addSpecialExplanation(i-1+t->header_size, msg.c_str());
        }
        i += size;
    }
}

int MeterApator162::registerSize(int c)
{
    switch (c)
    {
    case 0x0f: return 3; // Payload often starts with 0x0f,
    // which  also means dif = manufacturer data follows.
    case 0x10: return 4; // Total volume

    case 0x40: return 2;
    case 0x41: return 2;
    case 0x42: return 4;
    case 0x43: return 2;

    case 0x73: return 1+4*4; // Historical data
    case 0x75: return 1+6*4; // Historical data
    case 0x7B: return 1+12*4; // Historical data

    case 0x80: return 3; // Apparently payload can also start with 0x80, but hey,
        // what happened to 0x0f which indicated mfct data? 0x80 is a valid dif
        // now its impossible to see that the telegram contains mfct data....
        // except by using the mfct/type/version info.
    case 0x81: return 10;
    case 0x83: return 10;
    case 0x84: return 10;
    case 0x87: return 10;

    case 0x92: return 3;
    case 0x93: return 3;
    case 0x94: return 3;
    case 0x95: return 3;

    case 0xA0: return 4;

    case 0xB4: return 3;

    case 0xF0: return 4;
    }
    return -1;
}
