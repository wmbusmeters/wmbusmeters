// Copyright (c) 2017 Fredrik Öhrström
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include"meters.h"
#include"meters_common_implementation.h"
#include"wmbus.h"
#include"wmbus_utils.h"
#include"util.h"

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

    int info_codes_ {};
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

WaterMeter *createMultical21(WMBus *bus, const char *name, const char *id, const char *key) {
    return new MeterMultical21(bus,name,id,key);
}

void MeterMultical21::handleTelegram(Telegram *t) {

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

float getScaleFactor(int vif) {
    switch (vif) {
    case 0x13: return 1000.0;
    case 0x14: return 100.0;
    case 0x15: return 10.0;
    case 0x16: return 10.0;
    }
    warning("(multical21) warning: Unknown vif code %d for scale factor, using 1000.0 instead.\n", vif);
    return 1000.0;
}

void MeterMultical21::processContent(Telegram *t) {
    vector<uchar> full_content;
    full_content.insert(full_content.end(), t->parsed.begin(), t->parsed.end());
    full_content.insert(full_content.end(), t->content.begin(), t->content.end());

    int crc0 = t->content[0];
    int crc1 = t->content[1];
    t->addExplanation(full_content, 2, "%02x%02x plcrc", crc0, crc1);
    int frame_type = t->content[2];
    t->addExplanation(full_content, 1, "%02x frame type (%s)", frame_type, frameTypeKamstrupC1(frame_type).c_str());

    if (frame_type == 0x79) {
        if (t->content.size() != 15) {
            warning("(multical21) warning: Unexpected length of short frame %zu. Expected 15 bytes! ",
                    t->content.size());
            padWithZeroesTo(&t->content, 15, &full_content);
            warning("\n");
        }
        int ecrc0 = t->content[3];
        int ecrc1 = t->content[4];
        int ecrc2 = t->content[5];
        int ecrc3 = t->content[6];
        t->addExplanation(full_content, 4, "%02x%02x%02x%02x ecrc", ecrc0, ecrc1, ecrc2, ecrc3);
        int rec1val0 = t->content[7];
        int rec1val1 = t->content[8];

        int rec2val0 = t->content[9];
        int rec2val1 = t->content[10];
        int rec2val2 = t->content[11];

        int rec2val3 = t->content[12];
        int rec3val0 = t->content[13];
        int rec3val1 = t->content[14];

        info_codes_ = rec1val1*256+rec1val0;
        t->addExplanation(full_content, 2, "%02x%02x info codes (%s)", rec1val0, rec1val1, statusHumanReadable().c_str());

        int consumption_raw = rec2val3*256*256*256 + rec2val2*256*256 + rec2val1*256 + rec2val0;
        // The dif=0x04 vif=0x13 means current volume with scale factor .001
        total_water_consumption_ = ((float)consumption_raw) / ((float)1000);
        t->addExplanation(full_content, 4, "%02x%02x%02x%02x total consumption (%d)",
                       rec2val0, rec2val1, rec2val2, rec2val3, consumption_raw);
	has_total_water_consumption_ = true;

        // The short frame target volume supplies two low bytes,
        // the remaining two hi bytes are >>probably<< picked from rec2!
        int target_volume_raw = rec2val3*256*256*256 + rec2val2*256*256 + rec3val1*256 + rec3val0;
        target_volume_ = ((float)target_volume_raw) / ((float)1000);
        t->addExplanation(full_content, 2, "%02x%02x target volume (%d)",
                       rec3val0, rec3val1, target_volume_raw);
	has_target_volume_ = true;
    } else
    if (frame_type == 0x78) {
        if (t->content.size() != 22) {
            warning("(multical21) warning: Unexpected length of long frame %zu. Expected 22 bytes! ", t->content.size());
            padWithZeroesTo(&t->content, 22, &full_content);
            warning("\n");
        }
        int rec1dif = t->content[3];
        int rec1vif = t->content[4];
        int rec1vife = t->content[5];

        int rec1val0 = t->content[6];
        int rec1val1 = t->content[7];

        int rec2dif = t->content[8];
        int rec2vif = t->content[9];
        int rec2val0 = t->content[10];
        int rec2val1 = t->content[11];
        int rec2val2 = t->content[12];
        int rec2val3 = t->content[13];

        int rec3dif = t->content[14];
        int rec3vif = t->content[15];
        int rec3val0 = t->content[16];
        int rec3val1 = t->content[17];
        int rec3val2 = t->content[18];
        int rec3val3 = t->content[19];

        // There are two more bytes in the data. Unknown purpose.
        int rec4val0 = t->content[20];
        int rec4val1 = t->content[21];

        if (rec1dif != 0x02 || rec1vif != 0xff || rec1vife != 0x20 ) {
            warning("(multical21) warning: Unexpected field! Expected info codes\n"
                    "with dif=0x02 vif=0xff vife=0x20 but got dif=%02x vif=%02x vife=%02x\n", rec1dif, rec1vif, rec1vife);
        }
        t->addExplanation(full_content, 1, "%02x dif (%s)", rec1dif, difType(rec1dif).c_str());
        t->addExplanation(full_content, 1, "%02x vif (%s)", rec1vif, vifType(rec1vif).c_str());
        t->addExplanation(full_content, 1, "%02x vife (%s)", rec1vife, vifeType(rec1vif, rec1vife).c_str());

        info_codes_ = rec1val1*256+rec1val0;
        t->addExplanation(full_content, 2, "%02x%02x info codes (%s)", rec1val1, rec1val0, statusHumanReadable().c_str());

        if (rec2dif != 0x04 || rec2vif != 0x13) {
            warning("(multical21) warning: Unexpected field! Expected current volume\n"
                    "with dif=0x04 vif=0x13 but got dif=%02x vif=%02x\n", rec2dif, rec2vif);
        }
        t->addExplanation(full_content, 1, "%02x dif (%s)", rec2dif, difType(rec2dif).c_str());
        t->addExplanation(full_content, 1, "%02x vif (%s)", rec2vif, vifType(rec2vif).c_str());

        int consumption_raw = rec2val3*256*256*256 + rec2val2*256*256 + rec2val1*256 + rec2val0;
        // The dif=0x04 vif=0x13 means current volume with scale factor .001
        total_water_consumption_ = ((float)consumption_raw) / ((float)1000);
	has_total_water_consumption_ = true;
        t->addExplanation(full_content, 4, "%02x%02x%02x%02x total consumption (%d)",
                       rec2val3, rec2val2, rec2val1, rec2val0, consumption_raw);

        if (rec3dif != 0x44 || rec3vif != 0x13) {
            warning("(multical21) warning: Unexpected field! Expected target volume (ie volume recorded on first day of month)\n"
                    "with dif=0x44 vif=0x13 but got dif=%02x vif=%02x\n", rec3dif, rec3vif);
        }
        t->addExplanation(full_content, 1, "%02x dif (%s)", rec3dif, difType(rec3dif).c_str());
        t->addExplanation(full_content, 1, "%02x vif (%s)", rec3vif, vifType(rec3vif).c_str());

        int target_volume_raw = rec3val3*256*256*256 + rec3val2*256*256 + rec3val1*256 + rec3val0;
        target_volume_ = ((float)target_volume_raw) / ((float)1000);
	has_target_volume_ = true;

        t->addExplanation(full_content, 4, "%02x%02x%02x%02x target consumption (%d)",
                       rec3val3, rec3val2, rec3val1, rec3val0, target_volume_raw);

        // To unknown bytes, seems to be very constant.
        t->addExplanation(full_content, 2, "%02x%02x unknown", rec4val0, rec4val1);
    } else {
        warning("(multical21) warning: unknown frame %02x (did you use the correct encryption key?)\n", frame_type);
    }
}

string MeterMultical21::status() {
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

string MeterMultical21::timeDry() {
    int time_dry = (info_codes_ >> INFO_CODE_DRY_SHIFT) & 7;
    if (time_dry) {
        return decodeTime(time_dry);
    }
    return "";
}

string MeterMultical21::timeReversed() {
    int time_reversed = (info_codes_ >> INFO_CODE_REVERSE_SHIFT) & 7;
    if (time_reversed) {
        return decodeTime(time_reversed);
    }
    return "";
}

string MeterMultical21::timeLeaking() {
    int time_leaking = (info_codes_ >> INFO_CODE_LEAK_SHIFT) & 7;
    if (time_leaking) {
        return decodeTime(time_leaking);
    }
    return "";
}

string MeterMultical21::timeBursting() {
    int time_bursting = (info_codes_ >> INFO_CODE_BURST_SHIFT) & 7;
    if (time_bursting) {
        return decodeTime(time_bursting);
    }
    return "";
}

string MeterMultical21::statusHumanReadable() {
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

string MeterMultical21::decodeTime(int time) {
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
    fprintf(output, "%s%c%s%c%3.3f%c%3.3f%c%s%c%s\n",
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
	    Q(total_m3,%.3f)
	    Q(target_m3,%.3f)
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
