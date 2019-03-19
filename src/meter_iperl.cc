/*
 Copyright (C) 2017-2019 Fredrik Öhrström
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
    MeterIperl(WMBus *bus, string& name, string& id, string& key);

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
    double max_flow_ {};
};

MeterIperl::MeterIperl(WMBus *bus, string& name, string& id, string& key) :
    MeterCommonImplementation(bus, name, id, key, IPERL_METER, MANUFACTURER_SEN, LinkMode::T1)
{
    addMedia(0x06);
    addMedia(0x07);
    MeterCommonImplementation::bus()->onTelegram(calll(this,handleTelegram,Telegram*));
}


double MeterIperl::totalWaterConsumption()
{
    return total_water_consumption_;
}

unique_ptr<WaterMeter> createIperl(WMBus *bus, string& name, string& id, string& key)
{
    return unique_ptr<WaterMeter>(new MeterIperl(bus,name,id,key));
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

    t->expectVersion("iperl", 0x68);

    if (t->isEncrypted() && !useAes() && !t->isSimulated()) {
        warning("(iperl) warning: telegram is encrypted but no key supplied!\n");
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

    map<string,pair<int,DVEntry>> values;
    parseDV(t, t->content, t->content.begin(), t->content.size(), &values);

    int offset;
    string key;

    if(findKey(ValueInformation::Volume, 0, &key, &values)) {
        extractDVdouble(&values, key, &offset, &total_water_consumption_);
        t->addMoreExplanation(offset, " total consumption (%f m3)", total_water_consumption_);
    }

    if(findKey(ValueInformation::VolumeFlow, ANY_STORAGENR, &key, &values)) {
        extractDVdouble(&values, key, &offset, &max_flow_);
        t->addMoreExplanation(offset, " max flow (%f m3/h)", max_flow_);
    }
}

void MeterIperl::printMeter(Telegram *t,
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
             "% 3.3f m3/h\t"
             "%s",
             name().c_str(),
             t->id.c_str(),
             totalWaterConsumption(),
             maxFlow(),
             datetimeOfUpdateHumanReadable().c_str());

    *human_readable = buf;

    snprintf(buf, sizeof(buf)-1,
             "%s%c"
             "%s%c"
             "%f%c"
             "%f%c"
             "%s",
             name().c_str(), separator,
             t->id.c_str(), separator,
             totalWaterConsumption(), separator,
             maxFlow(), separator,
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
             Q(max_flow_m3h,%f)
             QSE(timestamp,%s)
             "}",
             mediaType(manufacturer(), t->a_field_device_type).c_str(),
             name().c_str(),
             t->id.c_str(),
             totalWaterConsumption(),
             maxFlow(),
             datetimeOfUpdateRobot().c_str());

    *json = buf;

    envs->push_back(string("METER_JSON=")+*json);
    envs->push_back(string("METER_TYPE=iperl"));
    envs->push_back(string("METER_ID=")+t->id);
    envs->push_back(string("METER_TOTAL_M3=")+to_string(totalWaterConsumption()));
    envs->push_back(string("METER_MAX_FLOW_M3H=")+to_string(maxFlow()));
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
    return max_flow_;
}

bool MeterIperl::hasMaxFlow()
{
    return true;
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
