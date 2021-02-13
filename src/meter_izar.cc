/*
 Copyright (C) 2019 Jacek Tomasiak
               2020 Fredrik Öhrström
               2021 Vincent Privat

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
#include"manufacturer_specificities.h"

#include<algorithm>
#include<stdbool.h>

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
    MeterIzar(MeterInfo &mi);

    // Total water counted through the meter
    double totalWaterConsumption(Unit u);
    bool  hasTotalWaterConsumption();

    string serialNumber();
    double lastMonthTotalWaterConsumption(Unit u);
    string setH0Date();
    string currentAlarmsText();
    string previousAlarmsText();

private:

    void processContent(Telegram *t);
    vector<uchar> decodePrios(const vector<uchar> &origin, const vector<uchar> &payload, uint32_t key);

    string prefix;
    uint32_t serial_number {0};
    double remaining_battery_life;
    uint16_t h0_year;
    uint8_t h0_month;
    uint8_t h0_day;
    double total_water_consumption_l_ {};
    double last_month_total_water_consumption_l_ {};
    uint32_t transmit_period_s_ {};
    uint16_t manufacture_year {0};
    izar_alarms alarms;

    vector<uint32_t> keys;
};

shared_ptr<WaterMeter> createIzar(MeterInfo &mi)
{
    return shared_ptr<WaterMeter>(new MeterIzar(mi));
}

MeterIzar::MeterIzar(MeterInfo &mi) :
    MeterCommonImplementation(mi, MeterType::IZAR)
{
    initializeDiehlDefaultKeySupport(meterKeys()->confidentiality_key, keys);
    addLinkMode(LinkMode::T1);

    addPrint("prefix", Quantity::Text,
             [&](){ return prefix; },
             "The alphanumeric prefix printed before serial number on device.",
             true, true);

    addPrint("serial_number", Quantity::Text,
             [&](){ return serialNumber(); },
             "The meter serial number.",
             true, true);

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
             "Alarms previously reported by the meter.",
             true, true);

    addPrint("transmit_period", Quantity::Time, Unit::Second,
             [&](Unit u){ return convert(transmit_period_s_, Unit::Second, u); },
             "The period at which the meter transmits its data.",
             true, true);

    addPrint("manufacture_year", Quantity::Text,
             [&](){ return to_string(manufacture_year); },
             "The year during which the meter was manufactured.",
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
    strprintf(date, "%d-%02d-%02d", h0_year, h0_month%99, h0_day%99);
    return date;
}

string MeterIzar::serialNumber()
{
    string result;
    strprintf(result, "%06d", serial_number);
    return result;
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

void MeterIzar::processContent(Telegram *t)
{
    vector<uchar> frame;
    t->extractFrame(&frame);
    vector<uchar> origin = t->original.empty() ? frame : t->original;

    vector<uchar> decoded_content;
    for (auto& key : keys) {
        decoded_content = decodePrios(origin, frame, key);
        if (!decoded_content.empty())
            break;
    }

    debug("(izar) Decoded PRIOS data: %s\n", bin2hex(decoded_content).c_str());

    if (decoded_content.empty())
    {
        warning("(izar) Decoding PRIOS data failed. Ignoring telegram.\n");
        return;
    }

    if (detectDiehlFrameInterpretation(frame) == DiehlFrameInterpretation::SAP_PRIOS)
    {
        string digits = to_string((origin[7] >> 5) << 24 | origin[6] << 16 | origin[5] << 8 | origin[4]);
        // get the manufacture year
        uint8_t yy = atoi(digits.substr(0, 2).c_str());
        manufacture_year = yy > 70 ? (1900 + yy) : (2000 + yy); // Maybe to adjust in 2070, if this code stills lives :D
        // get the serial number
        serial_number = atoi(digits.substr(3, digits.size()).c_str());
        // get letters
        uchar supplier_code = '@' + (((origin[9] & 0x0F) << 1) | (origin[8] >> 7));
        uchar meter_type = '@' + ((origin[8] & 0x7C) >> 2);
        uchar diameter = '@' + (((origin[8] & 0x03) << 3) | (origin[7] >> 5));
        // build the prefix
        strprintf(prefix, "%c%02d%c%c", supplier_code, yy, meter_type, diameter);
    }

    // get the remaining battery life (in year) and transmission period (in seconds)
    remaining_battery_life = (frame[12] & 0x1F) / 2.0;
    transmit_period_s_ = 1 << ((frame[11] & 0x0F) + 2);

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
}

vector<uchar> MeterIzar::decodePrios(const vector<uchar> &origin, const vector<uchar> &frame, uint32_t key)
{
    return decodeDiehlLfsr(origin, frame, key, DiehlLfsrCheckMethod::HEADER_1_BYTE, 0x4B);
}
