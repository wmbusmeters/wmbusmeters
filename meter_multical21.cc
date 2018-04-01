/*
 Copyright (C) 2017-2018 Fredrik Öhrström

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
#include<map>
#include<memory.h>
#include<stdio.h>
#include<string>
#include<time.h>
#include<vector>

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
    MeterMultical21(WMBus *bus, const char *name, const char *id, const char *key);

    // Total water counted through the meter
    float totalWaterConsumption();
    bool  hasTotalWaterConsumption();

    // Meter sends target water consumption or max flow, depending on meter configuration
    // We can see which was sent inside the wmbus message!
    // Target water consumption: The total consumption at the start of the previous 30 day period.
    float targetWaterConsumption();
    bool  hasTargetWaterConsumption();
    // Max flow during last month or last 24 hours depending on meter configuration.
    float maxFlow();
    bool  hasMaxFlow();

    // statusHumanReadable: DRY,REVERSED,LEAK,BURST if that status is detected right now, followed by
    //                      (dry 15-21 days) which means that, even it DRY is not active right now,
    //                      DRY has been active for 15-21 days during the last 30 days.
    string statusHumanReadable();
    string status();
    string timeDry();
    string timeReversed();
    string timeLeaking();
    string timeBursting();

    void printMeterHumanReadable(FILE *output);
    void printMeterFields(FILE *output, char separator);
    void printMeterJSON(FILE *output);

private:
    void handleTelegram(Telegram *t);
    void processContent(Telegram *t);
    string decodeTime(int time);

    uint16_t info_codes_ {};
    float total_water_consumption_ {};
    bool has_total_water_consumption_ {};
    float target_volume_ {};
    bool has_target_volume_ {};
    float max_flow_ {};
    bool has_max_flow_ {};
};

MeterMultical21::MeterMultical21(WMBus *bus, const char *name, const char *id, const char *key) :
    MeterCommonImplementation(bus, name, id, key, MULTICAL21_METER, MANUFACTURER_KAM, 0x16)
{
    MeterCommonImplementation::bus()->onTelegram(calll(this,handleTelegram,Telegram*));
}


float MeterMultical21::totalWaterConsumption()
{
    return total_water_consumption_;
}

bool MeterMultical21::hasTotalWaterConsumption()
{
    return has_total_water_consumption_;
}

float MeterMultical21::targetWaterConsumption()
{
    return target_volume_;
}

bool MeterMultical21::hasTargetWaterConsumption()
{
    return has_target_volume_;
}

float MeterMultical21::maxFlow()
{
    return max_flow_;
}

bool MeterMultical21::hasMaxFlow()
{
    return has_max_flow_;
}

WaterMeter *createMultical21(WMBus *bus, const char *name, const char *id, const char *key)
{
    return new MeterMultical21(bus,name,id,key);
}

void MeterMultical21::handleTelegram(Telegram *t)
{
    if (!isTelegramForMe(t)) {
        // This telegram is not intended for this meter.
        return;
    }

    verbose("(multical21) telegram for %s %02x%02x%02x%02x\n",
            name().c_str(),
            t->a_field_address[0], t->a_field_address[1], t->a_field_address[2],
            t->a_field_address[3]);

    if (t->a_field_device_type != 0x16) {
        warning("(multical21) expected telegram for water media, but got \"%s\"!\n",
                mediaType(t->m_field, t->a_field_device_type).c_str());
    }

    if (t->m_field != manufacturer() ||
        t->a_field_version != 0x1b) {
        warning("(multical21) expected telegram from KAM meter with version 0x1b, but got \"%s\" version 0x2x !\n",
                manufacturerFlag(t->m_field).c_str(), t->a_field_version);
    }

    if (useAes()) {
        vector<uchar> aeskey = key();
        decryptKamstrupC1(t, aeskey);
    } else {
        t->content = t->payload;
    }
    logTelegram("(multical21) log", t->parsed, t->content);
    int content_start = t->parsed.size();
    processContent(t);
    if (isDebugEnabled()) {
        t->explainParse("(multical21)", content_start);
    }
    triggerUpdate(t);
}

void MeterMultical21::processContent(Telegram *t)
{
    // Meter records:
    // 02 dif (16 Bit Integer/Binary Instantaneous value)
    // FF vif (vendor specific)
    // 20 vife (vendor specific)
    // xx xx (info codes)
    // 04 dif (32 Bit Integer/Binary Instantaneous value)
    // 13 vif (Volume l)
    // xx xx xx xx (total volume)
    // 44 dif (32 Bit Integer/Binary Instantaneous value KamstrupCombined)
    // 13 vif (Volume l)
    // xx xx (target volume in compact frame) but xx xx xx xx in full frame!

    vector<uchar>::iterator bytes = t->content.begin();

    int crc0 = t->content[0];
    int crc1 = t->content[1];
    t->addExplanation(bytes, 2, "%02x%02x payload crc", crc0, crc1);

    int frame_type = t->content[2];
    t->addExplanation(bytes, 1, "%02x frame type (%s)", frame_type, frameTypeKamstrupC1(frame_type).c_str());

    if (frame_type == 0x79)
    {
        if (t->content.size() != 15) {
            warning("(multical21) warning: Unexpected length of short frame %zu. Expected 15 bytes! ",
                    t->content.size());
            padWithZeroesTo(&t->content, 15, &t->content);
            warning("\n");
        }

        // 0,1 = crc for format signature = hash over DRH (Data Record Header)
        // The DRH is the dif(dife)vif(vife) bytes for all the records...
        // This hash should be used to pick up a suitable format string.
        // Below, DRH is hardcoded to 02FF2004134413
        int ecrc0 = t->content[3];
        int ecrc1 = t->content[4];

        // 2,3 = crc for payload = hash over both DRH and data bytes.
        int ecrc2 = t->content[5];
        int ecrc3 = t->content[6];

        // But since the crc calculation is not yet functional. This functionality
        // has to wait a bit.
        t->addExplanation(bytes, 4, "%02x%02x%02x%02x ecrc", ecrc0, ecrc1, ecrc2, ecrc3);

        // So we hardcode the format string here.
        vector<uchar> format_bytes;
        hex2bin("02FF2004134413", &format_bytes);
        vector<uchar>::iterator format = format_bytes.begin();

        map<string,pair<int,string>> values;
        parseDV(t, t->content.begin()+7, t->content.size()-7, &values, &format, format_bytes.size());

        int offset;

        extractDVuint16(&values, "02FF20", &offset, &info_codes_);
        t->addMoreExplanation(offset, " info codes (%s)", statusHumanReadable().c_str());

        extractDVfloat(&values, "0413", &offset, &total_water_consumption_);
        has_total_water_consumption_ = true;
        t->addMoreExplanation(offset, " total consumption (%f m3)", total_water_consumption_);

        extractDVfloatCombined(&values, "0413", "4413", &offset, &target_volume_);
	has_target_volume_ = true;
        t->addMoreExplanation(offset, " target consumption (%f m3)", target_volume_);
    }
    else
    if (frame_type == 0x78)
    {
        if (t->content.size() != 22) {
            warning("(multical21) warning: Unexpected length of long frame %zu. Expected 22 bytes! ", t->content.size());
            padWithZeroesTo(&t->content, 22, &t->content);
            warning("\n");
        }

        map<string,pair<int,string>> values;
        parseDV(t, t->content.begin()+3, t->content.size()-3-4, &values);

        // There are two more bytes in the data. Unknown purpose.
        int val0 = t->content[20];
        int val1 = t->content[21];

        int offset;

        extractDVuint16(&values, "02FF20", &offset, &info_codes_);
        t->addMoreExplanation(offset, " info codes (%s)", statusHumanReadable().c_str());

        extractDVfloat(&values, "0413", &offset, &total_water_consumption_);
        has_total_water_consumption_ = true;
        t->addMoreExplanation(offset, " total consumption (%f m3)", total_water_consumption_);

        extractDVfloat(&values, "4413", &offset, &target_volume_);
	has_target_volume_ = true;
        t->addMoreExplanation(offset, " target consumption (%f m3)", target_volume_);

        // To unknown bytes, seems to be very constant.
        vector<uchar>::iterator unknowns = t->content.begin()+20;
        t->addExplanation(unknowns, 2, "%02x%02x unknown", val0, val1);
    } else {
        warning("(multical21) warning: unknown frame %02x (did you use the correct encryption key?)\n", frame_type);
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
        warning("(multical21) warning: Cannot decode time %d should be 0-7.\n", time);
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

void MeterMultical21::printMeterHumanReadable(FILE *output)
{
    fprintf(output, "%s\t%s\t% 3.3f m3\t% 3.3f m3\t%s\t%s\n",
	    name().c_str(),
	    id().c_str(),
	    totalWaterConsumption(),
	    targetWaterConsumption(),
	    statusHumanReadable().c_str(),
	    datetimeOfUpdateHumanReadable().c_str());
}

void MeterMultical21::printMeterFields(FILE *output, char separator)
{
    fprintf(output, "%s%c%s%c%f%c%f%c%s%c%s\n",
	    name().c_str(), separator,
	    id().c_str(), separator,
	    totalWaterConsumption(), separator,
	    targetWaterConsumption(), separator,
	    statusHumanReadable().c_str(), separator,
            datetimeOfUpdateRobot().c_str());
}

#define Q(x,y) "\""#x"\":"#y","
#define QS(x,y) "\""#x"\":\""#y"\","
#define QSE(x,y) "\""#x"\":\""#y"\""

void MeterMultical21::printMeterJSON(FILE *output)
{
    fprintf(output, "{media:\"%s\",meter:\"multical21\","
	    QS(name,%s)
	    QS(id,%s)
	    Q(total_m3,%f)
	    Q(target_m3,%f)
	    QS(current_status,%s)
	    QS(time_dry,%s)
	    QS(time_reversed,%s)
	    QS(time_leaking,%s)
	    QS(time_bursting,%s)
	    QSE(timestamp,%s)
	    "}\n",
            mediaType(manufacturer(), media()).c_str(),
	    name().c_str(),
	    id().c_str(),
	    totalWaterConsumption(),
	    targetWaterConsumption(),
	    status().c_str(), // DRY REVERSED LEAK BURST
	    timeDry().c_str(),
	    timeReversed().c_str(),
	    timeLeaking().c_str(),
	    timeBursting().c_str(),
	    datetimeOfUpdateRobot().c_str());
}
