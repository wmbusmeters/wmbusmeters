// Copyright (c) 2018 Fredrik Öhrström
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

struct MeterMultical302 : public virtual HeatMeter, public virtual MeterCommonImplementation {
    MeterMultical302(WMBus *bus, const char *name, const char *id, const char *key);

    float totalPowerConsumption();
    float currentPowerConsumption();
    float totalVolume();

    void printMeterHumanReadable(FILE *output);
    void printMeterFields(FILE *output, char separator);
    void printMeterJSON(FILE *output);

private:
    void handleTelegram(Telegram *t);
    void processContent(Telegram *t);

    float total_power_ {};
    float current_power_ {};
    float total_volume_ {};
};

MeterMultical302::MeterMultical302(WMBus *bus, const char *name, const char *id, const char *key) :
    MeterCommonImplementation(bus, name, id, key, MULTICAL302_METER, MANUFACTURER_KAM, 0x04)
{
    MeterCommonImplementation::bus()->onTelegram(calll(this,handleTelegram,Telegram*));
}

float MeterMultical302::totalPowerConsumption()
{
    return total_power_;
}

float MeterMultical302::currentPowerConsumption()
{
    return current_power_;
}

float MeterMultical302::totalVolume()
{
    return total_volume_;
}

void MeterMultical302::handleTelegram(Telegram *t) {

    if (!isTelegramForMe(t)) {
        // This telegram is not intended for this meter.
        return;
    }

    verbose("(multical302) %s %02x%02x%02x%02x ",
            name().c_str(),
            t->a_field_address[0], t->a_field_address[1], t->a_field_address[2],
            t->a_field_address[3]);

    if (t->a_field_device_type != 0x04) {
        warning("(multical302) expected telegram for heat media, but got \"%s\"!\n",
                mediaType(t->m_field, t->a_field_device_type).c_str());
    }

    /*
    if (t->m_field != manufacturer() ||
        t->a_field_version != 0x1b) {
        warning("(multical302) expected telegram from KAM meter with version 0x1b, but got \"%s\" version 0x2x !\n",
                manufacturerFlag(t->m_field).c_str(), t->a_field_version);
                }*/

    if (useAes()) {
        vector<uchar> aeskey = key();
        decryptKamstrupC1(t, aeskey);
    } else {
        t->content = t->payload;
    }
    logTelegram("(multical302) log", t->parsed, t->content);
    int content_start = t->parsed.size();
    processContent(t);
    if (isDebugEnabled()) {
        t->explainParse("(multical302)", content_start);
    }
    triggerUpdate(t);
}

void MeterMultical302::processContent(Telegram *t) {
    vector<uchar> full_content;
    full_content.insert(full_content.end(), t->parsed.begin(), t->parsed.end());
    full_content.insert(full_content.end(), t->content.begin(), t->content.end());

    int crc0 = t->content[0];
    int crc1 = t->content[1];
    t->addExplanation(full_content, 2, "%02x%02x plcrc", crc0, crc1);
    int frame_type = t->content[2];
    t->addExplanation(full_content, 1, "%02x frame type (%s)", frame_type, frameTypeKamstrupC1(frame_type).c_str());

    if (frame_type == 0x79) {
	if (t->content.size() != 17) {
            fprintf(stderr, "(multical302) warning: Unexpected length of frame %zu. Expected 17 bytes! ", t->content.size());
            padWithZeroesTo(&t->content, 17, &full_content);
            warning("\n");
        }

        t->addExplanation(full_content, 4, "%02x%02x%02x%02x unknown", t->content[3], t->content[4], t->content[5], t->content[6]);

        int rec1val0 = t->content[7];
        int rec1val1 = t->content[8];
        int rec1val2 = t->content[9];

        t->addExplanation(full_content, 4, "%02x%02x%02x unknown", t->content[10], t->content[11], t->content[12]);

        int total_power_raw  = rec1val2*256*256 + rec1val1*256 + rec1val0;
	total_power_ = total_power_raw;
        t->addExplanation(full_content, 3, "%02x%02x%02x total power (%d)",
                          rec1val0, rec1val1, rec1val2, total_power_raw);

	int rec2val0 = t->content[13];
        int rec2val1 = t->content[14];
	int rec2val2 = t->content[15];

        int total_volume_raw = rec2val2*256*256 + rec2val1*256 + rec2val0;
	total_volume_ = total_volume_raw;
        t->addExplanation(full_content, 3, "%02x%02x%02x total volume (%d)",
                          rec2val0, rec2val1, rec2val2, total_volume_raw);
    }
    else if (frame_type == 0x78)
    {
	if (t->content.size() != 26) {
            fprintf(stderr, "(multical302) warning: Unexpected length of frame %zu. Expected 26 bytes! ", t->content.size());
            padWithZeroesTo(&t->content, 26, &full_content);
            warning("\n");
        }

        vector<uchar> unknowns;
        unknowns.insert(unknowns.end(), t->content.begin()+3, t->content.begin()+24);
        string hex = bin2hex(unknowns);
        t->addExplanation(full_content, 23-2, "%s unknown", hex.c_str());

        int rec1val0 = t->content[24];
        int rec1val1 = t->content[25];

        int current_power_raw = (rec1val1*256 + rec1val0)*100;
        current_power_ = current_power_raw;
        t->addExplanation(full_content, 2, "%02x%02x current power (%d)",
                          rec1val0, rec1val1, current_power_raw);
    }
    else {
        warning("(multical302) warning: unknown frame %02x\n", frame_type);
    }
}

HeatMeter *createMultical302(WMBus *bus, const char *name, const char *id, const char *key) {
    return new MeterMultical302(bus,name,id,key);
}

void MeterMultical302::printMeterHumanReadable(FILE *output)
{
    fprintf(output, "%s\t%s\t% 3.3f kwh\t% 3.3f m3\t% 3.3f kwh\t%s\n",
	    name().c_str(),
	    id().c_str(),
	    totalPowerConsumption(),
            totalVolume(),
	    currentPowerConsumption(),
	    datetimeOfUpdateHumanReadable().c_str());
}

void MeterMultical302::printMeterFields(FILE *output, char separator)
{
    fprintf(output, "%s%c%s%c%3.3f%c%3.3f%c%3.3f%c%s\n",
	    name().c_str(), separator,
	    id().c_str(), separator,
	    totalPowerConsumption(), separator,
            totalVolume(), separator,
	    currentPowerConsumption(), separator,
	    datetimeOfUpdateHumanReadable().c_str());
}

#define Q(x,y) "\""#x"\":"#y","
#define QS(x,y) "\""#x"\":\""#y"\","
#define QSE(x,y) "\""#x"\":\""#y"\""

void MeterMultical302::printMeterJSON(FILE *output)
{
    fprintf(output, "{media:\"heat\",meter:\"multical302\","
	    QS(name,%s)
	    QS(id,%s)
	    Q(total_kwh,%.3f)
            Q(total_volume_m3,%.3f)
	    QS(current_kw,%.3f)
	    QSE(timestamp,%s)
	    "}\n",
	    name().c_str(),
	    id().c_str(),
	    totalPowerConsumption(),
            totalVolume(),
	    currentPowerConsumption(),
	    datetimeOfUpdateRobot().c_str());
}
