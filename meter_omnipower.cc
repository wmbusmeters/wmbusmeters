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

struct MeterOmnipower : public virtual ElectricityMeter, public virtual MeterCommonImplementation {
    MeterOmnipower(WMBus *bus, const char *name, const char *id, const char *key);

    float totalEnergyConsumption();
    float currentPowerConsumption();

    void printMeterHumanReadable(FILE *output);
    void printMeterFields(FILE *output, char separator);
    void printMeterJSON(FILE *output);

private:
    void handleTelegram(Telegram *t);
    void processContent(Telegram *t);

    float total_energy_ {};
    float current_power_ {};
};

MeterOmnipower::MeterOmnipower(WMBus *bus, const char *name, const char *id, const char *key) :
    MeterCommonImplementation(bus, name, id, key, OMNIPOWER_METER, MANUFACTURER_KAM, 0x04)
{
    MeterCommonImplementation::bus()->onTelegram(calll(this,handleTelegram,Telegram*));
}

float MeterOmnipower::totalEnergyConsumption()
{
    return total_energy_;
}

float MeterOmnipower::currentPowerConsumption()
{
    return current_power_;
}

void MeterOmnipower::handleTelegram(Telegram *t) {

    if (!isTelegramForMe(t)) {
        // This telegram is not intended for this meter.
        return;
    }

    verbose("(omnipower) %s %02x%02x%02x%02x ",
            name().c_str(),
            t->a_field_address[0], t->a_field_address[1], t->a_field_address[2],
            t->a_field_address[3]);

    if (t->a_field_device_type != 0x02) {
        warning("(omnipower) expected telegram for electricity media, but got \"%s\"!\n",
                mediaType(t->m_field, t->a_field_device_type).c_str());
    }

    if (t->m_field != manufacturer() ||
        t->a_field_version != 0x01) {
        warning("(omnipower) expected telegram from KAM meter with version 0x01, but got \"%s\" version 0x2x !\n",
                manufacturerFlag(t->m_field).c_str(), t->a_field_version);
    }

    if (useAes()) {
        vector<uchar> aeskey = key();
        // Proper decryption not yet implemented!
        decryptKamstrupC1(t, aeskey);
    } else {
        t->content = t->payload;
    }
    logTelegram("(omnipower) log", t->parsed, t->content);
    int content_start = t->parsed.size();
    processContent(t);
    if (isDebugEnabled()) {
        t->explainParse("(omnipower)", content_start);
    }
    triggerUpdate(t);
}

void MeterOmnipower::processContent(Telegram *t) {
    vector<uchar> full_content;
    full_content.insert(full_content.end(), t->parsed.begin(), t->parsed.end());
    full_content.insert(full_content.end(), t->content.begin(), t->content.end());

    int rec1dif = t->content[0];
    t->addExplanation(full_content, 1, "%02x dif (%s)", rec1dif, difType(rec1dif).c_str());
    int rec1vif = t->content[1];
    t->addExplanation(full_content, 1, "%02x vif (%s)", rec1vif, vifType(rec1vif).c_str());
    int rec1vife = t->content[2];
    t->addExplanation(full_content, 1, "%02x vife (%s)", rec1vife, vifeType(rec1vif, rec1vife).c_str());

    int rec1val0 = t->content[3];
    int rec1val1 = t->content[4];
    int rec1val2 = t->content[5];
    int rec1val3 = t->content[6];

    int total_energy_raw = rec1val3*256*256*256 + rec1val2*256*256 + rec1val1*256 + rec1val0;
    total_energy_ = ((float)total_energy_raw)/1000.0;
    t->addExplanation(full_content, 4, "%02x%02x%02x%02x total power (%d)",
                      rec1val0, rec1val1, rec1val2, rec1val3, total_energy_raw);
}

ElectricityMeter *createOmnipower(WMBus *bus, const char *name, const char *id, const char *key) {
    return new MeterOmnipower(bus,name,id,key);
}

void MeterOmnipower::printMeterHumanReadable(FILE *output)
{
    fprintf(output, "%s\t%s\t% 3.3f kwh\t% 3.3f kwh\t%s\n",
	    name().c_str(),
	    id().c_str(),
	    totalEnergyConsumption(),
	    currentPowerConsumption(),
	    datetimeOfUpdateHumanReadable().c_str());
}

void MeterOmnipower::printMeterFields(FILE *output, char separator)
{
    fprintf(output, "%s%c%s%c%3.3f%c%3.3f%c%s\n",
	    name().c_str(), separator,
	    id().c_str(), separator,
	    totalEnergyConsumption(), separator,
	    currentPowerConsumption(), separator,
	    datetimeOfUpdateRobot().c_str());
}

#define Q(x,y) "\""#x"\":"#y","
#define QS(x,y) "\""#x"\":\""#y"\","
#define QSE(x,y) "\""#x"\":\""#y"\""

void MeterOmnipower::printMeterJSON(FILE *output)
{
    fprintf(output, "{media:\"electricity\",meter:\"omnipower\","
	    QS(name,%s)
	    QS(id,%s)
	    Q(total_kwh,%.3f)
	    QS(current_kw,%.3f)
	    QSE(timestamp,%s)
	    "}\n",
	    name().c_str(),
	    id().c_str(),
	    totalEnergyConsumption(),
	    currentPowerConsumption(),
	    datetimeOfUpdateRobot().c_str());
}
