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

#include"meters.h"
#include"meters_common_implementation.h"
#include"units.h"
#include"wmbus.h"
#include"wmbus_utils.h"

#include<algorithm>
#include<memory.h>

MeterCommonImplementation::MeterCommonImplementation(WMBus *bus, string& name, string& id, string& key,
                                                     MeterType type, int manufacturer,
                                                     LinkMode required_link_mode) :
    type_(type), name_(name), bus_(bus),
    required_link_mode_(required_link_mode)
{
    use_aes_ = true;
    ids_ = splitIds(id);
    if (key.length() == 0) {
        use_aes_ = false;
    } else {
        hex2bin(key, &key_);
    }
    if (manufacturer) {
        manufacturers_.insert(manufacturer);
    }
}

void MeterCommonImplementation::addConversions(std::vector<Unit> cs)
{
    for (Unit c : cs)
    {
        conversions_.push_back(c);
    }
}

MeterType MeterCommonImplementation::type()
{
    return type_;
}

vector<int> MeterCommonImplementation::media()
{
    return media_;
}

void MeterCommonImplementation::addMedia(int m)
{
    media_.push_back(m);
}

void MeterCommonImplementation::addPrint(string vname, Quantity vquantity,
                                         function<double(Unit)> getValueFunc, string help, bool field, bool json)
{
    prints_.push_back( { vname, vquantity, defaultUnitForQuantity(vquantity), getValueFunc, NULL, help, field, json });
}

void MeterCommonImplementation::addPrint(string vname, Quantity vquantity,
                                         function<string()> getValueFunc,
                                         string help, bool field, bool json)
{
    prints_.push_back( { vname, vquantity, defaultUnitForQuantity(vquantity), NULL, getValueFunc, help, field, json } );
}

void MeterCommonImplementation::addManufacturer(int m)
{
    manufacturers_.insert(m);
}

vector<string> MeterCommonImplementation::ids()
{
    return ids_;
}

string MeterCommonImplementation::name()
{
    return name_;
}

WMBus *MeterCommonImplementation::bus()
{
    return bus_;
}

LinkMode MeterCommonImplementation::requiredLinkMode()
{
    return required_link_mode_;
}

void MeterCommonImplementation::onUpdate(function<void(Telegram*,Meter*)> cb)
{
    on_update_.push_back(cb);
}

int MeterCommonImplementation::numUpdates()
{
    return num_updates_;
}

string MeterCommonImplementation::datetimeOfUpdateHumanReadable()
{
    char datetime[40];
    memset(datetime, 0, sizeof(datetime));
    strftime(datetime, 20, "%Y-%m-%d %H:%M.%S", localtime(&datetime_of_update_));
    return string(datetime);
}

string MeterCommonImplementation::datetimeOfUpdateRobot()
{
    char datetime[40];
    memset(datetime, 0, sizeof(datetime));
    // This is the date time in the Greenwich timezone (Zulu time), dont get surprised!
    strftime(datetime, sizeof(datetime), "%FT%TZ", gmtime(&datetime_of_update_));
    return string(datetime);
}

string toMeterName(MeterType mt)
{
#define X(mname,link,info,type,cname) if (mt == MeterType::type) return #mname;
LIST_OF_METERS
#undef X
    return "unknown";
}

MeterType toMeterType(string& t)
{
#define X(mname,link,info,type,cname) if (t == #mname) return MeterType::type;
LIST_OF_METERS
#undef X
    return MeterType::UNKNOWN;
}

LinkMode toMeterLinkMode(string& t)
{
#define X(mname,link,info,type,cname) if (t == #mname) return LinkMode::link;
LIST_OF_METERS
#undef X
    return LinkMode::UNKNOWN;
}

bool MeterCommonImplementation::isTelegramForMe(Telegram *t)
{
    debug("(meter) %s: for me? %s\n", name_.c_str(), t->id.c_str());

    bool id_match = false;
    for (auto id : ids_)
    {
        if (id == t->id) {
            id_match = true;
            break;
        }
        if (id == "*") {
            id_match = true;
            break;
        }
    }

    if (!id_match) {
        // The id must match.
        debug("(meter) %s: not for me: not my id\n", name_.c_str());
        return false;
    }

    if (manufacturers_.count(t->m_field) == 0) {
        // We are not that strict for the manufacturer.
        // Simply warn.
        warning("(meter) %s: probably not for me since manufacturer differs\n", name_.c_str());
    }

    bool media_match = false;
    for (auto m : media_) {
        if (m == t->a_field_device_type) {
            media_match = true;
            break;
        }
    }

    if (!media_match) {
        warning("(meter) %s: probably not for me since media does not match\n", name_.c_str());
    }

    debug("(meter) %s: yes for me\n", name_.c_str());
    return true;
}

bool MeterCommonImplementation::useAes()
{
    return use_aes_;
}

vector<uchar> MeterCommonImplementation::key()
{
    return key_;
}

vector<string> MeterCommonImplementation::getRecords()
{
    vector<string> recs;
    for (auto& p : values_)
    {
        recs.push_back(p.first);
    }
    return recs;
}

double MeterCommonImplementation::getRecordAsDouble(string record)
{
    return 0.0;
}

uint16_t MeterCommonImplementation::getRecordAsUInt16(string record)
{
    return 0;
}

void MeterCommonImplementation::triggerUpdate(Telegram *t)
{
    datetime_of_update_ = time(NULL);
    num_updates_++;
    for (auto &cb : on_update_) if (cb) cb(t, this);
    t->handled = true;
}

string concatFields(Meter *m, Telegram *t, char c, vector<Print> &prints, vector<Unit> &cs, bool hr)
{
    string s;
    s = "";
    s += m->name() + c;
    s += t->id + c;
    for (Print p : prints)
    {
        if (p.field)
        {
            if (p.getValueDouble)
            {
                Unit u = replaceWithConversionUnit(p.default_unit, cs);
                double v = p.getValueDouble(u);
                if (hr) {
                    s += valueToString(v, u);
                    s += " "+unitToStringHR(u);
                } else {
                    s += to_string(v);
                }
            }
            if (p.getValueString)
            {
                s += p.getValueString();
            }
            s += c;
        }
    }
    s += m->datetimeOfUpdateHumanReadable();
    return s;
}

void MeterCommonImplementation::handleTelegram(Telegram *t)
{
    if (!isTelegramForMe(t)) {
        // This telegram is not intended for this meter.
        return;
    }

    verbose("(%s) %s %02x%02x%02x%02x ",
            meterName().c_str(),
            name().c_str(),
            t->a_field_address[0], t->a_field_address[1], t->a_field_address[2],
            t->a_field_address[3]);

    t->expectVersion(meterName().c_str(), expectedVersion());

    if (t->isEncrypted() && !useAes() && !t->isSimulated())
    {
        warning("(%s) warning: telegram is encrypted but no key supplied!\n",
                meterName().c_str());
    }
    if (useAes()) {
        vector<uchar> aeskey = key();
        if (encryptionMode() == EncryptionMode::AES_CTR) {
            decryptMode1_AES_CTR(t, aeskey);
        }
        if (encryptionMode() == EncryptionMode::AES_CBC) {
            decryptMode1_AES_CTR(t, aeskey);
        }
    } else {
        t->content = t->payload;
    }

    char log_prefix[256];
    snprintf(log_prefix, 255, "(%s) log", meterName().c_str());
    logTelegram(log_prefix, t->parsed, t->content);
    int content_start = t->parsed.size();

    // Invoke meter specific parsing!
    processContent(t);
    // All done....

    if (isDebugEnabled())
    {
        char log_prefix[256];
        snprintf(log_prefix, 255, "(%s)", meterName().c_str());
        t->explainParse("(vario451)", content_start);
    }
    triggerUpdate(t);
}

void MeterCommonImplementation::printMeter(Telegram *t,
                                           string *human_readable,
                                           string *fields, char separator,
                                           string *json,
                                           vector<string> *envs)
{
    *human_readable = concatFields(this, t, '\t', prints_, conversions_, true);
    *fields = concatFields(this, t, separator, prints_, conversions_, false);

    string s;
    s += "{";
    s += "\"media\":\""+mediaTypeJSON(t->a_field_device_type)+"\",";
    s += "\"meter\":\""+meterName()+"\",";
    s += "\"name\":\""+name()+"\",";
    s += "\"id\":\""+t->id+"\",";
    for (Print p : prints_)
    {
        if (p.json)
        {
            string default_unit = unitToStringLowerCase(p.default_unit);
            string var = p.vname;
            if (p.getValueString) {
                s += "\""+var+"\":\""+p.getValueString()+"\",";
            }
            if (p.getValueDouble) {
                s += "\""+var+"_"+default_unit+"\":"+valueToString(p.getValueDouble(p.default_unit), p.default_unit)+",";

                Unit u = replaceWithConversionUnit(p.default_unit, conversions_);
                if (u != p.default_unit)
                {
                    string unit = unitToStringLowerCase(u);
                    s += "\""+var+"_"+unit+"\":"+valueToString(p.getValueDouble(u), u)+",";
                }
            }
        }
    }
    s += "\"timestamp\":\""+datetimeOfUpdateRobot()+"\"";
    s += "}";
    *json = s;

    envs->push_back(string("METER_JSON=")+*json);
    envs->push_back(string("METER_TYPE=")+meterName());
    envs->push_back(string("METER_ID=")+t->id);

    for (Print p : prints_)
    {
        if (p.json)
        {
            string default_unit = unitToStringUpperCase(p.default_unit);
            string var = p.vname;
            if (p.getValueString) {
                s += "\""+var+"\":\""+p.getValueString()+"\",";
            }
            if (p.getValueDouble) {
                std::transform(var.begin(), var.end(), var.begin(), ::toupper);
                string envvar = "METER_"+var+"_"+default_unit+"="+valueToString(p.getValueDouble(p.default_unit), p.default_unit);
                envs->push_back(envvar);

                Unit u = replaceWithConversionUnit(p.default_unit, conversions_);
                if (u != p.default_unit)
                {
                    string unit = unitToStringUpperCase(u);
                    string envvar = "METER_"+var+"_"+unit+"="+valueToString(p.getValueDouble(u), u);
                    envs->push_back(envvar);
                }
            }
        }
    }
    envs->push_back(string("METER_TIMESTAMP=")+datetimeOfUpdateRobot());
}

double WaterMeter::totalWaterConsumption(Unit u) { return -47.11; }
bool  WaterMeter::hasTotalWaterConsumption() { return false; }
double WaterMeter::targetWaterConsumption(Unit u) { return -47.11; }
bool  WaterMeter::hasTargetWaterConsumption() { return false; }
double WaterMeter::maxFlow(Unit u) { return -47.11; }
bool  WaterMeter::hasMaxFlow() { return false; }
double WaterMeter::flowTemperature(Unit u) { return -47.11; }
bool WaterMeter::hasFlowTemperature() { return false; }
double WaterMeter::externalTemperature(Unit u) { return -47.11; }
bool WaterMeter::hasExternalTemperature() { return false; }

string WaterMeter::statusHumanReadable() { return "-47.11"; }
string WaterMeter::status() { return "-47.11"; }
string WaterMeter::timeDry() { return "-47.11"; }
string WaterMeter::timeReversed() { return "-47.11"; }
string WaterMeter::timeLeaking() { return "-47.11"; }
string WaterMeter::timeBursting() { return "-47.11"; }

double HeatMeter::totalEnergyConsumption(Unit u) { return -47.11; }
double HeatMeter::currentPeriodEnergyConsumption(Unit u) { return -47.11; }
double HeatMeter::previousPeriodEnergyConsumption(Unit u) { return -47.11; }
double HeatMeter::currentPowerConsumption(Unit u) { return -47.11; }
double HeatMeter::totalVolume(Unit u) { return -47.11; }

double ElectricityMeter::totalEnergyConsumption() { return -47.11; }
double ElectricityMeter::currentPowerConsumption() { return -47.11; }
double ElectricityMeter::totalEnergyProduction() { return -47.11; }
double ElectricityMeter::currentPowerProduction() { return -47.11; }

double HeatCostMeter::currentConsumption() { return -47.11; }
string HeatCostMeter::setDate() { return "47.11"; }
double HeatCostMeter::consumptionAtSetDate() { return -47.11; }

void MeterCommonImplementation::setEncryptionMode(EncryptionMode em)
{
    enc_mode_ = em;
}

EncryptionMode MeterCommonImplementation::encryptionMode()
{
    return enc_mode_;
}

void MeterCommonImplementation::setExpectedVersion(int version)
{
    expected_meter_version_ = version;
}

int MeterCommonImplementation::expectedVersion()
{
    return expected_meter_version_;
}
