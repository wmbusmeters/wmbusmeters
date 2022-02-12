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

struct MeterRfmTX1 : public virtual MeterCommonImplementation {
    MeterRfmTX1(MeterInfo &mi);

    // Total water counted through the meter
    double totalWaterConsumption(Unit u);
    bool  hasTotalWaterConsumption();

private:
    void processContent(Telegram *t);

    double total_water_consumption_m3_ {};
    string meter_datetime_;
};

shared_ptr<Meter> createRfmTX1(MeterInfo &mi)
{
    return shared_ptr<Meter>(new MeterRfmTX1(mi));
}

MeterRfmTX1::MeterRfmTX1(MeterInfo &mi) :
    MeterCommonImplementation(mi, "rfmtx1")
{
    setMeterType(MeterType::WaterMeter);

    setExpectedTPLSecurityMode(TPLSecurityMode::AES_CBC_IV);

    addLinkMode(LinkMode::T1);

    addPrint("total", Quantity::Volume,
             [&](Unit u){ return totalWaterConsumption(u); },
             "The total water consumption recorded by this meter.",
             true, true);

    addPrint("meter_datetime", Quantity::Text,
             [&](){ return meter_datetime_; },
             "A date.....",
             true, true);
}

uchar decode_vectors_[16][6] = { { 117, 150, 122, 16, 26, 10 }, { 91, 127, 112, 19, 34, 19 }, { 179, 24, 185, 11, 142, 153 }, { 142, 125, 121, 7, 74, 22 }, { 181, 145, 7, 154, 203, 105 }, { 184, 163, 50, 161, 57, 14 }, { 189, 128, 156, 126, 96, 153 }, { 39, 92, 180, 196, 128, 163 }, { 48, 208, 10, 206, 25, 3 }, { 194, 76, 240, 5, 165, 134 }, { 84, 75, 22, 152, 17, 94 }, { 75, 238, 12, 201, 125, 162 }, { 135, 202, 74, 72, 228, 31 }, { 196, 135, 119, 46, 138, 232 }, { 227, 48, 189, 120, 87, 140 }, { 164, 154, 57, 111, 40, 5 } };

void MeterRfmTX1::processContent(Telegram *t)
{
    int offset;
    string key;

    if (t->tpl_cfg == 0x1006)
    {
        // This is the old type of meter and some values needs to be de-obfuscated.
        vector<uchar> frame;
        t->extractFrame(&frame);

        debugPayload("(rftx1) decoding raw frame", frame);

        uchar decoded_total[6];

        for (int i = 0; i < 6; ++i)
        {
            decoded_total[i] = (uchar)(frame[0xf + i] ^ frame[0xb] ^ decode_vectors_[frame[0xb] & 0x0f][i]);
        }

        double total = 0;
        double mul = 1;
        for (int i = 2; i < 6; ++i)
        {
            total += mul*bcd2bin(decoded_total[i]);
            mul *= 100;
        }
        total_water_consumption_m3_ = total/1000.0;

        int o = 28; // Offset to datetime.
        int y = 2000+bcd2bin(frame[o+5]);
        int M = bcd2bin(frame[o+4]);
        int d = bcd2bin(frame[o+3]);
        int H = bcd2bin(frame[o+2]);
        int m = bcd2bin(frame[o+1]);
        int s = bcd2bin(frame[o+0]);

        strprintf(meter_datetime_, "%d-%02d-%02d %02d:%02d:%02d",
                  y, M%99, d%99, H%99, m%99, s%99);

        return;
    }

    if(findKey(MeasurementType::Unknown, ValueInformation::Volume, 0, 0, &key, &t->values)) {
        extractDVdouble(&t->values, key, &offset, &total_water_consumption_m3_);
        t->addMoreExplanation(offset, " total consumption (%f m3)", total_water_consumption_m3_);
    }

    if (findKey(MeasurementType::Unknown, ValueInformation::DateTime, 0, 0, &key, &t->values)) {
        struct tm datetime;
        extractDVdate(&t->values, key, &offset, &datetime);
        meter_datetime_ = strdatetime(&datetime);
        t->addMoreExplanation(offset, " meter_datetime (%s)", meter_datetime_.c_str());
    }
}

double MeterRfmTX1::totalWaterConsumption(Unit u)
{
    assertQuantity(u, Quantity::Volume);
    return convert(total_water_consumption_m3_, Unit::M3, u);
}

bool MeterRfmTX1::hasTotalWaterConsumption()
{
    return true;
}
