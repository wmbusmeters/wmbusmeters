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
#include"units.h"
#include"util.h"

#include<memory.h>
#include<stdio.h>
#include<string>
#include<time.h>
#include<vector>

#define METER_OUTPUT \
    X(total_kwh, totalEnergyConsumption(), Unit::KWH)        \
    X(current_kwh, currentPeriodEnergyConsumption(), Unit::KWH) \
    X(previous_kwh, previousPeriodEnergyConsumption(), Unit::KWH) \


struct MeterVario451 : public virtual HeatMeter, public virtual MeterCommonImplementation {
    MeterVario451(WMBus *bus, string& name, string& id, string& key);

    double totalEnergyConsumption();
    double currentPeriodEnergyConsumption();
    double currentPowerConsumption();
    double previousPeriodEnergyConsumption();
    double totalVolume();

    void printMeter(Telegram *t,
                    string *human_readable,
                    string *fields, char separator,
                    string *json,
                    vector<string> *envs);

    private:
    void handleTelegram(Telegram *t);
    void processContent(Telegram *t);

    double total_energy_kwh_ {};
    double curr_energy_kwh_ {};
    double prev_energy_kwh_ {};
};

MeterVario451::MeterVario451(WMBus *bus, string& name, string& id, string& key) :
    MeterCommonImplementation(bus, name, id, key, VARIO451_METER, MANUFACTURER_TCH, LinkMode::T1)
{
    addMedia(0x04); // C telegrams
    addMedia(0xC3); // T telegrams
    MeterCommonImplementation::bus()->onTelegram(calll(this,handleTelegram,Telegram*));
}

double MeterVario451::totalEnergyConsumption()
{
    return total_energy_kwh_;
}

double MeterVario451::currentPeriodEnergyConsumption()
{
    return curr_energy_kwh_;
}

double MeterVario451::currentPowerConsumption()
{
    return 0;
}

double MeterVario451::previousPeriodEnergyConsumption()
{
    return prev_energy_kwh_;
}

double MeterVario451::totalVolume()
{
    return 0;
}

void MeterVario451::handleTelegram(Telegram *t) {

    if (!isTelegramForMe(t)) {
        // This telegram is not intended for this meter.
        return;
    }

    verbose("(vario451) %s %02x%02x%02x%02x ",
            name().c_str(),
            t->a_field_address[0], t->a_field_address[1], t->a_field_address[2],
            t->a_field_address[3]);

    if (t->isEncrypted() && !useAes() && !t->isSimulated()) {

        // This is ugly but I have no idea how to do this proper way :/
        // Techem Vario 4 Typ 4.5.1 sends T and also encrypted C telegrams
        // We are intrested in T only (for now)

        //warning("(vario451) warning: telegram is encrypted but no key supplied!\n");
        return;
    }
    if (useAes()) {
        vector<uchar> aeskey = key();
        decryptMode1_AES_CTR(t, aeskey);
    } else {
        t->content = t->payload;
    }
    logTelegram("(vario451) log", t->parsed, t->content);
    int content_start = t->parsed.size();
    processContent(t);
    if (isDebugEnabled()) {
        t->explainParse("(vario451)", content_start);
    }
    triggerUpdate(t);
}

void MeterVario451::processContent(Telegram *t) {
    map<string,pair<int,DVEntry>> vendor_values;

    // Unfortunately, the Techem Vario 4 Typ 4.5.1 is mostly a proprieatary protocol
    // simple wrapped inside a wmbus telegram since the ci-field is 0xa2.
    // Which means that the entire payload is manufacturer specific.

    uchar prev_lo = t->content[3];
    uchar prev_hi = t->content[4];
    double prev = (256.0*prev_hi+prev_lo)/1000;

    string prevs;
    strprintf(prevs, "%02x%02x", prev_lo, prev_hi);
    int offset = t->parsed.size()+3;
    vendor_values["0215"] = { offset, DVEntry(0x15, 0, 0, 0, prevs) };
    t->explanations.push_back({ offset, prevs });
    t->addMoreExplanation(offset, " energy used in previous billing period (%f GJ)", prev);

    uchar curr_lo = t->content[7];
    uchar curr_hi = t->content[8];
    double curr = (256.0*curr_hi+curr_lo)/1000;

    string currs;
    strprintf(currs, "%02x%02x", curr_lo, curr_hi);
    offset = t->parsed.size()+7;
    vendor_values["0215"] = { offset, DVEntry(0x15, 0, 0, 0, currs) };
    t->explanations.push_back({ offset, currs });
    t->addMoreExplanation(offset, " energy used in current billing period (%f GJ)", curr);

    total_energy_kwh_ = convert(prev+curr, Unit::GJ, Unit::KWH);
    curr_energy_kwh_ = convert(curr, Unit::GJ, Unit::KWH);
    prev_energy_kwh_ = convert(prev, Unit::GJ, Unit::KWH);
}

unique_ptr<HeatMeter> createVario451(WMBus *bus, string& name, string& id, string& key) {
    return unique_ptr<HeatMeter>(new MeterVario451(bus,name,id,key));
}


void MeterVario451::printMeter(Telegram *t,
                               string *human_readable,
                               string *fields, char separator,
                               string *json,
                               vector<string> *envs)
{
    string s;
    s = "";
    s += name() + "\t";
    s += t->id + "\t";
#define X(key,func,unit) {s+=strWithUnitHR(func,unit) + "\t";}
    METER_OUTPUT
#undef X
    s += datetimeOfUpdateHumanReadable();
    *human_readable = s;

    s = "";
    s += name() + "\t";
    s += t->id + "\t";
#define X(key,func,unit) {s+=strWithUnitLowerCase(func,unit) + separator;}
    METER_OUTPUT
#undef X
    s += datetimeOfUpdateHumanReadable();
    *fields = s;

#define Q(x,y) "\""#x"\":"#y","
#define QS(x,y) "\""#x"\":\""#y"\","
#define QSE(x,y) "\""#x"\":\""#y"\""

    char buf[65535];
    snprintf(buf, sizeof(buf)-1, "{"
             QS(media,heat)
             QS(meter,vario451)
             QS(name,%s)
             QS(id,%s)
             Q(total_kwh,%f)
             Q(current_kwh,%f)
             Q(previous_kwh,%f)
             QSE(timestamp,%s)
             "}",
             name().c_str(),
             t->id.c_str(),
             totalEnergyConsumption(),
             currentPeriodEnergyConsumption(),
             previousPeriodEnergyConsumption(),
             datetimeOfUpdateRobot().c_str());

    *json = buf;

    envs->push_back(string("METER_JSON=")+*json);
    envs->push_back(string("METER_TYPE=vario451"));
    envs->push_back(string("METER_ID=")+t->id);
    envs->push_back(string("METER_TOTAL_KWH=")+to_string(totalEnergyConsumption()));
    envs->push_back(string("METER_CURRENT_KWH=")+to_string(currentPeriodEnergyConsumption()));
    envs->push_back(string("METER_PREVIOUS_KWH=")+to_string(previousPeriodEnergyConsumption()));
    envs->push_back(string("METER_TIMESTAMP=")+datetimeOfUpdateRobot());
}
