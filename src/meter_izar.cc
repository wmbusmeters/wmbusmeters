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

#include<algorithm>
#include<stdbool.h>

using namespace std;

#define PRIOS_DEFAULT_KEY1 "39BC8A10E66D83F8"
#define PRIOS_DEFAULT_KEY2 "51728910E66D83F8"

/** Contains all the booleans required to store the alarms of a PRIOS device. */
typedef struct _izar_alarms {
    bool general_alarm;
    bool leakage_currently;
    bool leakage_previously;
    bool meter_blocked;
    bool back_flow;
    bool underflow;
    bool overflow;
    bool submarine;
    bool sensor_fraud_currently;
    bool sensor_fraud_previously;
    bool mechanical_fraud_currently;
    bool mechanical_fraud_previously;
} izar_alarms;

struct MeterIzar : public virtual WaterMeter, public virtual MeterCommonImplementation {
    MeterIzar(WMBus *bus, MeterInfo &mi);

    // Total water counted through the meter
    double totalWaterConsumption(Unit u);
    bool  hasTotalWaterConsumption();

    double lastMonthTotalWaterConsumption(Unit u);
    string setH0Date();
    string currentAlarmsText();
    string previousAlarmsText();

private:

    void processContent(Telegram *t);
    uint32_t convertKey(const char *hex);
    uint32_t convertKey(const vector<uchar> &bytes);
    uint32_t uint32FromBytes(const vector<uchar> &data, int offset, bool reverse = false);
    vector<uchar> decodePrios(const vector<uchar> &payload, uint32_t key);

    double remaining_battery_life;
    uint16_t h0_year;
    uint8_t h0_month;
    uint8_t h0_day;
    double total_water_consumption_l_ {};
    double last_month_total_water_consumption_l_ {};
    izar_alarms alarms;

    vector<uint32_t> keys;
};

unique_ptr<WaterMeter> createIzar(WMBus *bus, MeterInfo &mi)
{
    return unique_ptr<WaterMeter>(new MeterIzar(bus, mi));
}

MeterIzar::MeterIzar(WMBus *bus, MeterInfo &mi) :
    MeterCommonImplementation(bus, mi, MeterType::IZAR, MANUFACTURER_SAP)
{
    addManufacturer(MANUFACTURER_DME);

    MeterKeys *mk = meterKeys();
    if (!mk->confidentiality_key.empty())
        keys.push_back(convertKey(mk->confidentiality_key));

    // fallback to default keys if no custom key provided
    if (keys.empty())
    {
        keys.push_back(convertKey(PRIOS_DEFAULT_KEY1));
        keys.push_back(convertKey(PRIOS_DEFAULT_KEY2));
    }

    addMedia(0x01); // Oil meter? why?
    addMedia(0x15); // Hot water
    addMedia(0x66); // Woot?

    addLinkMode(LinkMode::T1);

    // Meters with different versions exist, don't set any to avoid warnings

    addPrint("total", Quantity::Volume,
             [&](Unit u){ return totalWaterConsumption(u); },
             "The total water consumption recorded by this meter.",
             true, true);

    addPrint("last_month_total", Quantity::Volume,
             [&](Unit u){ return lastMonthTotalWaterConsumption(u); },
             "The total water consumption recorded by this meter around end of last month.",
             true, true);

    addPrint("last_month_measure_date", Quantity::Text,
             [&](){ return setH0Date(); },
             "The date when the meter recorded the most recent billing value.",
             true, true);

    addPrint("remaining_battery_life", Quantity::Time, Unit::Year,
             [&](Unit u){ return convert(remaining_battery_life, Unit::Year, u); },
             "How many more years the battery is expected to last",
             true, true);

    addPrint("current_alarms", Quantity::Text,
             [&](){ return currentAlarmsText(); },
             "Alarms currently reported by the meter.",
             true, true);

    addPrint("previous_alarms", Quantity::Text,
             [&](){ return previousAlarmsText(); },
             "Alarms currently reported by the meter.",
             true, true);

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

string MeterIzar::setH0Date()
{
    string date;
    strprintf(date, "%04d-%02d-%02d", h0_year, h0_month, h0_day);
    return date;
}

string MeterIzar::currentAlarmsText()
{
    string s;
    if (alarms.leakage_currently) {
        s.append("leakage,");
    }
    if (alarms.meter_blocked) {
        s.append("meter_blocked,");
    }
    if (alarms.back_flow) {
        s.append("back_flow,");
    }
    if (alarms.underflow) {
        s.append("underflow,");
    }
    if (alarms.overflow) {
        s.append("overflow,");
    }
    if (alarms.submarine) {
        s.append("submarine,");
    }
    if (alarms.sensor_fraud_currently) {
        s.append("sensor_fraud,");
    }
    if (alarms.mechanical_fraud_currently) {
        s.append("mechanical_fraud,");
    }
    if (s.length() > 0) {
        if (alarms.general_alarm) {
            return "general_alarm";
        }
        s.pop_back();
        return s;
    }
    return "no_alarm";
}

string MeterIzar::previousAlarmsText()
{
    string s;
    if (alarms.leakage_previously) {
        s.append("leakage,");
    }
    if (alarms.sensor_fraud_previously) {
        s.append("sensor_fraud,");
    }
    if (alarms.mechanical_fraud_previously) {
        s.append("mechanical_fraud,");
    }
    if (s.length() > 0) {
        s.pop_back();
        return s;
    }
    return "no_alarm";
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
    vector<uchar> frame;
    t->extractFrame(&frame);

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

    // get the remaining battery life (in year)
    remaining_battery_life = (frame[12] & 0x1F) / 2.0;

    total_water_consumption_l_ = uint32FromBytes(decoded_content, 1, true);
    last_month_total_water_consumption_l_ = uint32FromBytes(decoded_content, 5, true);

    // get the date when the second measurement was taken
    h0_year = ((decoded_content[10] & 0xF0) >> 1) + ((decoded_content[9] & 0xE0) >> 5);
    if (h0_year > 80) {
        h0_year += 1900;
    } else {
        h0_year += 2000;
    }
    h0_month = decoded_content[10] & 0xF;
    h0_day = decoded_content[9] & 0x1F;

    // read the alarms:
    alarms.general_alarm = frame[11] >> 7;
    alarms.leakage_currently = frame[12] >> 7;
    alarms.leakage_previously = frame[12] >> 6 & 0x1;
    alarms.meter_blocked = frame[12] >> 5 & 0x1;
    alarms.back_flow = frame[13] >> 7;
    alarms.underflow = frame[13] >> 6 & 0x1;
    alarms.overflow = frame[13] >> 5 & 0x1;
    alarms.submarine = frame[13] >> 4 & 0x1;
    alarms.sensor_fraud_currently = frame[13] >> 3 & 0x1;
    alarms.sensor_fraud_previously = frame[13] >> 2 & 0x1;
    alarms.mechanical_fraud_currently = frame[13] >> 1 & 0x1;
    alarms.mechanical_fraud_previously = frame[13] & 0x1;

    // override incorrectly reported medium (oil)
    t->dll_type = 7;
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
