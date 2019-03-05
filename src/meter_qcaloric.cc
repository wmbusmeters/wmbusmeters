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
    MeterQCaloric(WMBus *bus, string& name, string& id, string& key);

    double currentConsumption();
    string setDate();
    double consumptionAtSetDate();

    void printMeter(string id,
                    string *human_readable,
                    string *fields, char separator,
                    string *json,
                    vector<string> *envs);

private:
    void handleTelegram(Telegram *t);
    void processContent(Telegram *t);

    // Telegram type 1
    double current_consumption_ {};
    string set_date_;
    double consumption_at_set_date_ {};
    string set_date_17_;
    double consumption_at_set_date_17_ {};
    string error_date_;

    // Telegram type 2
    string vendor_proprietary_data_;
    string device_date_time_;
};

MeterQCaloric::MeterQCaloric(WMBus *bus, string& name, string& id, string& key) :
    MeterCommonImplementation(bus, name, id, key, QCALORIC_METER, MANUFACTURER_QDS, 0x08, LinkModeC1)
{
    MeterCommonImplementation::bus()->onTelegram(calll(this,handleTelegram,Telegram*));
}

double MeterQCaloric::currentConsumption()
{
    return current_consumption_;
}

string MeterQCaloric::setDate()
{
    return set_date_;
}

double MeterQCaloric::consumptionAtSetDate()
{
    return consumption_at_set_date_;
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
    // These meter sends two types of telegrams:
    // Type 1:
    // (qcaloric) 0f: 0B dif (6 digit BCD Instantaneous value)
    // (qcaloric) 10: 6E vif (Units for H.C.A.)
    // (qcaloric) 11: 270100 current consumption (127.000000 hca)
    //
    // (qcaloric) 14: 4B dif (6 digit BCD Instantaneous value storagenr=1)
    // (qcaloric) 15: 6E vif (Units for H.C.A.)
    // (qcaloric) 16: 450100 consumption at set date (145.000000 hca)
    //
    // (qcaloric) 19: 42 dif (16 Bit Integer/Binary Instantaneous value storagenr=1)
    // (qcaloric) 1a: 6C vif (Date type G)
    // (qcaloric) 1b: 5F2C set date (2018-12-31) set date (2018-12-31)
    //
    // (qcaloric) 1d: CB dif (6 digit BCD Instantaneous value storagenr=1)
    // (qcaloric) 1e: 08 dife (subunit=0 tariff=0 storagenr=17)
    // (qcaloric) 1f: 6E vif (Units for H.C.A.)
    // (qcaloric) 20: 790000 consumption at set date 17 (79.000000 hca)
    //
    // (qcaloric) 23: C2 dif (16 Bit Integer/Binary Instantaneous value storagenr=1)
    // (qcaloric) 24: 08 dife (subunit=0 tariff=0 storagenr=17)
    // (qcaloric) 25: 6C vif (Date type G)
    // (qcaloric) 26: 7F21 set date 17 (2019-01-31)
    //
    // (qcaloric) 28: 32 dif (16 Bit Integer/Binary Value during error state)
    // (qcaloric) 29: 6C vif (Date type G)
    // (qcaloric) 2a: FFFF error date (2127-15-31)

    // (qcaloric) 2c: 04 dif (32 Bit Integer/Binary Instantaneous value)
    // (qcaloric) 2d: 6D vif (Date and time type)
    // (qcaloric) 2e: 200B7422 device datetime (2019-02-20 11:32)

    // Type 2:
    // (qcaloric) 0b: 0D dif (variable length Instantaneous value)
    // (qcaloric) 0c: FF vif (Vendor extension)
    // (qcaloric) 0d: 5F vife (?)
    // (qcaloric) 0e: varlen=53
    // (qcaloric) 0f: 0082750000810007B06EFFFF270100005F2C450100007F2179000000008000800080008000000000000000000C002E005700BEFF2F
    // (qcaloric) 44: 04 dif (32 Bit Integer/Binary Instantaneous value)
    // (qcaloric) 45: 6D vif (Date and time type)
    // (qcaloric) 46: 2D0B7422 device datetime (2019-02-20 11:45)

    map<string,pair<int,DVEntry>> values;
    parseDV(t, t->content, t->content.begin(), t->content.size(), &values);

    int offset;
    string key;

    if (findKey(ValueInformation::HeatCostAllocation, 0, &key, &values)) {
        extractDVdouble(&values, key, &offset, &current_consumption_);
        t->addMoreExplanation(offset, " current consumption (%f hca)", current_consumption_);
    }

    if (findKey(ValueInformation::Date, 1, &key, &values)) {
        struct tm date;
        extractDVdate(&values, key, &offset, &date);
        set_date_ = strdate(&date);
        t->addMoreExplanation(offset, " set date (%s)", set_date_.c_str());
    }

    if (findKey(ValueInformation::HeatCostAllocation, 1, &key, &values)) {
        extractDVdouble(&values, key, &offset, &consumption_at_set_date_);
        t->addMoreExplanation(offset, " consumption at set date (%f hca)", consumption_at_set_date_);
    }

    if (findKey(ValueInformation::HeatCostAllocation, 17, &key, &values)) {
        extractDVdouble(&values, key, &offset, &consumption_at_set_date_17_);
        t->addMoreExplanation(offset, " consumption at set date 17 (%f hca)", consumption_at_set_date_17_);
    }

    if (findKey(ValueInformation::Date, 17, &key, &values)) {
        struct tm date;
        extractDVdate(&values, key, &offset, &date);
        set_date_17_ = strdate(&date);
        t->addMoreExplanation(offset, " set date 17 (%s)", set_date_17_.c_str());
    }

    key = "326C";
    if (hasKey(&values, key)) {
        struct tm date;
        extractDVdate(&values, key, &offset, &date);
        error_date_ = strdate(&date);
        t->addMoreExplanation(offset, " error date (%s)", error_date_.c_str());
    }

    if (findKey(ValueInformation::DateTime, 0, &key, &values)) {
        struct tm datetime;
        extractDVdate(&values, key, &offset, &datetime);
        device_date_time_ = strdatetime(&datetime);
        t->addMoreExplanation(offset, " device datetime (%s)", device_date_time_.c_str());
    }

    key = "0DFF5F";
    if (hasKey(&values, key)) {
        string hex;
        extractDVstring(&values, key, &offset, &hex);
        t->addMoreExplanation(offset, " vendor extension data");
        // This is not stored anywhere yet, we need to understand it, if necessary.
    }
}

unique_ptr<HeatCostMeter> createQCaloric(WMBus *bus, string& name, string& id, string& key)
{
    return unique_ptr<HeatCostMeter>(new MeterQCaloric(bus,name,id,key));
}

void MeterQCaloric::printMeter(string id,
                               string *human_readable,
                               string *fields, char separator,
                               string *json,
                               vector<string> *envs)
{

    char buf[65536];
    buf[65535] = 0;

    snprintf(buf, sizeof(buf)-1, "%s\t%s\t% 3.0f hca\t%s\t% 3.0f hca\t%s",
             name().c_str(),
             id.c_str(),
             currentConsumption(),
             setDate().c_str(),
             consumptionAtSetDate(),
             datetimeOfUpdateHumanReadable().c_str());

    *human_readable = buf;

    snprintf(buf, sizeof(buf)-1, "%s%c%s%c%f%c%s%c%f%c%s",
             name().c_str(), separator,
             id.c_str(), separator,
             currentConsumption(), separator,
             setDate().c_str(), separator,
             consumptionAtSetDate(), separator,
             datetimeOfUpdateRobot().c_str());

    *fields = buf;

#define Q(x,y)   "\""#x"\":"#y","
#define QS(x,y)  "\""#x"\":\""#y"\","
#define QSE(x,y) "\""#x"\":\""#y"\""

    snprintf(buf, sizeof(buf)-1, "{"
             QS(media,heat_cost_allocation)
             QS(meter,qcaloric)
             QS(name,%s)
             QS(id,%s)
             Q(current_consumption_hca,%f)
             QS(set_date,%s)
             Q(consumption_at_set_date_hca,%f)
             QS(set_date_17,%s)
             Q(consumption_at_set_date_17_hca,%f)
             QS(error_date,%s)
             QS(device_date_time,%s)
             QSE(timestamp,%s)
             "}",
             name().c_str(),
             id.c_str(),
             currentConsumption(),
             setDate().c_str(),
             consumptionAtSetDate(),
             set_date_17_.c_str(),
             consumption_at_set_date_17_,
             error_date_.c_str(),
             device_date_time_.c_str(),
             datetimeOfUpdateRobot().c_str());

    *json = buf;

    envs->push_back(string("METER_JSON=")+*json);
    envs->push_back(string("METER_TYPE=qcaloric"));
    envs->push_back(string("METER_ID=")+id);
    envs->push_back(string("METER_CURRENT_CONSUMPTION_HCA=")+to_string(currentConsumption()));
    envs->push_back(string("METER_SET_DATE=")+setDate());
    envs->push_back(string("METER_CONSUMPTION_AT_SET_DATE_HCA=")+to_string(consumptionAtSetDate()));
    envs->push_back(string("METER_SET_DATE_17=")+set_date_17_);
    envs->push_back(string("METER_CONSUMPTION_AT_SET_DATE_17_HCA=")+to_string(consumption_at_set_date_17_));
    envs->push_back(string("METER_ERROR_DATE=")+error_date_);
    envs->push_back(string("METER_TIMESTAMP=")+datetimeOfUpdateRobot());
}
