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

struct MeterOmnipower : public virtual ElectricityMeter, public virtual MeterCommonImplementation {
    MeterOmnipower(WMBus *bus, const char *name, const char *id, const char *key);

    double totalEnergyConsumption();
    double currentPowerConsumption();

    void printMeterHumanReadable(FILE *output);
    void printMeterFields(FILE *output, char separator);
    void printMeterJSON(FILE *output);

private:
    void handleTelegram(Telegram *t);
    void processContent(Telegram *t);

    double total_energy_ {};
    double current_power_ {};
};

MeterOmnipower::MeterOmnipower(WMBus *bus, const char *name, const char *id, const char *key) :
    MeterCommonImplementation(bus, name, id, key, OMNIPOWER_METER, MANUFACTURER_KAM, 0x04)
{
    MeterCommonImplementation::bus()->onTelegram(calll(this,handleTelegram,Telegram*));
}

double MeterOmnipower::totalEnergyConsumption()
{
    return total_energy_;
}

double MeterOmnipower::currentPowerConsumption()
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

void MeterOmnipower::processContent(Telegram *t)
{
    // Meter record:
    // 04 dif (32 Bit Integer/Binary Instantaneous value)
    // 83 vif (Energy Wh)
    // 3b vife (Forward flow contribution only)
    // xx xx xx xx (total energy)

    map<string,pair<int,string>> values;
    parseDV(t, t->content.begin(), t->content.size(), &values);

    int offset;
    extractDVdouble(&values, "04833B", &offset, &total_energy_);
    t->addMoreExplanation(offset, " total power (%f kwh)", total_energy_);
}

ElectricityMeter *createOmnipower(WMBus *bus, const char *name, const char *id, const char *key)
{
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
    fprintf(output, "%s%c%s%c%f%c%f%c%s\n",
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
    fprintf(output, "{"
            QS(media,electricity)
            QS(meter,omnipower)
	    QS(name,%s)
	    QS(id,%s)
	    Q(total_kwh,%f)
	    QS(current_kw,%f)
	    QSE(timestamp,%s)
	    "}\n",
	    name().c_str(),
	    id().c_str(),
	    totalEnergyConsumption(),
	    currentPowerConsumption(),
	    datetimeOfUpdateRobot().c_str());
}
