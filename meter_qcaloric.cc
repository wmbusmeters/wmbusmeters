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

#include<map>
#include<memory.h>
#include<stdio.h>
#include<string>
#include<time.h>
#include<vector>

struct MeterQCaloric : public virtual HeatCostMeter, public virtual MeterCommonImplementation {
    MeterQCaloric(WMBus *bus, const char *name, const char *id, const char *key);

    double totalEnergyConsumption();

    void printMeter(string *human_readable,
                    string *fields, char separator,
                    string *json,
                    vector<string> *envs);

private:
    void handleTelegram(Telegram *t);
    void processContent(Telegram *t);

    double total_energy_ {};
};

MeterQCaloric::MeterQCaloric(WMBus *bus, const char *name, const char *id, const char *key) :
    MeterCommonImplementation(bus, name, id, key, QCALORIC_METER, MANUFACTURER_QDS, 0x08, LinkModeC1)
{
    MeterCommonImplementation::bus()->onTelegram(calll(this,handleTelegram,Telegram*));
}

double MeterQCaloric::totalEnergyConsumption()
{
    return total_energy_;
}

void MeterQCaloric::handleTelegram(Telegram *t) {

    if (!isTelegramForMe(t)) {
        // This telegram is not intended for this meter.
        return;
    }

    verbose("(qcaloric) %s %02x%02x%02x%02x ",
            name().c_str(),
            t->a_field_address[0], t->a_field_address[1], t->a_field_address[2],
            t->a_field_address[3]);

    if (t->a_field_device_type != 0x08) {
        warning("(qcaloric) expected telegram for heat cost allocator, but got \"%s\"!\n",
                mediaType(t->m_field, t->a_field_device_type).c_str());
    }

    if (t->m_field != manufacturer() ||
        t->a_field_version != 0x35) {
        warning("(qcaloric) expected telegram from QDS meter with version 0x01, but got \"%s\" version 0x%02x !\n",
                manufacturerFlag(t->m_field).c_str(), t->a_field_version);
    }

    if (useAes()) {
        vector<uchar> aeskey = key();
        decryptMode5_AES_CBC(t, aeskey);
    } else {
        t->content = t->payload;
    }
    logTelegram("(qcaloric) log", t->parsed, t->content);
    int content_start = t->parsed.size();
    processContent(t);
    if (isDebugEnabled()) {
        t->explainParse("(qcaloric)", content_start);
    }
    triggerUpdate(t);
}

void MeterQCaloric::processContent(Telegram *t)
{
    map<string,pair<int,DVEntry>> values;
    parseDV(t, t->content, t->content.begin(), t->content.size(), &values);
}

unique_ptr<HeatCostMeter> createQCaloric(WMBus *bus, const char *name, const char *id, const char *key)
{
    return unique_ptr<HeatCostMeter>(new MeterQCaloric(bus,name,id,key));
}

void MeterQCaloric::printMeter(string *human_readable,
                               string *fields, char separator,
                               string *json,
                               vector<string> *envs)
{

    char buf[65536];
    buf[65535] = 0;

    snprintf(buf, sizeof(buf)-1, "%s\t%s\t% 3.3f kwh\t%s",
             name().c_str(),
             id().c_str(),
             totalEnergyConsumption(),
             datetimeOfUpdateHumanReadable().c_str());

    *human_readable = buf;

    snprintf(buf, sizeof(buf)-1, "%s%c%s%c%f%c%s",
             name().c_str(), separator,
             id().c_str(), separator,
             totalEnergyConsumption(), separator,
             datetimeOfUpdateRobot().c_str());

    *fields = buf;

#define Q(x,y) "\""#x"\":"#y","
#define QS(x,y) "\""#x"\":\""#y"\","
#define QSE(x,y) "\""#x"\":\""#y"\""

    snprintf(buf, sizeof(buf)-1, "{"
             QS(media,electricity)
             QS(meter,omnipower)
             QS(name,%s)
             QS(id,%s)
             Q(total_kwh,%f)
             QSE(timestamp,%s)
             "}",
             name().c_str(),
             id().c_str(),
             totalEnergyConsumption(),
             datetimeOfUpdateRobot().c_str());

    *json = buf;

    envs->push_back(string("METER_JSON=")+*json);
    envs->push_back(string("METER_TYPE=qcaloric"));
    envs->push_back(string("METER_ID=")+id());
    envs->push_back(string("METER_TOTAL_KWH=")+to_string(totalEnergyConsumption()));
    envs->push_back(string("METER_TIMESTAMP=")+datetimeOfUpdateRobot());
}
