/*
 Copyright (C) 2019 Jacek Tomasiak

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

#include <algorithm>

using namespace std;

#define PRIOS_DEFAULT_KEY1 "39BC8A10E66D83F8"
#define PRIOS_DEFAULT_KEY2 "51728910E66D83F8"

struct MeterIzar : public virtual WaterMeter, public virtual MeterCommonImplementation {
    MeterIzar(WMBus *bus, MeterInfo &mi);

    // Total water counted through the meter
    double totalWaterConsumption(Unit u);
    bool  hasTotalWaterConsumption();

    double lastMonthTotalWaterConsumption(Unit u);

private:

    void processContent(Telegram *t);
    uint32_t convertKey(const char *hex);
    uint32_t convertKey(const vector<uchar> &bytes);
    uint32_t uint32FromBytes(const vector<uchar> &data, int offset, bool reverse = false);
    vector<uchar> decodePrios(const vector<uchar> &payload, uint32_t key);

    double total_water_consumption_l_ {};
    double last_month_total_water_consumption_l_ {};

    vector<uint32_t> keys;
};

unique_ptr<WaterMeter> createIzar(WMBus *bus, MeterInfo &mi)
{
    return unique_ptr<WaterMeter>(new MeterIzar(bus, mi));
}

MeterIzar::MeterIzar(WMBus *bus, MeterInfo &mi) :
    MeterCommonImplementation(bus, mi, MeterType::IZAR, MANUFACTURER_SAP)
{
    if (!key().empty())
        keys.push_back(convertKey(key()));

    // fallback to default keys if no custom key provided
    if (keys.empty())
    {
        keys.push_back(convertKey(PRIOS_DEFAULT_KEY1));
        keys.push_back(convertKey(PRIOS_DEFAULT_KEY2));
    }

    addMedia(0x01); // Oil meter? why?

    addLinkMode(LinkMode::T1);

    // meters with different versions exist, don't set any to avoid warnings
    // setExpectedVersion(0xd4); // or 0xcc

    addPrint("total", Quantity::Volume,
             [&](Unit u){ return totalWaterConsumption(u); },
             "The total water consumption recorded by this meter.",
             true, true);

    addPrint("last_month_total", Quantity::Volume,
             [&](Unit u){ return lastMonthTotalWaterConsumption(u); },
             "The total water consumption recorded by this meter around end of last month.",
             true, true);

    MeterCommonImplementation::bus()->onTelegram(calll(this,handleTelegram,Telegram*));
}

double MeterIzar::totalWaterConsumption(Unit u)
{
    assertQuantity(u, Quantity::Volume);
    return convert(total_water_consumption_l_, Unit::L, u);
}

bool MeterIzar::hasTotalWaterConsumption()
{
    return true;
}

double MeterIzar::lastMonthTotalWaterConsumption(Unit u)
{
    assertQuantity(u, Quantity::Volume);
    return convert(last_month_total_water_consumption_l_, Unit::L, u);
}

uint32_t MeterIzar::uint32FromBytes(const vector<uchar> &data, int offset, bool reverse)
{
    if (reverse)
        return ((uint32_t)data[offset + 3] << 24) |
            ((uint32_t)data[offset + 2] << 16) |
            ((uint32_t)data[offset + 1] << 8) |
            (uint32_t)data[offset];
    else
        return ((uint32_t)data[offset] << 24) |
            ((uint32_t)data[offset + 1] << 16) |
            ((uint32_t)data[offset + 2] << 8) |
            (uint32_t)data[offset + 3];
}

uint32_t MeterIzar::convertKey(const char *hex)
{
    vector<uchar> bytes;
    hex2bin(hex, &bytes);
    return convertKey(bytes);
}

uint32_t MeterIzar::convertKey(const vector<uchar> &bytes)
{
    uint32_t key1 = uint32FromBytes(bytes, 0);
    uint32_t key2 = uint32FromBytes(bytes, 4);
    uint32_t key = key1 ^ key2;
    return key;
}

void MeterIzar::processContent(Telegram *t)
{
    // recover full frame content
    vector<uchar> frame;
    frame.insert(frame.end(), t->parsed.begin(), t->parsed.end());
    frame.insert(frame.end(), t->payload.begin(), t->payload.end());

    vector<uchar> decoded_content;
    for (auto& key : keys) {
        decoded_content = decodePrios(frame, key);
        if (!decoded_content.empty())
            break;
    }

    debug("(izar) Decoded PRIOS data: %s\n", bin2hex(decoded_content).c_str());

    if (decoded_content.empty())
    {
        warning("(izar) Decoding PRIOS data failed. Ignoring telegram.\n");
        return;
    }

    total_water_consumption_l_ = uint32FromBytes(decoded_content, 1, true);
    last_month_total_water_consumption_l_ = uint32FromBytes(decoded_content, 5, true);
}

vector<uchar> MeterIzar::decodePrios(const vector<uchar> &frame, uint32_t key)
{
    // modify seed key with header values
    key ^= uint32FromBytes(frame, 2); // manufacturer + address[0-1]
    key ^= uint32FromBytes(frame, 6); // address[2-3] + version + type
    key ^= uint32FromBytes(frame, 10); // ci + some more bytes from the telegram...

    int size = frame.size() - 15;
    vector<uchar> decoded(size);

    for (int i = 0; i < size; ++i) {
        // calculate new key (LFSR)
        // https://en.wikipedia.org/wiki/Linear-feedback_shift_register
        for (int j = 0; j < 8; ++j) {
            // calculate new bit value (xor of selected bits from previous key)
            uchar bit = ((key & 0x2) != 0) ^ ((key & 0x4) != 0) ^ ((key & 0x800) != 0) ^ ((key & 0x80000000) != 0);
            // shift key bits and add new one at the end
            key = (key << 1) | bit;
        }
        // decode i-th content byte with fresh/last 8-bits of key
        decoded[i] = frame[i + 15] ^ (key & 0xFF);
        // check-byte doesn't match?
        if (decoded[0] != 0x4B) {
            decoded.clear();
            return decoded;
        }
    }

    return decoded;
}
