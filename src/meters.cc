/*
 Copyright (C) 2017-2020 Fredrik Öhrström

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
#include<numeric>
#include<time.h>
#include<cmath>

struct MeterManagerImplementation : public virtual MeterManager
{
    void addMeter(shared_ptr<Meter> meter)
    {
        meters_.push_back(meter);
    }

    Meter *lastAddedMeter()
    {
        return meters_.back().get();
    }

    void removeAllMeters()
    {
        meters_.clear();
    }

    void forEachMeter(std::function<void(Meter*)> cb)
    {
        for (auto &meter : meters_)
        {
            cb(meter.get());
        }
    }

    bool hasAllMetersReceivedATelegram()
    {
        for (auto &meter : meters_)
        {
            if (meter->numUpdates() == 0) return false;
        }

        return true;
    }

    bool hasMeters()
    {
        return meters_.size() != 0;
    }

    bool handleTelegram(AboutTelegram &about, vector<uchar> data, bool simulated)
    {
        if (!hasMeters())
        {
            if (on_telegram_)
            {
                on_telegram_(about, data);
            }
            return true;
        }

        bool handled = false;

        string ids;
        for (auto &m : meters_)
        {
            bool h = m->handleTelegram(about, data, simulated, &ids);
            if (h) handled = true;
        }
        if (isVerboseEnabled() && !handled)
        {
            verbose("(wmbus) telegram from %s ignored by all configured meters!\n", ids.c_str());
        }
        return handled;
    }

    void onTelegram(function<void(AboutTelegram &about, vector<uchar>)> cb)
    {
        on_telegram_ = cb;
    }
    ~MeterManagerImplementation() {}

private:

    vector<shared_ptr<Meter>> meters_;
    function<void(AboutTelegram&,vector<uchar>)> on_telegram_;
};

shared_ptr<MeterManager> createMeterManager()
{
    return shared_ptr<MeterManager>(new MeterManagerImplementation);
}

MeterCommonImplementation::MeterCommonImplementation(MeterInfo &mi,
                                                     MeterType type) :
    type_(type), name_(mi.name)
{
    ids_ = splitMatchExpressions(mi.id);
    if (mi.key.length() > 0)
    {
        hex2bin(mi.key, &meter_keys_.confidentiality_key);
    }
    /*if (bus->type() == DEVICE_SIMULATION)
    {
        meter_keys_.simulation = true;
    }*/
    for (auto s : mi.shells) {
        addShell(s);
    }
    for (auto j : mi.jsons) {
        addJson(j);
    }
}

void MeterCommonImplementation::addConversions(std::vector<Unit> cs)
{
    for (Unit c : cs)
    {
        conversions_.push_back(c);
    }
}

void MeterCommonImplementation::addShell(string cmdline)
{
    shell_cmdlines_.push_back(cmdline);
}

void MeterCommonImplementation::addJson(string json)
{
    jsons_.push_back(json);
}

vector<string> &MeterCommonImplementation::shellCmdlines()
{
    return shell_cmdlines_;
}

vector<string> &MeterCommonImplementation::additionalJsons()
{
    return jsons_;
}

MeterType MeterCommonImplementation::type()
{
    return type_;
}

void MeterCommonImplementation::addLinkMode(LinkMode lm)
{
    link_modes_.addLinkMode(lm);
}

void MeterCommonImplementation::addPrint(string vname, Quantity vquantity,
                                         function<double(Unit)> getValueFunc, string help, bool field, bool json)
{
    string default_unit = unitToStringLowerCase(defaultUnitForQuantity(vquantity));
    string field_name = vname+"_"+default_unit;
    fields_.push_back(field_name);
    prints_.push_back( { vname, vquantity, defaultUnitForQuantity(vquantity), getValueFunc, NULL, help, field, json, field_name });
}

void MeterCommonImplementation::addPrint(string vname, Quantity vquantity, Unit unit,
                                         function<double(Unit)> getValueFunc, string help, bool field, bool json)
{
    string default_unit = unitToStringLowerCase(defaultUnitForQuantity(vquantity));
    string field_name = vname+"_"+default_unit;
    fields_.push_back(field_name);
    prints_.push_back( { vname, vquantity, unit, getValueFunc, NULL, help, field, json, field_name });
}

void MeterCommonImplementation::addPrint(string vname, Quantity vquantity,
                                         function<string()> getValueFunc,
                                         string help, bool field, bool json)
{
    prints_.push_back( { vname, vquantity, defaultUnitForQuantity(vquantity), NULL, getValueFunc, help, field, json, vname } );
}

vector<string> MeterCommonImplementation::ids()
{
    return ids_;
}

vector<string> MeterCommonImplementation::fields()
{
    return fields_;
}

vector<Print> MeterCommonImplementation::prints()
{
    return prints_;
}

string MeterCommonImplementation::name()
{
    return name_;
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
#define X(mname,linkmodes,info,type,cname) if (t == #mname) return MeterType::type;
LIST_OF_METERS
#undef X
    return MeterType::UNKNOWN;
}

LinkModeSet toMeterLinkModeSet(string& t)
{
#define X(mname,linkmodes,info,type,cname) if (t == #mname) return LinkModeSet(linkmodes);
LIST_OF_METERS
#undef X
    return LinkModeSet();
}

bool MeterCommonImplementation::isTelegramForMe(Telegram *t)
{
    debug("(meter) %s: for me? %s\n", name_.c_str(), t->idsc.c_str());

    bool used_wildcard = false;
    bool id_match = doesIdsMatchExpressions(t->ids, ids_, &used_wildcard);

    if (!id_match) {
        // The id must match.
        debug("(meter) %s: not for me: not my id\n", name_.c_str());
        return false;
    }

    bool valid_driver = isMeterDriverValid(type_, t->dll_mfct, t->dll_type, t->dll_version);
    if (!valid_driver && t->tpl_id_found)
    {
        valid_driver = isMeterDriverValid(type_, t->tpl_mfct, t->tpl_type, t->tpl_version);
    }

    if (!valid_driver)
    {
        // Are we using the right driver? Perhaps not since
        // this particular driver, mfct, media, version combo
        // is not registered in the METER_DETECTION list in meters.h

        if (used_wildcard)
        {
            // The match for the id was not exact, thus the user is listening using a wildcard
            // to many meters and some received matched meter telegrams are not from the right meter type,
            // ie their driver does not match. Lets just ignore telegrams that probably cannot be decoded properly.
            verbose("(meter) ignoring telegram from %s since it matched a wildcard id rule but driver does not match.\n",
                    t->idsc.c_str());
            return false;
        }

        // The match was exact, ie the user has actually specified 12345678 and foo as driver even
        // though they do not match. Lets warn and then proceed. It is common that a user tries a
        // new version of a meter with the old driver, thus it might not be a real error.
        if (isVerboseEnabled() || isDebugEnabled() || !warned_for_telegram_before(t, t->dll_a))
        {
            string possible_drivers = t->autoDetectPossibleDrivers();
            warning("(meter) %s: meter detection did not match the selected driver %s! correct driver is: %s\n"
                    "(meter) Not printing this warning agin for id: %02x%02x%02x%02x mfct: (%s) %s (0x%02x) type: %s (0x%02x) ver: 0x%02x\n",
                    name_.c_str(),
                    toMeterName(type()).c_str(),
                    possible_drivers.c_str(),
                    t->dll_id_b[3], t->dll_id_b[2], t->dll_id_b[1], t->dll_id_b[0],
                    manufacturerFlag(t->dll_mfct).c_str(),
                    manufacturer(t->dll_mfct).c_str(),
                    t->dll_mfct,
                    mediaType(t->dll_type, t->dll_mfct).c_str(), t->dll_type,
                    t->dll_version);

            if (possible_drivers == "unknown!")
            {
                warning("(meter) please consider opening an issue at https://github.com/weetmuts/wmbusmeters/\n");
                warning("(meter) to add support for this unknown mfct,media,version combination\n");
            }
        }
    }

    debug("(meter) %s: yes for me\n", name_.c_str());
    return true;
}

MeterKeys *MeterCommonImplementation::meterKeys()
{
    return &meter_keys_;
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

string concatAllFields(Meter *m, Telegram *t, char c, vector<Print> &prints, vector<Unit> &cs, bool hr)
{
    string s;
    s = "";
    s += m->name() + c;
    if (t->ids.size() > 0)
    {
        s += t->ids.back() + c;
    }
    else
    {
        s += c;
    }
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

string concatFields(Meter *m, Telegram *t, char c, vector<Print> &prints, vector<Unit> &cs, bool hr,
                    vector<string> *selected_fields)
{
    if (selected_fields == NULL || selected_fields->size() == 0)
    {
        return concatAllFields(m, t, c, prints, cs, hr);
    }
    string s;
    s = "";

    for (string field : *selected_fields)
    {
        if (field == "name")
        {
            s += m->name() + c;
            continue;
        }
        if (field == "id")
        {
            s += t->ids.back() + c;
            continue;
        }
        if (field == "timestamp")
        {
            s += m->datetimeOfUpdateHumanReadable() + c;
            continue;
        }
        if (field == "device")
        {
            s += t->about.device + c;
            continue;
        }
        if (field == "rssi_dbm")
        {
            s += to_string(t->about.rssi_dbm) + c;
            continue;
        }

        bool handled = false;
        for (Print p : prints)
        {
            if (p.getValueString)
            {
                if (field == p.vname)
                {
                    s += p.getValueString() + c;
                    handled = true;
                }
            }
            else if (p.getValueDouble)
            {
                string default_unit = unitToStringLowerCase(p.default_unit);
                string var = p.vname+"_"+default_unit;
                if (field == var)
                {
                    s += valueToString(p.getValueDouble(p.default_unit), p.default_unit) + c;
                    handled = true;
                }
                else
                {
                    Unit u = replaceWithConversionUnit(p.default_unit, cs);
                    if (u != p.default_unit)
                    {
                        string unit = unitToStringLowerCase(u);
                        string var = p.vname+"_"+unit;
                        if (field == var)
                        {
                            s += valueToString(p.getValueDouble(u), u) + c;
                            handled = true;
                        }
                    }
                }
            }
        }
        if (!handled)
        {
            s += "?"+field+"?"+c;
        }
    }
    if (s.back() == c) s.pop_back();
    return s;
}

bool MeterCommonImplementation::handleTelegram(AboutTelegram &about, vector<uchar> input_frame, bool simulated, string *ids)
{
    Telegram t;
    t.about = about;
    bool ok = t.parseHeader(input_frame);

    if (simulated) t.markAsSimulated();

    *ids = t.idsc;

    if (!ok || !isTelegramForMe(&t))
    {
        // This telegram is not intended for this meter.
        return false;
    }

    verbose("(meter) %s %s handling telegram from %s\n", name().c_str(), meterName().c_str(), t.ids.back().c_str());

    if (isDebugEnabled())
    {
        string msg = bin2hex(input_frame);
        debug("(meter) %s %s \"%s\"\n", name().c_str(), t.ids.back().c_str(), msg.c_str());
    }

    ok = t.parse(input_frame, &meter_keys_, true);
    if (!ok)
    {
        // Ignoring telegram since it could not be parsed.
        return false;
    }

    char log_prefix[256];
    snprintf(log_prefix, 255, "(%s) log", meterName().c_str());
    logTelegram(t.frame, t.header_size, t.suffix_size);

    // Invoke meter specific parsing!
    processContent(&t);
    // All done....

    if (isDebugEnabled())
    {
        char log_prefix[256];
        snprintf(log_prefix, 255, "(%s)", meterName().c_str());
        t.explainParse(log_prefix, 0);
    }
    triggerUpdate(&t);
    return true;
}

void MeterCommonImplementation::printMeter(Telegram *t,
                                           string *human_readable,
                                           string *fields, char separator,
                                           string *json,
                                           vector<string> *envs,
                                           vector<string> *more_json,
                                           vector<string> *selected_fields)
{
    *human_readable = concatFields(this, t, '\t', prints_, conversions_, true, selected_fields);
    *fields = concatFields(this, t, separator, prints_, conversions_, false, selected_fields);

    string mfct;
    if (t->tpl_id_found)
    {
        mfct = mediaTypeJSON(t->tpl_type, t->tpl_mfct);
    }
    else if (t->ell_id_found)
    {
        mfct = mediaTypeJSON(t->ell_type, t->ell_mfct);
    }
    else
    {
        mfct = mediaTypeJSON(t->dll_type, t->dll_mfct);
    }

    string s;
    s += "{";
    s += "\"media\":\""+mfct+"\",";
    s += "\"meter\":\""+meterName()+"\",";
    s += "\"name\":\""+name()+"\",";
    if (t->ids.size() > 0)
    {
        s += "\"id\":\""+t->ids.back()+"\",";
    }
    else
    {
        s += "\"id\":\"\",";
    }
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
    if (t->about.device != "")
    {
        s += ",";
        s += "\"device\":\""+t->about.device+"\",";
        s += "\"rssi_dbm\":"+to_string(t->about.rssi_dbm);
    }
    for (string add_json : additionalJsons())
    {
        s += ",";
        s += makeQuotedJson(add_json);
    }
    for (string add_json : *more_json)
    {
        s += ",";
        s += makeQuotedJson(add_json);
    }
    s += "}";
    *json = s;

    envs->push_back(string("METER_JSON=")+*json);
    envs->push_back(string("METER_TYPE=")+meterName());
    envs->push_back(string("METER_NAME=")+name());
    if (t->ids.size() > 0)
    {
        envs->push_back(string("METER_ID=")+t->ids.back());
    }
    else
    {
        envs->push_back(string("METER_ID="));
    }

    for (Print p : prints_)
    {
        if (p.json)
        {
            string default_unit = unitToStringUpperCase(p.default_unit);
            string var = p.vname;
            std::transform(var.begin(), var.end(), var.begin(), ::toupper);
            if (p.getValueString) {
                string envvar = "METER_"+var+"="+p.getValueString();
                envs->push_back(envvar);
            }
            if (p.getValueDouble) {
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
    // If the configuration has supplied json_address=Roodroad 123
    // then the env variable METER_address will available and have the content "Roodroad 123"
    for (string add_json : additionalJsons())
    {
        envs->push_back(string("METER_")+add_json);
    }
    for (string add_json : *more_json)
    {
        envs->push_back(string("METER_")+add_json);
    }
}

double WaterMeter::totalWaterConsumption(Unit u) { return -NAN; }
bool  WaterMeter::hasTotalWaterConsumption() { return false; }
double WaterMeter::targetWaterConsumption(Unit u) { return -NAN; }
bool  WaterMeter::hasTargetWaterConsumption() { return false; }
double WaterMeter::maxFlow(Unit u) { return -NAN; }
bool  WaterMeter::hasMaxFlow() { return false; }
double WaterMeter::flowTemperature(Unit u) { return -NAN; }
bool WaterMeter::hasFlowTemperature() { return false; }
double WaterMeter::externalTemperature(Unit u) { return -NAN; }
bool WaterMeter::hasExternalTemperature() { return false; }

string WaterMeter::statusHumanReadable() { return "-NAN"; }
string WaterMeter::status() { return "-NAN"; }
string WaterMeter::timeDry() { return "-NAN"; }
string WaterMeter::timeReversed() { return "-NAN"; }
string WaterMeter::timeLeaking() { return "-NAN"; }
string WaterMeter::timeBursting() { return "-NAN"; }

double HeatMeter::totalEnergyConsumption(Unit u) { return -NAN; }
double HeatMeter::currentPeriodEnergyConsumption(Unit u) { return -NAN; }
double HeatMeter::previousPeriodEnergyConsumption(Unit u) { return -NAN; }
double HeatMeter::currentPowerConsumption(Unit u) { return -NAN; }
double HeatMeter::totalVolume(Unit u) { return -NAN; }

double ElectricityMeter::totalEnergyConsumption(Unit u) { return -NAN; }
double ElectricityMeter::totalEnergyProduction(Unit u) { return -NAN; }
double ElectricityMeter::totalReactiveEnergyConsumption(Unit u) { return -NAN; }
double ElectricityMeter::totalReactiveEnergyProduction(Unit u) { return -NAN; }
double ElectricityMeter::totalApparentEnergyConsumption(Unit u) { return -NAN; }
double ElectricityMeter::totalApparentEnergyProduction(Unit u) { return -NAN; }

double ElectricityMeter::currentPowerConsumption(Unit u) { return -NAN; }
double ElectricityMeter::currentPowerProduction(Unit u) { return -NAN; }

double HeatCostAllocationMeter::currentConsumption(Unit u) { return -NAN; }
string HeatCostAllocationMeter::setDate() { return "NAN"; }
double HeatCostAllocationMeter::consumptionAtSetDate(Unit u) { return -NAN; }

void MeterCommonImplementation::setExpectedTPLSecurityMode(TPLSecurityMode tsm)
{
    expected_tpl_sec_mode_ = tsm;
}

void MeterCommonImplementation::setExpectedELLSecurityMode(ELLSecurityMode dsm)
{
    expected_ell_sec_mode_ = dsm;
}

TPLSecurityMode MeterCommonImplementation::expectedTPLSecurityMode()
{
    return expected_tpl_sec_mode_;
}

ELLSecurityMode MeterCommonImplementation::expectedELLSecurityMode()
{
    return expected_ell_sec_mode_;
}

void detectMeterDriver(int manufacturer, int media, int version, vector<string> *drivers)
{
#define X(TY,MA,ME,VE) { if (manufacturer == MA && (media == ME || ME == -1) && (version == VE || VE == -1)) { drivers->push_back(toMeterName(MeterType::TY)); }}
METER_DETECTION
#undef X
}

bool isMeterDriverValid(MeterType type, int manufacturer, int media, int version)
{
#define X(TY,MA,ME,VE) { if (type == MeterType::TY && manufacturer == MA && (media == ME || ME == -1) && (version == VE || VE == -1)) { return true; }}
METER_DETECTION
#undef X

    return false;
}
