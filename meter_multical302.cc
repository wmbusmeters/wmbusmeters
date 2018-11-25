/*
 Copyright (C) 2018 Fredrik Öhrström

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

    double totalEnergyConsumption();
    double currentPowerConsumption();
    double totalVolume();


    void printMeter(string *human_readable,
                    string *fields, char separator,
                    string *json,
                    vector<string> *envs);

private:
    void handleTelegram(Telegram *t);
    void processContent(Telegram *t);

    double total_energy_ {};
    double current_power_ {};
    double total_volume_ {};
};

MeterMultical302::MeterMultical302(WMBus *bus, const char *name, const char *id, const char *key) :
    MeterCommonImplementation(bus, name, id, key, MULTICAL302_METER, MANUFACTURER_KAM, 0x04, LinkModeC1)
{
    MeterCommonImplementation::bus()->onTelegram(calll(this,handleTelegram,Telegram*));
}

double MeterMultical302::totalEnergyConsumption()
{
    return total_energy_;
}

double MeterMultical302::currentPowerConsumption()
{
    return current_power_;
}

double MeterMultical302::totalVolume()
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
        decryptMode1_AES_CTR(t, aeskey);
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
    vector<uchar>::iterator bytes = t->content.begin();

    int crc0 = t->content[0];
    int crc1 = t->content[1];
    t->addExplanation(bytes, 2, "%02x%02x payload crc", crc0, crc1);
    int frame_type = t->content[2];
    t->addExplanation(bytes, 1, "%02x frame type (%s)", frame_type, frameTypeKamstrupC1(frame_type).c_str());

    if (frame_type == 0x79) {
	if (t->content.size() != 17) {
            fprintf(stderr, "(multical302) warning: Unexpected length of frame %zu. Expected 17 bytes! ", t->content.size());
            padWithZeroesTo(&t->content, 17, &t->content);
            warning("\n");
        }

        // This code should be rewritten to use parseDV see the Multical21 code.
        // But I cannot do this without more examples of 302 telegrams.
        t->addExplanation(bytes, 4, "%02x%02x%02x%02x unknown", t->content[3], t->content[4], t->content[5], t->content[6]);

        int rec1val0 = t->content[7];
        int rec1val1 = t->content[8];
        int rec1val2 = t->content[9];

        t->addExplanation(bytes, 4, "%02x%02x%02x unknown", t->content[10], t->content[11], t->content[12]);

        int total_energy_raw  = rec1val2*256*256 + rec1val1*256 + rec1val0;
	total_energy_ = total_energy_raw;
        t->addExplanation(bytes, 3, "%02x%02x%02x total power (%d)",
                          rec1val0, rec1val1, rec1val2, total_energy_raw);

	int rec2val0 = t->content[13];
        int rec2val1 = t->content[14];
	int rec2val2 = t->content[15];

        int total_volume_raw = rec2val2*256*256 + rec2val1*256 + rec2val0;
	total_volume_ = total_volume_raw;
        t->addExplanation(bytes, 3, "%02x%02x%02x total volume (%d)",
                          rec2val0, rec2val1, rec2val2, total_volume_raw);
    }
    else if (frame_type == 0x78)
    {
	if (t->content.size() != 26) {
            fprintf(stderr, "(multical302) warning: Unexpected length of frame %zu. Expected 26 bytes! ", t->content.size());
            padWithZeroesTo(&t->content, 26, &t->content);
            warning("\n");
        }

        // This code should be rewritten to use parseDV see the Multical21 code.
        // But I cannot do this without more examples of 302 telegrams.
        vector<uchar> unknowns;
        unknowns.insert(unknowns.end(), t->content.begin()+3, t->content.begin()+24);
        string hex = bin2hex(unknowns);
        t->addExplanation(bytes, 23-2, "%s unknown", hex.c_str());

        int rec1val0 = t->content[24];
        int rec1val1 = t->content[25];

        int current_power_raw = (rec1val1*256 + rec1val0)*100;
        current_power_ = current_power_raw;
        t->addExplanation(bytes, 2, "%02x%02x current power (%d)",
                          rec1val0, rec1val1, current_power_raw);
    }
    else {
        warning("(multical302) warning: unknown frame %02x (did you use the correct encryption key?)\n", frame_type);
    }
}

HeatMeter *createMultical302(WMBus *bus, const char *name, const char *id, const char *key) {
    return new MeterMultical302(bus,name,id,key);
}

void MeterMultical302::printMeter(string *human_readable,
                                  string *fields, char separator,
                                  string *json,
                                  vector<string> *envs)
{
    char buf[65536];
    buf[65535] = 0;

    snprintf(buf, sizeof(buf)-1, "%s\t%s\t% 3.3f kwh\t% 3.3f m3\t% 3.3f kwh\t%s",
             name().c_str(),
             id().c_str(),
             totalEnergyConsumption(),
             totalVolume(),
             currentPowerConsumption(),
             datetimeOfUpdateHumanReadable().c_str());

    *human_readable = buf;

    snprintf(buf, sizeof(buf)-1, "%s%c%s%c%f%c%f%c%f%c%s",
             name().c_str(), separator,
             id().c_str(), separator,
             totalEnergyConsumption(), separator,
             totalVolume(), separator,
             currentPowerConsumption(), separator,
             datetimeOfUpdateRobot().c_str());

    *fields = buf;

#define Q(x,y) "\""#x"\":"#y","
#define QS(x,y) "\""#x"\":\""#y"\","
#define QSE(x,y) "\""#x"\":\""#y"\""

    snprintf(buf, sizeof(buf)-1, "{"
             QS(media,heat)
             QS(meter,multical302)
             QS(name,%s)
             QS(id,%s)
             Q(total_kwh,%f)
             Q(total_volume_m3,%f)
             QS(current_kw,%f)
             QSE(timestamp,%s)
             "}",
             name().c_str(),
             id().c_str(),
             totalEnergyConsumption(),
             totalVolume(),
             currentPowerConsumption(),
             datetimeOfUpdateRobot().c_str());

    *json = buf;

    envs->push_back(string("METER_JSON=")+*json);
    envs->push_back(string("METER_TYPE=multical302"));
    envs->push_back(string("METER_ID=")+id());
    envs->push_back(string("METER_TOTAL_KWH=")+to_string(totalEnergyConsumption()));
    envs->push_back(string("METER_TOTAL_VOLUME_M3=")+to_string(totalVolume()));
    envs->push_back(string("METER_CURRENT_KW=")+to_string(currentPowerConsumption()));
    envs->push_back(string("METER_TIMESTAMP=")+datetimeOfUpdateRobot());
}
