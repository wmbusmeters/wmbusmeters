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

struct MeterAmiplus : public virtual ElectricityMeter, public virtual MeterCommonImplementation {
    MeterAmiplus(WMBus *bus, string& name, string& id, string& key);

    double totalEnergyConsumption();
    double currentPowerConsumption();
    double totalEnergyProduction();
    double currentPowerProduction();

    void printMeter(Telegram *t,
                    string *human_readable,
                    string *fields, char separator,
                    string *json,
                    vector<string> *envs);

private:
    void handleTelegram(Telegram *t);
    void processContent(Telegram *t);

    double total_energy_ {};
    double current_power_ {};
    double total_energy_returned_ {};
    double current_power_returned_ {};
    string device_date_time_;
};

MeterAmiplus::MeterAmiplus(WMBus *bus, string& name, string& id, string& key) :
    MeterCommonImplementation(bus, name, id, key, MeterType::AMIPLUS, 0, LinkMode::T1)
{
    // This is one manufacturer of Amiplus compatible meters.
    addManufacturer(MANUFACTURER_APA);
    addMedia(0x02); // Electricity meter

    // This is another manufacturer
    addManufacturer(MANUFACTURER_DEV);
    // Oddly, this device has not been configured to send as a electricity meter,
    // but instead a device/media type that is used for gateway or relays or something?
    addMedia(0x37); // Radio converter (meter side)

    setExpectedVersion(0x02);

    MeterCommonImplementation::bus()->onTelegram(calll(this,handleTelegram,Telegram*));
}

double MeterAmiplus::totalEnergyConsumption()
{
    return total_energy_;
}

double MeterAmiplus::currentPowerConsumption()
{
    return current_power_;
}

double MeterAmiplus::totalEnergyProduction()
{
    return total_energy_returned_;
}

double MeterAmiplus::currentPowerProduction()
{
    return current_power_returned_;
}

void MeterAmiplus::handleTelegram(Telegram *t) {

    if (!isTelegramForMe(t)) {
        // This telegram is not intended for this meter.
        return;
    }

    verbose("(amiplus) %s %02x%02x%02x%02x ",
            name().c_str(),
            t->a_field_address[0], t->a_field_address[1], t->a_field_address[2],
            t->a_field_address[3]);

    if (t->isEncrypted() && !useAes() && !t->isSimulated()) {
        warning("(amiplus) warning: telegram is encrypted but no key supplied!\n");
    }
    if (useAes()) {
        vector<uchar> aeskey = key();
        decryptMode5_AES_CBC(t, aeskey);
    } else {
        t->content = t->payload;
    }
    logTelegram("(amiplus) log", t->parsed, t->content);
    int content_start = t->parsed.size();
    processContent(t);
    if (isDebugEnabled()) {
        t->explainParse("(amiplus)", content_start);
    }
    triggerUpdate(t);
}

void MeterAmiplus::processContent(Telegram *t)
{
    map<string,pair<int,DVEntry>> values;
    parseDV(t, t->content, t->content.begin(), t->content.size(), &values);

    int offset;
    string key;

    if (findKey(ValueInformation::EnergyWh, 0, &key, &values)) {
        extractDVdouble(&values, key, &offset, &total_energy_);
        t->addMoreExplanation(offset, " total energy (%f kwh)", total_energy_);
    }

    if (findKey(ValueInformation::PowerW, 0, &key, &values)) {
        extractDVdouble(&values, key, &offset, &current_power_);
        t->addMoreExplanation(offset, " current power (%f kw)", current_power_);
    }

    extractDVdouble(&values, "0E833C", &offset, &total_energy_returned_);
    t->addMoreExplanation(offset, " total energy returned (%f kwh)", total_energy_returned_);

    extractDVdouble(&values, "0BAB3C", &offset, &current_power_returned_);
    t->addMoreExplanation(offset, " current power returned (%f kw)", current_power_returned_);

    if (findKey(ValueInformation::DateTime, 0, &key, &values)) {
        struct tm datetime;
        extractDVdate(&values, key, &offset, &datetime);
        device_date_time_ = strdatetime(&datetime);
        t->addMoreExplanation(offset, " device datetime (%s)", device_date_time_.c_str());
    }

}

unique_ptr<ElectricityMeter> createAmiplus(WMBus *bus, string& name, string& id, string& key)
{
    return unique_ptr<ElectricityMeter>(new MeterAmiplus(bus,name,id,key));
}

void MeterAmiplus::printMeter(Telegram *t,
                              string *human_readable,
                              string *fields, char separator,
                              string *json,
                              vector<string> *envs)
{

    char buf[65536];
    buf[65535] = 0;

    snprintf(buf, sizeof(buf)-1, "%s\t%s\t% 3.3f kwh\t% 3.3f kw\t% 3.3f kwh\t% 3.3f kw\t%s",
             name().c_str(),
             t->id.c_str(),
             totalEnergyConsumption(),
             currentPowerConsumption(),
             totalEnergyProduction(),
             currentPowerProduction(),
             datetimeOfUpdateHumanReadable().c_str());

    *human_readable = buf;

    snprintf(buf, sizeof(buf)-1, "%s%c%s%c%f%c%f%c%f%c%f%c%s",
             name().c_str(), separator,
             t->id.c_str(), separator,
             totalEnergyConsumption(), separator,
             currentPowerConsumption(), separator,
             totalEnergyProduction(), separator,
             currentPowerProduction(), separator,
             datetimeOfUpdateRobot().c_str());

    *fields = buf;

#define Q(x,y) "\""#x"\":"#y","
#define QS(x,y) "\""#x"\":\""#y"\","
#define QSE(x,y) "\""#x"\":\""#y"\""

    snprintf(buf, sizeof(buf)-1, "{"
             QS(media,electricity)
             QS(meter,amiplus)
             QS(name,%s)
             QS(id,%s)
             Q(total_energy_consumption_kwh,%f)
             Q(current_power_consumption_kw,%f)
             Q(total_energy_production_kwh,%f)
             Q(current_power_production_kw,%f)
             QS(device_date_time,%s)
             QSE(timestamp,%s)
             "}",
             name().c_str(),
             t->id.c_str(),
             totalEnergyConsumption(),
             currentPowerConsumption(),
             totalEnergyProduction(),
             currentPowerProduction(),
             device_date_time_.c_str(),
             datetimeOfUpdateRobot().c_str());

    *json = buf;

    envs->push_back(string("METER_JSON=")+*json);
    envs->push_back(string("METER_TYPE=amiplus"));
    envs->push_back(string("METER_ID=")+t->id);
    envs->push_back(string("METER_TOTAL_ENERGY_CONSUMPTION_KWH=")+to_string(totalEnergyConsumption()));
    envs->push_back(string("METER_CURRENT_POWER_CONSUMPTION_KW=")+to_string(currentPowerConsumption()));
    envs->push_back(string("METER_TOTAL_ENERGY_PRODUCTION_KWH=")+to_string(totalEnergyProduction()));
    envs->push_back(string("METER_CURRENT_POWER_PRODUCTION_KW=")+to_string(currentPowerProduction()));
    envs->push_back(string("METER_TIMESTAMP=")+datetimeOfUpdateRobot());
}
