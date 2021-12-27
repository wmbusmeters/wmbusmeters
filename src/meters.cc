/*
 Copyright (C) 2017-2021 Fredrik Öhrström

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

#include"config.h"
#include"meters.h"
#include"meter_detection.h"
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
private:
    bool is_daemon_ {};
    bool should_analyze_ {};
    OutputFormat analyze_format_ {};
    vector<MeterInfo> meter_templates_;
    vector<shared_ptr<Meter>> meters_;
    function<void(AboutTelegram&,vector<uchar>)> on_telegram_;
    function<void(Telegram*t,Meter*)> on_meter_updated_;

public:
    void addMeterTemplate(MeterInfo &mi)
    {
        meter_templates_.push_back(mi);
    }

    void addMeter(shared_ptr<Meter> meter)
    {
        meters_.push_back(meter);
        meter->setIndex(meters_.size());
        meter->onUpdate(on_meter_updated_);
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
        return meters_.size() != 0 || meter_templates_.size() != 0;
    }

    void warnForUnknownDriver(string name, Telegram *t)
    {
        int mfct = t->dll_mfct;
        int media = t->dll_type;
        int version = t->dll_version;
        uchar *id_b = t->dll_id_b;

        if (t->tpl_id_found)
        {
            mfct = t->tpl_mfct;
            media = t->tpl_type;
            version = t->tpl_version;
            id_b = t->tpl_id_b;
        }

        warning("(meter) %s: meter detection could not find driver for "
                "id: %02x%02x%02x%02x mfct: (%s) %s (0x%02x) type: %s (0x%02x) ver: 0x%02x\n",
                name.c_str(),
                id_b[3], id_b[2], id_b[1], id_b[0],
                manufacturerFlag(mfct).c_str(),
                manufacturer(mfct).c_str(),
                mfct,
                mediaType(media, mfct).c_str(), media,
                version);


        warning("(meter) please consider opening an issue at https://github.com/weetmuts/wmbusmeters/\n");
        warning("(meter) to add support for this unknown mfct,media,version combination\n");
    }

    bool handleTelegram(AboutTelegram &about, vector<uchar> input_frame, bool simulated)
    {
        if (should_analyze_)
        {
            analyzeTelegram(about, input_frame, simulated);
            return true;
        }

        if (!hasMeters())
        {
            if (on_telegram_)
            {
                on_telegram_(about, input_frame);
            }
            return true;
        }

        bool handled = false;
        bool exact_id_match = false;

        string ids;
        for (auto &m : meters_)
        {
            bool h = m->handleTelegram(about, input_frame, simulated, &ids, &exact_id_match);
            if (h) handled = true;
        }

        // If not properly handled, and there was no exact id match.
        // then lets check if there is a template that can create a meter for it.
        if (!handled && !exact_id_match)
        {
            debug("(meter) no meter handled %s checking %d templates.\n", ids.c_str(), meter_templates_.size());
            // Not handled, maybe we have a template to create a new meter instance for this telegram?
            Telegram t;
            t.about = about;
            bool ok = t.parseHeader(input_frame);
            if (simulated) t.markAsSimulated();

            if (ok)
            {
                ids = t.idsc;
                for (auto &mi : meter_templates_)
                {
                    if (MeterCommonImplementation::isTelegramForMeter(&t, NULL, &mi))
                    {
                        // We found a match, make a copy of the meter info.
                        MeterInfo tmp = mi;
                        // Overwrite the wildcard pattern with the highest level id.
                        // The last id in the t.ids is the highest level id.
                        // For example: a telegram can have dll_id,tpl_id
                        // This will pick the tpl_id.
                        // Or a telegram can have a single dll_id,
                        // then the dll_id will be picked.
                        vector<string> tmp_ids;
                        tmp_ids.push_back(t.ids.back());
                        tmp.ids = tmp_ids;
                        tmp.idsc = t.ids.back();

                        if (tmp.driver == MeterDriver::AUTO)
                        {
                            // Look up the proper meter driver!
                            tmp.driver = pickMeterDriver(&t);
                            if (tmp.driver == MeterDriver::UNKNOWN)
                            {
                                if (should_analyze_ == false)
                                {
                                    // We are not analyzing, so warn here.
                                    warnForUnknownDriver(mi.name, &t);
                                }
                            }
                        }
                        // Now build a meter object with for this exact id.
                        auto meter = createMeter(&tmp);
                        addMeter(meter);
                        string idsc = toIdsCommaSeparated(t.ids);
                        verbose("(meter) used meter template %s %s %s to match %s\n",
                                mi.name.c_str(),
                                mi.idsc.c_str(),
                                toString(mi.driver).c_str(),
                                idsc.c_str());

                        if (is_daemon_)
                        {
                            notice("(wmbusmeters) started meter %d (%s %s %s)\n",
                                   meter->index(),
                                   mi.name.c_str(),
                                   tmp.idsc.c_str(),
                                   toString(mi.driver).c_str());
                        }
                        else
                        {
                            verbose("(meter) started meter %d (%s %s %s)\n",
                                   meter->index(),
                                   mi.name.c_str(),
                                   tmp.idsc.c_str(),
                                   toString(mi.driver).c_str());
                        }

                        bool match = false;
                        bool h = meter->handleTelegram(about, input_frame, simulated, &ids, &match);
                        if (!match)
                        {
                            // Oups, we added a new meter object tailored for this telegram
                            // but it still did not match! This is probably an error in wmbusmeters!
                            warning("(meter) newly created meter (%s %s %s) did not match telegram! ",
                                    "Please open an issue at https://github.com/weetmuts/wmbusmeters/\n",
                                    meter->name().c_str(), meter->idsc().c_str(), toString(meter->driver()).c_str());
                        }
                        else if (!h)
                        {
                            // Oups, we added a new meter object tailored for this telegram
                            // but it still did not handle it! This can happen if the wrong
                            // decryption key was used.
                            warning("(meter) newly created meter (%s %s %s) did not handle telegram!\n",
                                    meter->name().c_str(), meter->idsc().c_str(), toString(meter->driver()).c_str());
                        }
                        else
                        {
                            handled = true;
                        }
                    }
                }
            }
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

    void whenMeterUpdated(std::function<void(Telegram*t,Meter*)> cb)
    {
        on_meter_updated_ = cb;
    }

    void pollMeters(shared_ptr<BusManager> bus)
    {
        for (auto &m : meters_)
        {
            m->poll(bus);
        }
    }

    void analyzeEnabled(bool b, OutputFormat f)
    {
        should_analyze_ = b;
        analyze_format_ = f;
    }

    void analyzeTelegram(AboutTelegram &about, vector<uchar> &input_frame, bool simulated)
    {
        Telegram t;
        t.about = about;
        bool ok = t.parseHeader(input_frame);
        if (simulated) t.markAsSimulated();
        t.markAsBeingAnalyzed();

        if (!ok)
        {
            printf("Could not even analyze header, giving up.\n");
            return;
        }

        vector<MeterDriver> drivers;
#define X(mname,linkmode,info,type,cname) drivers.push_back(MeterDriver::type);
LIST_OF_METERS
#undef X

        MeterInfo mi;
        if (meter_templates_.size() > 0)
        {
            if (meter_templates_.size() > 1)
            {
                error("When analyzing you can only specify a single meter quadruple.\n");
            }
            if (meter_templates_[0].driver != MeterDriver::AUTO)
            {
                drivers.clear();
                drivers.push_back(meter_templates_[0].driver);
                mi = meter_templates_[0];
            }
        }

        // Overwrite the id with the id from the telegram to be analyzed.
        mi.ids.clear();
        mi.ids.push_back(t.ids.back());
        mi.idsc = t.ids.back();

        bool hide_output = drivers.size() > 1;
        bool printed = false;
        bool handled = false;
        MeterDriver best_driver {};
        // For the best driver we have:
        int best_content_length = 0;
        int best_understood_content_length = 0;


        for (MeterDriver dr : drivers)
        {
            if (dr == MeterDriver::AUTO) continue;
            if (dr == MeterDriver::UNKNOWN) continue;
            string driver_name = toString(dr);
            debug("Testing driver %s...\n", driver_name.c_str());
            mi.driver = dr;
            auto meter = createMeter(&mi);
            bool match = false;
            string id;
            bool h = meter->handleTelegram(about, input_frame, simulated, &id, &match, &t);
            if (!match)
            {

            }
            else if (!h)
            {
                // Oups, we added a new meter object tailored for this telegram
                // but it still did not handle it! This can happen if the wrong
                // decryption key was used. But it is ok if analyzing....
                debug("(meter) newly created meter (%s %s %s) did not handle telegram!\n",
                        meter->name().c_str(), meter->idsc().c_str(), toString(meter->driver()).c_str());
            }
            else
            {
                handled = true;
                int l = 0;
                int u = 0;
                OutputFormat of = analyze_format_;
                if (hide_output) of = OutputFormat::NONE;
                else printed = true;
                t.analyzeParse(of, &l, &u);
                verbose("(analyze) %s %d/%d\n", driver_name.c_str(), u, l);
                if (u > best_understood_content_length)
                {
                    // Understood so many bytes
                    best_understood_content_length = u;
                    // Out of this many bytes of content total.
                    best_content_length = l;
                    best_driver = dr;
                }
            }
        }
        if (handled)
        {
            MeterDriver auto_driver = pickMeterDriver(&t);
            string ad = toString(auto_driver);
            string bd = toString(best_driver);
            if (auto_driver != MeterDriver::UNKNOWN)
            {
                if (best_driver != auto_driver)
                {
                    printf("The automatic driver selection picks \"%s\" based on mfct/type/version!\n", ad.c_str());
                    printf("BUT the driver which matches most of the content is %s with %d/%d content bytes understood.\n",
                           bd.c_str(), best_understood_content_length, best_content_length);
                    mi.driver = auto_driver;
                }
                else
                {
                    printf("The automatic driver selection picked \"%s\" based on mfct/type/version!\n", ad.c_str());
                    printf("Which is also the best matching driver with %d/%d content bytes understood.\n",
                           best_understood_content_length, best_content_length);
                    mi.driver = best_driver;
                }
            }
            else
            {
                printf("No automatic driver selection could be found based on mfct/type/version!\n");
                printf("The driver which matches most of the content is %s with %d/%d content bytes understood.\n",
                       bd.c_str(), best_understood_content_length, best_content_length);
                mi.driver = best_driver;
            }

            if (!printed)
            {
                auto meter = createMeter(&mi);
                bool match = false;
                string id;
                meter->handleTelegram(about, input_frame, simulated, &id, &match, &t);
                int l = 0;
                int u = 0;
                t.analyzeParse(analyze_format_, &l, &u);
            }
        }
        else
        {
            printf("No suitable driver found.\n");
        }
    }

    MeterManagerImplementation(bool daemon) : is_daemon_(daemon) {}
    ~MeterManagerImplementation() {}
};

shared_ptr<MeterManager> createMeterManager(bool daemon)
{
    return shared_ptr<MeterManager>(new MeterManagerImplementation(daemon));
}

MeterCommonImplementation::MeterCommonImplementation(MeterInfo &mi,
                                                     MeterDriver driver) :
    driver_(driver), bus_(mi.bus), name_(mi.name)
{
    ids_ = mi.ids;
    idsc_ = toIdsCommaSeparated(ids_);

    if (mi.key.length() > 0)
    {
        hex2bin(mi.key, &meter_keys_.confidentiality_key);
    }
    for (auto s : mi.shells) {
        addShell(s);
    }
    for (auto j : mi.extra_constant_fields) {
        addExtraConstantField(j);
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

void MeterCommonImplementation::addExtraConstantField(string ecf)
{
    extra_constant_fields_.push_back(ecf);
}

vector<string> &MeterCommonImplementation::shellCmdlines()
{
    return shell_cmdlines_;
}

vector<string> &MeterCommonImplementation::meterExtraConstantFields()
{
    return extra_constant_fields_;
}

MeterDriver MeterCommonImplementation::driver()
{
    return driver_;
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

void MeterCommonImplementation::poll(shared_ptr<BusManager> bus)
{
}

vector<string>& MeterCommonImplementation::ids()
{
    return ids_;
}

string MeterCommonImplementation::idsc()
{
    return idsc_;
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

string MeterCommonImplementation::unixTimestampOfUpdate()
{
    char ut[40];
    memset(ut, 0, sizeof(ut));
    snprintf(ut, sizeof(ut)-1, "%lu", datetime_of_update_);
    return string(ut);
}

bool needsPolling(MeterDriver d)
{
#define X(mname,linkmodes,info,driver,cname) if (d == MeterDriver::driver && 0 != ((linkmodes) & MBUS_bit)) return true;
LIST_OF_METERS
#undef X
    return false;
}

string toString(MeterDriver mt)
{
#define X(mname,link,info,type,cname) if (mt == MeterDriver::type) return #mname;
LIST_OF_METERS
#undef X
    return "unknown";
}

MeterDriver toMeterDriver(string& t)
{
#define X(mname,linkmodes,info,type,cname) if (t == #mname) return MeterDriver::type;
LIST_OF_METERS
#undef X
    return MeterDriver::UNKNOWN;
}

LinkModeSet toMeterLinkModeSet(string& t)
{
#define X(mname,linkmodes,info,type,cname) if (t == #mname) return LinkModeSet(linkmodes);
LIST_OF_METERS
#undef X
    return LinkModeSet();
}

LinkModeSet toMeterLinkModeSet(MeterDriver d)
{
#define X(mname,linkmodes,info,driver,cname) if (d == MeterDriver::driver) return LinkModeSet(linkmodes);
LIST_OF_METERS
#undef X
    return LinkModeSet();
}

bool MeterCommonImplementation::isTelegramForMeter(Telegram *t, Meter *meter, MeterInfo *mi)
{
    string name;
    vector<string> ids;
    string idsc;
    MeterDriver driver;

    assert((meter && !mi) ||
           (!meter && mi));

    if (meter)
    {
        name = meter->name();
        ids = meter->ids();
        idsc = meter->idsc();
        driver = meter->driver();
    }
    else
    {
        name = mi->name;
        ids = mi->ids;
        idsc = mi->idsc;
        driver = mi->driver;
    }

    debug("(meter) %s: for me? %s in %s\n", name.c_str(), t->idsc.c_str(), idsc.c_str());

    bool used_wildcard = false;
    bool id_match = doesIdsMatchExpressions(t->ids, ids, &used_wildcard);

    if (!id_match) {
        // The id must match.
        debug("(meter) %s: not for me: not my id\n", name.c_str());
        return false;
    }

    bool valid_driver = isMeterDriverValid(driver, t->dll_mfct, t->dll_type, t->dll_version);
    if (!valid_driver && t->tpl_id_found)
    {
        valid_driver = isMeterDriverValid(driver, t->tpl_mfct, t->tpl_type, t->tpl_version);
    }

    if (!valid_driver && driver != MeterDriver::AUTO)
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
            if (t->beingAnalyzed() == false)
            {
                warning("(meter) %s: meter detection did not match the selected driver %s! correct driver is: %s\n"
                        "(meter) Not printing this warning again for id: %02x%02x%02x%02x mfct: (%s) %s (0x%02x) type: %s (0x%02x) ver: 0x%02x\n",
                        name.c_str(),
                        toString(driver).c_str(),
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
    }

    debug("(meter) %s: yes for me\n", name.c_str());
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

int MeterCommonImplementation::index()
{
    return index_;
}

void MeterCommonImplementation::setIndex(int i)
{
    index_ = i;
}

string MeterCommonImplementation::bus()
{
    return bus_;
}

void MeterCommonImplementation::triggerUpdate(Telegram *t)
{
    datetime_of_update_ = time(NULL);
    num_updates_++;
    for (auto &cb : on_update_) if (cb) cb(t, this);
    t->handled = true;
}

string concatAllFields(Meter *m, Telegram *t, char c, vector<Print> &prints, vector<Unit> &cs, bool hr,
                       vector<string> *extra_constant_fields)
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

string findField(string key, vector<string> *extra_constant_fields)
{
    key = key+"=";
    for (string ecf : *extra_constant_fields)
    {
        if (startsWith(ecf, key))
        {
            return ecf.substr(key.length());
        }
    }
    return "";
}

// Is the desired field one of the fields common to all meters and telegrams?
bool checkCommonField(string *buf, string field, Meter *m, Telegram *t, char c)
{
    if (field == "name")
    {
        *buf += m->name() + c;
        return true;
    }
    if (field == "id")
    {
        *buf += t->ids.back() + c;
        return true;
    }
    if (field == "timestamp")
    {
        *buf += m->datetimeOfUpdateHumanReadable() + c;
        return true;
    }
    if (field == "timestamp_lt")
    {
        *buf += m->datetimeOfUpdateHumanReadable() + c;
        return true;
    }
    if (field == "timestamp_utc")
    {
        *buf += m->datetimeOfUpdateRobot() + c;
        return true;
    }
    if (field == "timestamp_ut")
    {
        *buf += m->unixTimestampOfUpdate() + c;
        return true;
    }
    if (field == "device")
    {
        *buf += t->about.device + c;
        return true;
    }
    if (field == "rssi_dbm")
    {
        *buf += to_string(t->about.rssi_dbm) + c;
        return true;
    }

    return false;
}

// Is the desired field one of the meter printable fields?
bool checkPrintableField(string *buf, string field, Meter *m, Telegram *t, char c,
                         vector<Print> &prints, vector<Unit> &cs)
{
    for (Print p : prints)
    {
        if (p.getValueString)
        {
            // Strings are simply just print them.
            if (field == p.vname)
            {
                *buf += p.getValueString() + c;
                return true;
            }
        }
        else if (p.getValueDouble)
        {
            // Doubles have to be converted into the proper unit.
            string default_unit = unitToStringLowerCase(p.default_unit);
            string var = p.vname+"_"+default_unit;
            if (field == var)
            {
                // Default unit.
                *buf += valueToString(p.getValueDouble(p.default_unit), p.default_unit) + c;
                return true;
            }
            else
            {
                // Added conversion unit.
                Unit u = replaceWithConversionUnit(p.default_unit, cs);
                if (u != p.default_unit)
                {
                    string unit = unitToStringLowerCase(u);
                    string var = p.vname+"_"+unit;
                    if (field == var)
                    {
                        *buf += valueToString(p.getValueDouble(u), u) + c;
                        return true;
                    }
                }
            }
        }
    }

    return false;
}

// Is the desired field one of the constant fields?
bool checkConstantField(string *buf, string field, char c, vector<string> *extra_constant_fields)
{
    // Ok, lets look for extra constant fields and print any such static information.
    string v = findField(field, extra_constant_fields);
    if (v != "")
    {
        *buf += v + c;
        return true;
    }

    return false;
}


string concatFields(Meter *m, Telegram *t, char c, vector<Print> &prints, vector<Unit> &cs, bool hr,
                    vector<string> *selected_fields, vector<string> *extra_constant_fields)
{
    if (selected_fields == NULL || selected_fields->size() == 0)
    {
        return concatAllFields(m, t, c, prints, cs, hr, extra_constant_fields);
    }
    string buf = "";

    for (string field : *selected_fields)
    {
        bool handled = checkCommonField(&buf, field, m, t, c);
        if (handled) continue;

        handled = checkPrintableField(&buf, field, m, t, c, prints, cs);
        if (handled) continue;

        handled = checkConstantField(&buf, field, c, extra_constant_fields);
        if (handled) continue;

        if (!handled)
        {
            buf += "?"+field+"?"+c;
        }
    }
    if (buf.back() == c) buf.pop_back();
    return buf;
}

bool MeterCommonImplementation::handleTelegram(AboutTelegram &about, vector<uchar> input_frame,
                                               bool simulated, string *ids, bool *id_match, Telegram *out_analyzed)
{
    Telegram t;
    t.about = about;
    bool ok = t.parseHeader(input_frame);

    if (simulated) t.markAsSimulated();
    if (out_analyzed != NULL) t.markAsBeingAnalyzed();

    *ids = t.idsc;

    if (!ok || !isTelegramForMeter(&t, this, NULL))
    {
        // This telegram is not intended for this meter.
        return false;
    }

    *id_match = true;
    verbose("(meter) %s %s handling telegram from %s\n", name().c_str(), meterDriver().c_str(), t.ids.back().c_str());

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
    snprintf(log_prefix, 255, "(%s) log", meterDriver().c_str());
    logTelegram(t.original, t.frame, t.header_size, t.suffix_size);

    // Invoke meter specific parsing!
    processContent(&t);
    // All done....

    if (isDebugEnabled())
    {
        char log_prefix[256];
        snprintf(log_prefix, 255, "(%s)", meterDriver().c_str());
        t.explainParse(log_prefix, 0);
    }
    triggerUpdate(&t);
    if (out_analyzed != NULL) *out_analyzed = t;
    return true;
}

void MeterCommonImplementation::printMeter(Telegram *t,
                                           string *human_readable,
                                           string *fields, char separator,
                                           string *json,
                                           vector<string> *envs,
                                           vector<string> *extra_constant_fields,
                                           vector<string> *selected_fields)
{
    *human_readable = concatFields(this, t, '\t', prints_, conversions_, true, selected_fields, extra_constant_fields);
    *fields = concatFields(this, t, separator, prints_, conversions_, false, selected_fields, extra_constant_fields);

    string media;
    if (t->tpl_id_found)
    {
        media = mediaTypeJSON(t->tpl_type, t->tpl_mfct);
    }
    else if (t->ell_id_found)
    {
        media = mediaTypeJSON(t->ell_type, t->ell_mfct);
    }
    else
    {
        media = mediaTypeJSON(t->dll_type, t->dll_mfct);
    }

    string s;
    s += "{";
    s += "\"media\":\""+media+"\",";
    s += "\"meter\":\""+meterDriver()+"\",";
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
    for (string extra_field : meterExtraConstantFields())
    {
        s += ",";
        s += makeQuotedJson(extra_field);
    }
    for (string extra_field : *extra_constant_fields)
    {
        s += ",";
        s += makeQuotedJson(extra_field);
    }
    s += "}";
    *json = s;

    envs->push_back(string("METER_JSON=")+*json);
    if (t->ids.size() > 0)
    {
        envs->push_back(string("METER_ID=")+t->ids.back());
    }
    else
    {
        envs->push_back(string("METER_ID="));
    }
    envs->push_back(string("METER_NAME=")+name());
    envs->push_back(string("METER_MEDIA=")+media);
    envs->push_back(string("METER_TYPE=")+meterDriver());
    envs->push_back(string("METER_TIMESTAMP=")+datetimeOfUpdateRobot());
    envs->push_back(string("METER_TIMESTAMP_UTC=")+datetimeOfUpdateRobot());
    envs->push_back(string("METER_TIMESTAMP_UT=")+unixTimestampOfUpdate());
    envs->push_back(string("METER_TIMESTAMP_LT=")+datetimeOfUpdateHumanReadable());
    if (t->about.device != "")
    {
        envs->push_back(string("METER_DEVICE=")+t->about.device);
        envs->push_back(string("METER_RSSI_DBM=")+to_string(t->about.rssi_dbm));
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

    // If the configuration has supplied json_address=Roodroad 123
    // then the env variable METER_address will available and have the content "Roodroad 123"
    for (string add_json : meterExtraConstantFields())
    {
        envs->push_back(string("METER_")+add_json);
    }
    for (string extra_field : *extra_constant_fields)
    {
        envs->push_back(string("METER_")+extra_field);
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

double GasMeter::totalGasConsumption(Unit u) { return -NAN; }
bool  GasMeter::hasTotalGasConsumption() { return false; }
double GasMeter::targetGasConsumption(Unit u) { return -NAN; }
bool  GasMeter::hasTargetGasConsumption() { return false; }
double GasMeter::maxFlow(Unit u) { return -NAN; }
bool  GasMeter::hasMaxFlow() { return false; }
double GasMeter::flowTemperature(Unit u) { return -NAN; }
bool GasMeter::hasFlowTemperature() { return false; }
double GasMeter::externalTemperature(Unit u) { return -NAN; }
bool GasMeter::hasExternalTemperature() { return false; }

string GasMeter::statusHumanReadable() { return "-NAN"; }
string GasMeter::status() { return "-NAN"; }
string GasMeter::timeDry() { return "-NAN"; }
string GasMeter::timeReversed() { return "-NAN"; }
string GasMeter::timeLeaking() { return "-NAN"; }
string GasMeter::timeBursting() { return "-NAN"; }

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

void detectMeterDrivers(int manufacturer, int media, int version, vector<string> *drivers)
{
#define X(TY,MA,ME,VE) { if (manufacturer == MA && (media == ME || ME == -1) && (version == VE || VE == -1)) { drivers->push_back(toString(MeterDriver::TY)); }}
METER_DETECTION
#undef X
}

bool isMeterDriverValid(MeterDriver type, int manufacturer, int media, int version)
{
#define X(TY,MA,ME,VE) { if (type == MeterDriver::TY && manufacturer == MA && (media == ME || ME == -1) && (version == VE || VE == -1)) { return true; }}
METER_DETECTION
#undef X

    return false;
}

MeterDriver pickMeterDriver(Telegram *t)
{
    int manufacturer = t->dll_mfct;
    int media = t->dll_type;
    int version = t->dll_version;

    if (t->tpl_id_found)
    {
        manufacturer = t->tpl_mfct;
        media = t->tpl_type;
        version = t->tpl_version;
    }

#define X(TY,MA,ME,VE) { if (manufacturer == MA && (media == ME || ME == -1) && (version == VE || VE == -1)) { return MeterDriver::TY; }}
METER_DETECTION
#undef X
    return MeterDriver::UNKNOWN;
}

shared_ptr<Meter> createMeter(MeterInfo *mi)
{
    shared_ptr<Meter> newm;

    const char *keymsg = (mi->key[0] == 0) ? "not-encrypted" : "encrypted";

    switch (mi->driver)
    {
#define X(mname,link,info,driver,cname) \
        case MeterDriver::driver:                           \
        {                                                   \
            newm = create##cname(*mi);                      \
            newm->addConversions(mi->conversions);          \
            verbose("(meter) created \"%s\" \"" #mname "\" \"%s\" %s\n", \
                    mi->name.c_str(), mi->idsc.c_str(), keymsg);              \
            return newm;                                                \
        }                                                               \
        break;
LIST_OF_METERS
#undef X
    }
    return newm;
}

bool is_driver_extras(string t, MeterDriver *out_driver, string *out_extras)
{
    // piigth(jump=foo)
    // multical21

    size_t ps = t.find('(');
    size_t pe = t.find(')');

    size_t te = 0; // Position after type end.

    bool found_parentheses = (ps != string::npos && pe != string::npos);

    if (!found_parentheses)
    {
        // No brackets nor parentheses found, is t a known wmbus device? like im871a amb8465 etc....
        MeterDriver md = toMeterDriver(t);
        if (md == MeterDriver::UNKNOWN) return false;
        *out_driver = md;
        *out_extras = "";
        return true;
    }

    // Parentheses must be last.
    if (! (ps > 0 && ps < pe && pe == t.length()-1)) return false;
    te = ps;

    string type = t.substr(0, te);
    MeterDriver md = toMeterDriver(type);
    if (md == MeterDriver::UNKNOWN) return false;
    *out_driver = md;

    string extras = t.substr(ps+1, pe-ps-1);
    *out_extras = extras;

    return true;
}

string MeterInfo::str()
{
    string r;
    r += toString(driver);
    if (extras != "")
    {
        r += "("+extras+")";
    }
    r += ":";
    if (bus != "") r += bus+":";
    if (bps != 0) r += bps+":";
    if (!link_modes.empty()) r += link_modes.hr()+":";
    if (r.size() > 0) r.pop_back();

    return r;
}

bool MeterInfo::parse(string n, string d, string i, string k)
{
    clear();

    name = n;
    ids = splitMatchExpressions(i);
    key = k;
    bool driverextras_checked = false;
    bool bus_checked = false;
    bool bps_checked = false;
    bool link_modes_checked = false;

    // For the moment the colon : is forbidden in file names and commands.
    // It cannot occur in type,fq or bps.
    vector<string> parts = splitString(d, ':');

    // Example piigth:MAIN:2400
    //         multical21:c1
    //         telco:BUS2:c2
    // driver ( extras ) : bus_alias : bps : linkmodes

    for (auto& p : parts)
    {
        if (!driverextras_checked && is_driver_extras(p, &driver, &extras))
        {
            driverextras_checked = true;
        }
        else if (!bus_checked && isValidAlias(p) && !isValidBps(p) && !isValidLinkModes(p))
        {
            driverextras_checked = true;
            bus_checked = true;
            bus = p;
        }
        else if (!bps_checked && isValidBps(p) && !isValidLinkModes(p))
        {
            driverextras_checked = true;
            bus_checked = true;
            bps_checked = true;
            bps = atoi(p.c_str());
        }
        else if (!link_modes_checked && isValidLinkModes(p))
        {
            driverextras_checked = true;
            bus_checked = true;
            bps_checked = true;
            link_modes_checked = true;
            link_modes = parseLinkModes(p);
        }
        else
        {
            // Unknown part....
            return false;
        }
    }

    if (!link_modes_checked)
    {
        // No explicit link mode set, set to the default link modes
        // that the meter can transmit on.
        link_modes = toMeterLinkModeSet(driver);
    }

    return true;
}

bool isValidKey(string& key, MeterDriver mt)
{
    if (key.length() == 0) return true;
    if (key == "NOKEY") {
        key = "";
        return true;
    }
    if (mt == MeterDriver::IZAR ||
        mt == MeterDriver::HYDRUS)
    {
        // These meters can either be OMS compatible 128 bit key (32 hex).
        // Or using an older proprietary encryption with 64 bit keys (16 hex)
        if (key.length() != 16 && key.length() != 32) return false;
    }
    else
    {
        // OMS compliant meters have 128 bit AES keys (32 hex).
        // There is a deprecated DES mode, but I have not yet
        // seen any telegram using that mode.
        if (key.length() != 32) return false;
    }
    vector<uchar> tmp;
    return hex2bin(key, &tmp);
}
