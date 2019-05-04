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
    double totalWaterConsumption(Unit u);
    bool  hasTotalWaterConsumption();

private:
    void handleTelegram(Telegram *t);
    void processContent(Telegram *t);
    string decodeTime(int time);

    double total_water_consumption_m3_ {};
};

unique_ptr<WaterMeter> createSupercom587(WMBus *bus, string& name, string& id, string& key)
{
    return unique_ptr<WaterMeter>(new MeterSupercom587(bus,name,id,key));
}

MeterSupercom587::MeterSupercom587(WMBus *bus, string& name, string& id, string& key) :
    MeterCommonImplementation(bus, name, id, key, MeterType::SUPERCOM587, MANUFACTURER_SON, LinkMode::T1)
{
    addMedia(0x06);
    addMedia(0x07);

    addPrint("total", Quantity::Volume,
             [&](Unit u){ return totalWaterConsumption(u); },
             "The total water consumption recorded by this meter.",
             true);

    MeterCommonImplementation::bus()->onTelegram(calll(this,handleTelegram,Telegram*));
}

double MeterSupercom587::totalWaterConsumption(Unit u)
{
    assertQuantity(u, Quantity::Volume);
    return convert(total_water_consumption_m3_, Unit::M3, u);
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

    extractDVdouble(&values, "0C13", &offset, &total_water_consumption_m3_);
    t->addMoreExplanation(offset, " total consumption (%f m3)", total_water_consumption_m3_);
}

bool MeterSupercom587::hasTotalWaterConsumption()
{
    return true;
}
