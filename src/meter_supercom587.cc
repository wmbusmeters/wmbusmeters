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
#include<map>
#include<memory.h>
#include<stdio.h>
#include<string>
#include<time.h>
#include<vector>

using namespace std;

struct MeterSupercom587 : public virtual WaterMeter, public virtual MeterCommonImplementation {
    MeterSupercom587(WMBus *bus, string& name, string& id, string& key);

    // Total water counted through the meter
    double totalWaterConsumption();
    bool  hasTotalWaterConsumption();
    double targetWaterConsumption();
    bool  hasTargetWaterConsumption();
    double maxFlow();
    bool  hasMaxFlow();
    double flowTemperature();
    bool  hasFlowTemperature();
    double externalTemperature();
    bool  hasExternalTemperature();

    string statusHumanReadable();
    string status();
    string timeDry();
    string timeReversed();
    string timeLeaking();
    string timeBursting();

    void printMeter(Telegram *t,
                    string *human_readable,
                    string *fields, char separator,
                    string *json,
                    vector<string> *envs);

private:
    void handleTelegram(Telegram *t);
    void processContent(Telegram *t);
    string decodeTime(int time);

    double total_water_consumption_ {};
};

MeterSupercom587::MeterSupercom587(WMBus *bus, string& name, string& id, string& key) :
    MeterCommonImplementation(bus, name, id, key, SUPERCOM587_METER, MANUFACTURER_SON, LinkMode::T1)
{
    addMedia(0x06);
    addMedia(0x07);
    MeterCommonImplementation::bus()->onTelegram(calll(this,handleTelegram,Telegram*));
}


double MeterSupercom587::totalWaterConsumption()
{
    return total_water_consumption_;
}

unique_ptr<WaterMeter> createSupercom587(WMBus *bus, string& name, string& id, string& key)
{
    return unique_ptr<WaterMeter>(new MeterSupercom587(bus,name,id,key));
}

void MeterSupercom587::handleTelegram(Telegram *t)
{
    if (!isTelegramForMe(t)) {
        // This telegram is not intended for this meter.
        return;
    }

    verbose("(%s) telegram for %s %02x%02x%02x%02x\n", "supercom587",
            name().c_str(),
            t->a_field_address[0], t->a_field_address[1], t->a_field_address[2],
            t->a_field_address[3]);

    t->expectVersion("supercom587", 0x3c);

    if (t->isEncrypted() && !useAes() && !t->isSimulated()) {
        warning("(supercom587) warning: telegram is encrypted but no key supplied!\n");
    }
    if (useAes()) {
        vector<uchar> aeskey = key();
        decryptMode5_AES_CBC(t, aeskey);
    } else {
        t->content = t->payload;
    }
    char log_prefix[256];
    snprintf(log_prefix, 255, "(%s) log", "supercom587");
    logTelegram(log_prefix, t->parsed, t->content);
    int content_start = t->parsed.size();
    processContent(t);
    if (isDebugEnabled()) {
        snprintf(log_prefix, 255, "(%s)", "supercom587");
        t->explainParse(log_prefix, content_start);
    }
    triggerUpdate(t);
}

void MeterSupercom587::processContent(Telegram *t)
{
    // Meter record:

    map<string,pair<int,DVEntry>> values;
    parseDV(t, t->content, t->content.begin(), t->content.size(), &values);

    int offset;

    extractDVdouble(&values, "0C13", &offset, &total_water_consumption_);
    t->addMoreExplanation(offset, " total consumption (%f m3)", total_water_consumption_);
}

void MeterSupercom587::printMeter(Telegram *t,
                                  string *human_readable,
                                  string *fields, char separator,
                                  string *json,
                                  vector<string> *envs)
{
    char buf[65536];
    buf[65535] = 0;

    snprintf(buf, sizeof(buf)-1,
             "%s\t"
             "%s\t"
             "% 3.3f m3\t"
             "%s",
             name().c_str(),
             t->id.c_str(),
             totalWaterConsumption(),
             datetimeOfUpdateHumanReadable().c_str());

    *human_readable = buf;

    snprintf(buf, sizeof(buf)-1,
             "%s%c"
             "%s%c"
             "%f%c"
             "%s",
             name().c_str(), separator,
             t->id.c_str(), separator,
             totalWaterConsumption(), separator,
            datetimeOfUpdateRobot().c_str());

    *fields = buf;

#define Q(x,y) "\""#x"\":"#y","
#define QS(x,y) "\""#x"\":\""#y"\","
#define QSE(x,y) "\""#x"\":\""#y"\""

    snprintf(buf, sizeof(buf)-1, "{"
             QS(media,%s)
             QS(meter,supercom587)
             QS(name,%s)
             QS(id,%s)
             Q(total_m3,%f)
             QSE(timestamp,%s)
             "}",
             mediaType(t->a_field_device_type).c_str(),
             name().c_str(),
             t->id.c_str(),
             totalWaterConsumption(),
             datetimeOfUpdateRobot().c_str());

    *json = buf;

    envs->push_back(string("METER_JSON=")+*json);
    envs->push_back(string("METER_TYPE=supercom587"));
    envs->push_back(string("METER_ID=")+t->id);
    envs->push_back(string("METER_TOTAL_M3=")+to_string(totalWaterConsumption()));
    envs->push_back(string("METER_TIMESTAMP=")+datetimeOfUpdateRobot());
}

bool MeterSupercom587::hasTotalWaterConsumption()
{
    return true;
}

double MeterSupercom587::targetWaterConsumption()
{
    return 0.0;
}

bool MeterSupercom587::hasTargetWaterConsumption()
{
    return false;
}

double MeterSupercom587::maxFlow()
{
    return 0.0;
}

bool MeterSupercom587::hasMaxFlow()
{
    return false;
}

double MeterSupercom587::flowTemperature()
{
    return 127;
}

bool MeterSupercom587::hasFlowTemperature()
{
    return false;
}

double MeterSupercom587::externalTemperature()
{
    return 127;
}

bool MeterSupercom587::hasExternalTemperature()
{
    return false;
}

string MeterSupercom587::statusHumanReadable()
{
    return "";
}

string MeterSupercom587::status()
{
    return "";
}

string MeterSupercom587::timeDry()
{
    return "";
}

string MeterSupercom587::timeReversed()
{
    return "";
}

string MeterSupercom587::timeLeaking()
{
    return "";
}

string MeterSupercom587::timeBursting()
{
    return "";
}
