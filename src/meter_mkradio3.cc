/*
 Copyright (C) 2019 Fredrik Öhrström

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

struct MKRadio3 : public virtual WaterMeter, public virtual MeterCommonImplementation {
    MKRadio3(WMBus *bus, string& name, string& id, string& key);

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
    double target_water_consumption_ {};
};

MKRadio3::MKRadio3(WMBus *bus, string& name, string& id, string& key) :
    MeterCommonImplementation(bus, name, id, key, MeterType::MKRADIO3, MANUFACTURER_TCH, LinkMode::T1)
{
    addMedia(0x62);
    addMedia(0x72);
    MeterCommonImplementation::bus()->onTelegram(calll(this,handleTelegram,Telegram*));
}


double MKRadio3::totalWaterConsumption()
{
    return total_water_consumption_;
}

unique_ptr<WaterMeter> createMKRadio3(WMBus *bus, string& name, string& id, string& key)
{
    return unique_ptr<WaterMeter>(new MKRadio3(bus,name,id,key));
}

void MKRadio3::handleTelegram(Telegram *t)
{
    if (!isTelegramForMe(t)) {
        // This telegram is not intended for this meter.
        return;
    }

    verbose("(%s) telegram for %s %02x%02x%02x%02x\n", "mkradio3",
            name().c_str(),
            t->a_field_address[0], t->a_field_address[1], t->a_field_address[2],
            t->a_field_address[3]);

    t->expectVersion("mkradio3", 0x74);

    if (t->isEncrypted() && !useAes() && !t->isSimulated()) {
        warning("(mkradio3) warning: telegram is encrypted but no key supplied!\n");
    }
    if (useAes()) {
        vector<uchar> aeskey = key();
        decryptMode5_AES_CBC(t, aeskey);
    } else {
        t->content = t->payload;
    }
    char log_prefix[256];
    snprintf(log_prefix, 255, "(%s) log", "mkradio3");
    logTelegram(log_prefix, t->parsed, t->content);
    int content_start = t->parsed.size();
    processContent(t);
    if (isDebugEnabled()) {
        snprintf(log_prefix, 255, "(%s)", "mkradio3");
        t->explainParse(log_prefix, content_start);
    }
    triggerUpdate(t);
}

void MKRadio3::processContent(Telegram *t)
{
    // Meter record:
    map<string,pair<int,DVEntry>> vendor_values;

    // Unfortunately, the MK Radio 3 is mostly a proprieatary protocol
    // simple wrapped inside a wmbus telegram since the ci-field is 0xa2.
    // Which means that the entire payload is manufacturer specific.

    uchar prev_lo = t->content[3];
    uchar prev_hi = t->content[4];
    double prev = (256.0*prev_hi+prev_lo)/10.0;

    string prevs;
    strprintf(prevs, "%02x%02x", prev_lo, prev_hi);
    int offset = t->parsed.size()+3;
    vendor_values["0215"] = { offset, DVEntry(0x15, 0, 0, 0, prevs) };
    t->explanations.push_back({ offset, prevs });
    t->addMoreExplanation(offset, " prev consumption (%f m3)", prev);

    uchar curr_lo = t->content[7];
    uchar curr_hi = t->content[8];
    double curr = (256.0*curr_hi+curr_lo)/10.0;

    string currs;
    strprintf(currs, "%02x%02x", curr_lo, curr_hi);
    offset = t->parsed.size()+7;
    vendor_values["0215"] = { offset, DVEntry(0x15, 0, 0, 0, currs) };
    t->explanations.push_back({ offset, currs });
    t->addMoreExplanation(offset, " curr consumption (%f m3)", curr);

    total_water_consumption_ = prev+curr;
    target_water_consumption_ = prev;
}

void MKRadio3::printMeter(Telegram *t,
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
             "% 3.3f m3\t"
             "%s",
             name().c_str(),
             t->id.c_str(),
             totalWaterConsumption(),
             targetWaterConsumption(),
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
             targetWaterConsumption(), separator,
            datetimeOfUpdateRobot().c_str());

    *fields = buf;

#define Q(x,y) "\""#x"\":"#y","
#define QS(x,y) "\""#x"\":\""#y"\","
#define QSE(x,y) "\""#x"\":\""#y"\""

    snprintf(buf, sizeof(buf)-1, "{"
             QS(media,%s)
             QS(meter,mkradio3)
             QS(name,%s)
             QS(id,%s)
             Q(total_m3,%f)
             Q(target_m3,%f)
             QSE(timestamp,%s)
             "}",
             mediaTypeJSON(t->a_field_device_type).c_str(),
             name().c_str(),
             t->id.c_str(),
             totalWaterConsumption(),
             targetWaterConsumption(),
             datetimeOfUpdateRobot().c_str());

    *json = buf;

    envs->push_back(string("METER_JSON=")+*json);
    envs->push_back(string("METER_TYPE=mkradio3"));
    envs->push_back(string("METER_ID=")+t->id);
    envs->push_back(string("METER_TOTAL_M3=")+to_string(totalWaterConsumption()));
    envs->push_back(string("METER_TARGET_M3=")+to_string(targetWaterConsumption()));
    envs->push_back(string("METER_TIMESTAMP=")+datetimeOfUpdateRobot());
}

bool MKRadio3::hasTotalWaterConsumption()
{
    return true;
}

double MKRadio3::targetWaterConsumption()
{
    return target_water_consumption_;
}

bool MKRadio3::hasTargetWaterConsumption()
{
    return true;
}

double MKRadio3::maxFlow()
{
    return 0.0;
}

bool MKRadio3::hasMaxFlow()
{
    return false;
}

double MKRadio3::flowTemperature()
{
    return 127;
}

bool MKRadio3::hasFlowTemperature()
{
    return false;
}

double MKRadio3::externalTemperature()
{
    return 127;
}

bool MKRadio3::hasExternalTemperature()
{
    return false;
}

string MKRadio3::statusHumanReadable()
{
    return "";
}

string MKRadio3::status()
{
    return "";
}

string MKRadio3::timeDry()
{
    return "";
}

string MKRadio3::timeReversed()
{
    return "";
}

string MKRadio3::timeLeaking()
{
    return "";
}

string MKRadio3::timeBursting()
{
    return "";
}
