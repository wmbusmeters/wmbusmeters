/*
 Copyright (C) 2017-2018 Fredrik Öhrström
 Copyright (C) 2018 David Mallon

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

struct MeterIperl : public virtual WaterMeter, public virtual MeterCommonImplementation {
    MeterIperl(WMBus *bus, const char *name, const char *id, const char *key);

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

    void printMeter(string *human_readable,
                    string *fields, char separator,
                    string *json,
                    vector<string> *envs);

private:
    void handleTelegram(Telegram *t);
    void processContent(Telegram *t);
    string decodeTime(int time);

    double total_water_consumption_ {};
};

MeterIperl::MeterIperl(WMBus *bus, const char *name, const char *id, const char *key) :
    MeterCommonImplementation(bus, name, id, key, IPERL_METER, MANUFACTURER_SEN, 0x16, LinkModeT1)
{
    MeterCommonImplementation::bus()->onTelegram(calll(this,handleTelegram,Telegram*));
}


double MeterIperl::totalWaterConsumption()
{
    return total_water_consumption_;
}

WaterMeter *createIperl(WMBus *bus, const char *name, const char *id, const char *key)
{
    return new MeterIperl(bus,name,id,key);
}

void MeterIperl::handleTelegram(Telegram *t)
{
    if (!isTelegramForMe(t)) {
        // This telegram is not intended for this meter.
        return;
    }

    verbose("(%s) telegram for %s %02x%02x%02x%02x\n", "iperl",
            name().c_str(),
            t->a_field_address[0], t->a_field_address[1], t->a_field_address[2],
            t->a_field_address[3]);

    if (t->a_field_device_type != 0x07 && t->a_field_device_type != 0x06) {
        warning("(%s) expected telegram for cold or warm water media, but got \"%s\"!\n", "iperl",
                mediaType(t->m_field, t->a_field_device_type).c_str());
    }

    updateMedia(t->a_field_device_type);

    if (t->m_field != manufacturer() ||
        t->a_field_version != 0x68) {
        warning("(%s) expected telegram from SEN meter with version 0x%02x, "
                "but got \"%s\" meter with version 0x%02x !\n", "iperl",
                0x68,
                manufacturerFlag(t->m_field).c_str(),
                t->a_field_version);
    }

    if (useAes()) {
        vector<uchar> aeskey = key();
		decryptMode5_AES_CBC(t, aeskey);
    } else {
        t->content = t->payload;
    }
    char log_prefix[256];
    snprintf(log_prefix, 255, "(%s) log", "iperl");
    logTelegram(log_prefix, t->parsed, t->content);
    int content_start = t->parsed.size();
    processContent(t);
    if (isDebugEnabled()) {
        snprintf(log_prefix, 255, "(%s)", "iperl");
        t->explainParse(log_prefix, content_start);
    }
    triggerUpdate(t);
}

void MeterIperl::processContent(Telegram *t)
{
    vector<uchar>::iterator bytes = t->content.begin();


    map<string,pair<int,string>> values;
    parseDV(t, t->content.begin(), t->content.size(), &values);

    int offset;

    extractDVdouble(&values, "0413", &offset, &total_water_consumption_);
    t->addMoreExplanation(offset, " total consumption (%f m3)", total_water_consumption_);
}

void MeterIperl::printMeter(string *human_readable,
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
             id().c_str(),
             totalWaterConsumption(),
             datetimeOfUpdateHumanReadable().c_str());

    *human_readable = buf;

    snprintf(buf, sizeof(buf)-1,
             "%s%c"
             "%s%c"
             "%f%c"
             "%s",
             name().c_str(), separator,
             id().c_str(), separator,
             totalWaterConsumption(), separator,
            datetimeOfUpdateRobot().c_str());

    *fields = buf;

#define Q(x,y) "\""#x"\":"#y","
#define QS(x,y) "\""#x"\":\""#y"\","
#define QSE(x,y) "\""#x"\":\""#y"\""

    snprintf(buf, sizeof(buf)-1, "{"
             QS(media,%s)
             QS(meter,iperl)
             QS(name,%s)
             QS(id,%s)
             Q(total_m3,%f)
             QSE(timestamp,%s)
             "}",
             mediaType(manufacturer(), media()).c_str(),
             name().c_str(),
             id().c_str(),
             totalWaterConsumption(),
             datetimeOfUpdateRobot().c_str());

    *json = buf;

    envs->push_back(string("METER_JSON=")+*json);
    envs->push_back(string("METER_TYPE=iperl"));
    envs->push_back(string("METER_ID=")+id());
    envs->push_back(string("METER_TOTAL_M3=")+to_string(totalWaterConsumption()));
    envs->push_back(string("METER_TIMESTAMP=")+datetimeOfUpdateRobot());
}

bool MeterIperl::hasTotalWaterConsumption()
{
    return true;
}

double MeterIperl::targetWaterConsumption()
{
    return 0.0;
}

bool MeterIperl::hasTargetWaterConsumption()
{
    return false;
}

double MeterIperl::maxFlow()
{
    return 0.0;
}

bool MeterIperl::hasMaxFlow()
{
    return false;
}

double MeterIperl::flowTemperature()
{
    return 127;
}

bool MeterIperl::hasFlowTemperature()
{
    return false;
}

double MeterIperl::externalTemperature()
{
    return 127;
}

bool MeterIperl::hasExternalTemperature()
{
    return false;
}

string MeterIperl::statusHumanReadable()
{
    return "";
}

string MeterIperl::status()
{
    return "";
}

string MeterIperl::timeDry()
{
    return "";
}

string MeterIperl::timeReversed()
{
    return "";
}

string MeterIperl::timeLeaking()
{
    return "";
}

string MeterIperl::timeBursting()
{
    return "";
}
