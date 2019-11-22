/*
 Copyright (C) 2017-2019 Fredrik Öhrström

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

#define INFO_CODE_DRY 0x01
#define INFO_CODE_DRY_SHIFT (4+0)

#define INFO_CODE_REVERSE 0x02
#define INFO_CODE_REVERSE_SHIFT (4+3)

#define INFO_CODE_LEAK 0x04
#define INFO_CODE_LEAK_SHIFT (4+6)

#define INFO_CODE_BURST 0x08
#define INFO_CODE_BURST_SHIFT (4+9)

struct MeterMultical21 : public virtual WaterMeter, public virtual MeterCommonImplementation {
    MeterMultical21(WMBus *bus, MeterInfo &mi, MeterType mt);

    // Total water counted through the meter
    double totalWaterConsumption(Unit u);
    bool  hasTotalWaterConsumption();

    // Meter sends target water consumption or max flow, depending on meter configuration
    // We can see which was sent inside the wmbus message!

    // Target water consumption: The total consumption at the start of the previous 30 day period.
    double targetWaterConsumption(Unit u);
    bool  hasTargetWaterConsumption();
    // Max flow during last month or last 24 hours depending on meter configuration.
    double maxFlow(Unit u);
    bool  hasMaxFlow();
    // Water temperature
    double flowTemperature(Unit u);
    bool  hasFlowTemperature();
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
    double max_flow_m3h_ {};
    bool has_max_flow_ {};
    double flow_temperature_c_ { 127 };
    bool has_flow_temperature_ {};
    double external_temperature_c_ { 127 };
    bool has_external_temperature_ {};

    int expected_version_ {}; // 0x1b for Multical21 and 0x1d for FlowIQ3100
};

MeterMultical21::MeterMultical21(WMBus *bus, MeterInfo &mi, MeterType mt) :
    MeterCommonImplementation(bus, mi, mt, MANUFACTURER_KAM)
{
    setEncryptionMode(EncryptionMode::AES_CTR);

    addMedia(0x16); // Water media

    addLinkMode(LinkMode::C1);

    if (type() == MeterType::MULTICAL21) {
        setExpectedVersion(0x1b);
    } else if (type() == MeterType::FLOWIQ3100) {
        setExpectedVersion(0x1d);
    } else {
        assert(0);
    }

    addPrint("total", Quantity::Volume,
             [&](Unit u){ return totalWaterConsumption(u); },
             "The total water consumption recorded by this meter.",
             true, true);

    addPrint("target", Quantity::Volume,
             [&](Unit u){ return targetWaterConsumption(u); },
             "The total water consumption recorded at the beginning of this month.",
             true, true);

    addPrint("max_flow", Quantity::Flow,
             [&](Unit u){ return maxFlow(u); },
             "The maxium flow recorded during previous period.",
             true, true);

    addPrint("flow_temperature", Quantity::Temperature,
             [&](Unit u){ return flowTemperature(u); },
             "The water temperature.",
             true, true);

    addPrint("external_temperature", Quantity::Temperature,
             [&](Unit u){ return externalTemperature(u); },
             "The external temperature outside of the meter.",
             true, true);

    addPrint("", Quantity::Text,
             [&](){ return statusHumanReadable(); },
             "Status of meter.",
             true, false);

    addPrint("current_status", Quantity::Text,
             [&](){ return status(); },
             "Status of meter.",
             false, true);

    addPrint("time_dry", Quantity::Text,
             [&](){ return timeDry(); },
             "Amount of time the meter has been dry.",
             false, true);

    addPrint("time_reversed", Quantity::Text,
             [&](){ return timeReversed(); },
             "Amount of time the meter has been reversed.",
             false, true);

    addPrint("time_leaking", Quantity::Text,
             [&](){ return timeLeaking(); },
             "Amount of time the meter has been leaking.",
             false, true);

    addPrint("time_bursting", Quantity::Text,
             [&](){ return timeBursting(); },
             "Amount of time the meter has been bursting.",
             false, true);

    MeterCommonImplementation::bus()->onTelegram(calll(this,handleTelegram,Telegram*));
}

double MeterMultical21::totalWaterConsumption(Unit u)
{
    assertQuantity(u, Quantity::Volume);
    return convert(total_water_consumption_m3_, Unit::M3, u);
}

bool MeterMultical21::hasTotalWaterConsumption()
{
    return has_total_water_consumption_;
}

double MeterMultical21::targetWaterConsumption(Unit u)
{
    assertQuantity(u, Quantity::Volume);
    return convert(target_water_consumption_m3_, Unit::M3, u);
}

bool MeterMultical21::hasTargetWaterConsumption()
{
    return has_target_water_consumption_;
}

double MeterMultical21::maxFlow(Unit u)
{
    assertQuantity(u, Quantity::Flow);
    return convert(max_flow_m3h_, Unit::M3H, u);
}

bool MeterMultical21::hasMaxFlow()
{
    return has_max_flow_;
}

double MeterMultical21::flowTemperature(Unit u)
{
    assertQuantity(u, Quantity::Temperature);
    return convert(flow_temperature_c_, Unit::C, u);
}

bool MeterMultical21::hasFlowTemperature()
{
    return has_flow_temperature_;
}

double MeterMultical21::externalTemperature(Unit u)
{
    assertQuantity(u, Quantity::Temperature);
    return convert(external_temperature_c_, Unit::C, u);
}

bool MeterMultical21::hasExternalTemperature()
{
    return has_external_temperature_;
}

unique_ptr<WaterMeter> createMulticalWaterMeter(WMBus *bus, MeterInfo &mi, MeterType mt)
{
    if (mt != MeterType::MULTICAL21 && mt != MeterType::FLOWIQ3100) {
        error("Internal error! Not a proper meter type when creating a multical21 style meter.\n");
    }
    return unique_ptr<WaterMeter>(new MeterMultical21(bus,mi,mt));
}

unique_ptr<WaterMeter> createMultical21(WMBus *bus, MeterInfo &mi)
{
    return createMulticalWaterMeter(bus, mi, MeterType::MULTICAL21);
}

unique_ptr<WaterMeter> createFlowIQ3100(WMBus *bus, MeterInfo &mi)
{
    return createMulticalWaterMeter(bus, mi, MeterType::FLOWIQ3100);
}

void MeterMultical21::processContent(Telegram *t)
{
    // 02 dif (16 Bit Integer/Binary Instantaneous value)
    // FF vif (Kamstrup extension)
    // 20 vife (?)
    // 7100 info codes (DRY(dry 22-31 days))
    // 04 dif (32 Bit Integer/Binary Instantaneous value)
    // 13 vif (Volume l)
    // F8180000 total consumption (6.392000 m3)
    // 44 dif (32 Bit Integer/Binary Instantaneous value storagenr=1)
    // 13 vif (Volume l)
    // F4180000 target consumption (6.388000 m3)
    // 61 dif (8 Bit Integer/Binary Minimum value storagenr=1)
    // 5B vif (Flow temperature °C)
    // 7F flow temperature (127.000000 °C)
    // 61 dif (8 Bit Integer/Binary Minimum value storagenr=1)
    // 67 vif (External temperature °C)
    // 17 external temperature (23.000000 °C)

    // 02 dif (16 Bit Integer/Binary Instantaneous value)
    // FF vif (Kamstrup extension)
    // 20 vife (?)
    // 0000 info codes (OK)
    // 04 dif (32 Bit Integer/Binary Instantaneous value)
    // 13 vif (Volume l)
    // 2F4E0000 total consumption (20.015000 m3)
    // 92 dif (16 Bit Integer/Binary Maximum value)
    // 01 dife (subunit=0 tariff=0 storagenr=2)
    // 3B vif (Volume flow l/h)
    // 3D01 max flow (0.317000 m3/h)
    // A1 dif (8 Bit Integer/Binary Minimum value)
    // 01 dife (subunit=0 tariff=0 storagenr=2)
    // 5B vif (Flow temperature °C)
    // 02 flow temperature (2.000000 °C)
    // 81 dif (8 Bit Integer/Binary Instantaneous value)
    // 01 dife (subunit=0 tariff=0 storagenr=2)
    // E7 vif (External temperature °C)
    // FF vife (?)
    // 0F vife (?)
    // 03 external temperature (3.000000 °C)

    // 14: 02 dif (16 Bit Integer/Binary Instantaneous value)
    // 15: FF vif (Vendor extension)
    // 16: 20 vife (per second)
    // 17: * 0008 info codes ((leak 9-24 hours))
    // 19: 04 dif (32 Bit Integer/Binary Instantaneous value)
    // 1a: 13 vif (Volume l)
    // 1b: * 1F090100 total consumption (67.871000 m3)
    // 1f: 44 dif (32 Bit Integer/Binary Instantaneous value storagenr=1)
    // 20: 13 vif (Volume l)
    // 21: * EBF00000 target consumption (61.675000 m3)
    // 25: A1 dif (8 Bit Integer/Binary Minimum value)
    // 26: 01 dife (subunit=0 tariff=0 storagenr=2)
    // 27: 5B vif (Flow temperature °C)
    // 28: * 09 flow temperature (9.000000 °C)
    // 29: 81 dif (8 Bit Integer/Binary Instantaneous value)
    // 2a: 01 dife (subunit=0 tariff=0 storagenr=2)
    // 2b: E7 vif (External temperature °C)
    // 2c: FF vife (additive correction constant: unit of VIF * 10^0)
    // 2d: 0F vife (?)
    // 2e: * 0D external temperature (13.000000 °C)

    string meter_name = toMeterName(type()).c_str();
    vector<uchar>::iterator bytes = t->content.begin();

    int crc0 = t->content[0];
    int crc1 = t->content[1];
    t->addExplanation(bytes, 2, "%02x%02x payload crc", crc0, crc1);

    int frame_type = t->content[2];
    t->addExplanation(bytes, 1, "%02x frame type (%s)", frame_type, frameTypeKamstrupC1(frame_type).c_str());

    map<string,pair<int,DVEntry>> values;

    if (frame_type == 0x79)
    {
        // This is a "compact frame" in wmbus lingo.
        // (Other such frame_types are Ci=0x69, 0x6a, 0x6b and Ci=0x79, 0x7b, compact frames and format frames)
        // 0,1 = crc for format signature = hash over DRH (Data Record Header)
        // The DRH is the dif(difes)vif(vifes) bytes for all the records...
        // This hash is used to find the suitable format string, that has been previously
        // seen in a long frame telegram.
        uchar ecrc0 = t->content[3];
        uchar ecrc1 = t->content[4];
        t->addExplanation(bytes, 2, "%02x%02x format signature", ecrc0, ecrc1);
        uint16_t format_signature = ecrc1<<8 | ecrc0;

        vector<uchar> format_bytes;
        bool ok = loadFormatBytesFromSignature(format_signature, &format_bytes);
        if (!ok) {
            // We have not yet seen a long frame, but we know the formats for these
            // particular hashes:
            if (format_signature == 0xa8ed)
            {
                hex2bin("02FF2004134413615B6167", &format_bytes);
                debug("(%s) using hard coded format for hash a8ed\n", meter_name.c_str());
            }
            else if (format_signature == 0xc412)
            {
                hex2bin("02FF20041392013BA1015B8101E7FF0F", &format_bytes);
                debug("(%s) using hard coded format for hash c412\n", meter_name.c_str());
            }
            else if (format_signature == 0x61eb)
            {
                hex2bin("02FF2004134413A1015B8101E7FF0F", &format_bytes);
                debug("(%s) using hard coded format for hash 61eb\n", meter_name.c_str());
            }
            else if (format_signature == 0xd2f7)
            {
                hex2bin("02FF2004134413615B5167", &format_bytes);
                debug("(%s) using hard coded format for hash d2f7\n", meter_name.c_str());
            }
            else
            {
                verbose("(%s) ignoring telegram since format signature hash 0x%02x is yet unknown.\n",
                        meter_name.c_str(),  format_signature);
                return;
            }
        }
        vector<uchar>::iterator format = format_bytes.begin();

        // 2,3 = crc for payload = hash over both DRH and data bytes. Or is it only over the data bytes?
        int ecrc2 = t->content[5];
        int ecrc3 = t->content[6];
        t->addExplanation(bytes, 2, "%02x%02x data crc", ecrc2, ecrc3);
        parseDV(t, t->content, t->content.begin()+7, t->content.size()-7, &values, &format, format_bytes.size());
    }
    else
    if (frame_type == 0x78)
    {
        parseDV(t, t->content, t->content.begin()+3, t->content.size()-3, &values);
    }
    else
    {
        warning("(%s) warning: unknown frame %02x (did you use the correct encryption key?)\n", meter_name.c_str(), frame_type);
        return;
    }

    int offset;
    string key;

    extractDVuint16(&values, "02FF20", &offset, &info_codes_);
    t->addMoreExplanation(offset, " info codes (%s)", statusHumanReadable().c_str());

    if(findKey(MeasurementType::Instantaneous, ValueInformation::Volume, 0, &key, &values)) {
        extractDVdouble(&values, key, &offset, &total_water_consumption_m3_);
        has_total_water_consumption_ = true;
        t->addMoreExplanation(offset, " total consumption (%f m3)", total_water_consumption_m3_);
    }

    if(findKey(MeasurementType::Unknown, ValueInformation::Volume, 1, &key, &values)) {
        extractDVdouble(&values, key, &offset, &target_water_consumption_m3_);
        has_target_water_consumption_ = true;
        t->addMoreExplanation(offset, " target consumption (%f m3)", target_water_consumption_m3_);
    }

    if(findKey(MeasurementType::Unknown, ValueInformation::VolumeFlow, ANY_STORAGENR, &key, &values)) {
        extractDVdouble(&values, key, &offset, &max_flow_m3h_);
        has_max_flow_ = true;
        t->addMoreExplanation(offset, " max flow (%f m3/h)", max_flow_m3h_);
    }

    if(findKey(MeasurementType::Unknown, ValueInformation::FlowTemperature, ANY_STORAGENR, &key, &values)) {
        has_flow_temperature_ = extractDVdouble(&values, key, &offset, &flow_temperature_c_);
        t->addMoreExplanation(offset, " flow temperature (%f °C)", flow_temperature_c_);
    }

    if(findKey(MeasurementType::Unknown, ValueInformation::ExternalTemperature, ANY_STORAGENR, &key, &values)) {
        has_external_temperature_ = extractDVdouble(&values, key, &offset, &external_temperature_c_);
        t->addMoreExplanation(offset, " external temperature (%f °C)", external_temperature_c_);
    }

}

string MeterMultical21::status()
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

string MeterMultical21::timeDry()
{
    int time_dry = (info_codes_ >> INFO_CODE_DRY_SHIFT) & 7;
    if (time_dry) {
        return decodeTime(time_dry);
    }
    return "";
}

string MeterMultical21::timeReversed()
{
    int time_reversed = (info_codes_ >> INFO_CODE_REVERSE_SHIFT) & 7;
    if (time_reversed) {
        return decodeTime(time_reversed);
    }
    return "";
}

string MeterMultical21::timeLeaking()
{
    int time_leaking = (info_codes_ >> INFO_CODE_LEAK_SHIFT) & 7;
    if (time_leaking) {
        return decodeTime(time_leaking);
    }
    return "";
}

string MeterMultical21::timeBursting()
{
    int time_bursting = (info_codes_ >> INFO_CODE_BURST_SHIFT) & 7;
    if (time_bursting) {
        return decodeTime(time_bursting);
    }
    return "";
}

string MeterMultical21::statusHumanReadable()
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

string MeterMultical21::decodeTime(int time)
{
    if (time>7) {
        string meter_name = toMeterName(type()).c_str();
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
