/*
 Copyright (C) 2017-2022 Fredrik Öhrström (gpl-3.0-or-later)

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


map<string, DriverInfo> all_registered_drivers_;
vector<DriverInfo> all_registered_drivers_list_;

bool DriverInfo::detect(uint16_t mfct, uchar type, uchar version)
{
    for (auto &dd : detect_)
    {
        if (dd.mfct == 0 && dd.type == 0 && dd.version == 0) continue; // Ignore drivers with no detection.
        if (dd.mfct == mfct && dd.type == type && dd.version == version) return true;
    }
    return false;
}

bool DriverInfo::isValidMedia(uchar type)
{
    for (auto &dd : detect_)
    {
        if (dd.type == type) return true;
    }
    return false;
}

bool DriverInfo::isCloseEnoughMedia(uchar type)
{
    for (auto &dd : detect_)
    {
        if (isCloseEnough(dd.type, type)) return true;
    }
    return false;
}

void DriverInfo::setExpectedELLSecurityMode(ELLSecurityMode dsm)
{
    // TODO should check that the telegram is encrypted using the same mode.
}

void DriverInfo::setExpectedTPLSecurityMode(TPLSecurityMode tsm)
{
    // TODO should check that the telegram is encrypted using the same mode.
}

bool registerDriver(function<void(DriverInfo&)> setup)
{
    DriverInfo di;
    setup(di);

    // Check that the driver name is unique.
    assert(all_registered_drivers_.count(di.name().str()) == 0);

    // Check that no other driver also triggers on the same detection values.
    for (auto &d : di.detect())
    {
        for (auto &p : all_registered_drivers_)
        {
            bool foo = p.second.detect(d.mfct, d.type, d.version);
            if (foo)
            {
                error("Internal error: driver %s tried to register the same auto detect combo as driver %s alread has taken!\n",
                      di.name().str().c_str(), p.second.name().str().c_str());
            }
        }
    }

    // Everything looks, good install this driver.
    all_registered_drivers_[di.name().str()] = di;
    all_registered_drivers_list_.push_back(di);

    // This code is invoked from the static initializers of DriverInfos when starting
    // wmbusmeters. Thus we do not yet know if the user has supplied --debug or similar setting.
    // To debug this you have to uncomment the printf below.
    // fprintf(stderr, "(STATIC) added driver: %s\n", n.c_str());
    return true;
}

bool lookupDriverInfo(string& driver, DriverInfo *out_di)
{
    if (all_registered_drivers_.count(driver) == 0)
    {
        return false;
    }

    *out_di = all_registered_drivers_[driver];
    return true;
}

struct MeterManagerImplementation : public virtual MeterManager
{
private:
    bool is_daemon_ {};
    bool should_analyze_ {};
    OutputFormat analyze_format_ {};
    string analyze_driver_;
    string analyze_key_;
    bool analyze_verbose_;
    vector<MeterInfo> meter_templates_;
    vector<shared_ptr<Meter>> meters_;
    vector<function<bool(AboutTelegram&,vector<uchar>)>> telegram_listeners_;
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
        if (meters_.size() < meter_templates_.size()) return false;

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
                        MeterInfo meter_info = mi;
                        // Overwrite the wildcard pattern with the highest level id.
                        // The last id in the t.ids is the highest level id.
                        // For example: a telegram can have dll_id,tpl_id
                        // This will pick the tpl_id.
                        // Or a telegram can have a single dll_id,
                        // then the dll_id will be picked.
                        vector<string> tmp_ids;
                        tmp_ids.push_back(t.ids.back());
                        meter_info.ids = tmp_ids;
                        meter_info.idsc = t.ids.back();

                        if (meter_info.driver == MeterDriver::AUTO)
                        {
                            // Look up the proper meter driver!
                            DriverInfo di = pickMeterDriver(&t);
                            if (di.driver() == MeterDriver::UNKNOWN && di.name().str() == "")
                            {
                                if (should_analyze_ == false)
                                {
                                    // We are not analyzing, so warn here.
                                    warnForUnknownDriver(mi.name, &t);
                                }
                            }
                            else
                            {
                                meter_info.driver = di.driver();
                                meter_info.driver_name = di.name();
                            }
                        }
                        // Now build a meter object with for this exact id.
                        auto meter = createMeter(&meter_info);
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
                                   meter_info.idsc.c_str(),
                                   toString(mi.driver).c_str());
                        }
                        else
                        {
                            verbose("(meter) started meter %d (%s %s %s)\n",
                                   meter->index(),
                                   mi.name.c_str(),
                                   meter_info.idsc.c_str(),
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
                                    meter->name().c_str(), meter->idsc().c_str(), meter->driverName().str().c_str());
                        }
                        else if (!h)
                        {
                            // Oups, we added a new meter object tailored for this telegram
                            // but it still did not handle it! This can happen if the wrong
                            // decryption key was used.
                            warning("(meter) newly created meter (%s %s %s) did not handle telegram!\n",
                                    meter->name().c_str(), meter->idsc().c_str(), meter->driverName().str().c_str());
                        }
                        else
                        {
                            handled = true;
                        }
                    }
                }
            }
        }
        for (auto f : telegram_listeners_)
        {
            f(about, input_frame);
        }
        if (isVerboseEnabled() && !handled)
        {
            verbose("(wmbus) telegram from %s ignored by all configured meters!\n", ids.c_str());
        }
        return handled;
    }

    void onTelegram(function<bool(AboutTelegram &about, vector<uchar>)> cb)
    {
        telegram_listeners_.push_back(cb);
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

    void analyzeEnabled(bool b, OutputFormat f, string force_driver, string key, bool verbose)
    {
        should_analyze_ = b;
        analyze_format_ = f;
        if (force_driver != "auto")
        {
            analyze_driver_ = force_driver;
        }
        analyze_key_ = key;
        analyze_verbose_ = verbose;
    }

    string findBestOldStyleDriver(MeterInfo &mi,
                                  int *best_length,
                                  int *best_understood,
                                  Telegram &t,
                                  AboutTelegram &about,
                                  vector<uchar> &input_frame,
                                  bool simulated,
                                  string force)
    {
        vector<MeterDriver> old_drivers;
#define X(mname,linkmode,info,type,cname) old_drivers.push_back(MeterDriver::type);
LIST_OF_METERS
#undef X

        string best_driver = "";
        for (MeterDriver odr : old_drivers)
        {
            if (odr == MeterDriver::AUTO) continue;
            if (odr == MeterDriver::UNKNOWN) continue;
            string driver_name = toString(odr);
            if (force != "")
            {
                if (driver_name != force) continue;
                return driver_name;
            }

            if (force == "" &&
                !isMeterDriverReasonableForMedia(odr, "", t.dll_type) &&
                !isMeterDriverReasonableForMedia(odr, "", t.tpl_type))
            {
                // Sanity check, skip this driver since it is not relevant for this media.
                continue;
            }

            debug("Testing old style driver %s...\n", driver_name.c_str());
            mi.driver = odr;
            mi.driver_name = DriverName("");

            auto meter = createMeter(&mi);

            bool match = false;
            string id;
            bool h = meter->handleTelegram(about, input_frame, simulated, &id, &match, &t);
            if (!match)
            {
                debug("no match!\n");
            }
            else if (!h)
            {
                // Oups, we added a new meter object tailored for this telegram
                // but it still did not handle it! This can happen if the wrong
                // decryption key was used. But it is ok if analyzing....
                debug("Newly created meter (%s %s %s) did not handle telegram!\n",
                      meter->name().c_str(), meter->idsc().c_str(), meter->driverName().str().c_str());
            }
            else
            {
                int l = 0;
                int u = 0;
                t.analyzeParse(OutputFormat::NONE, &l, &u);
                if (analyze_verbose_ && force == "") printf("(verbose) old %02d/%02d %s\n", u, l, driver_name.c_str());
                if (u > *best_understood)
                {
                    *best_understood = u;
                    *best_length = l;
                    best_driver = driver_name;
                    if (analyze_verbose_ && force == "") printf("(verbose) old best so far: %s %02d/%02d\n", best_driver.c_str(), u, l);
                }
            }
        }
        return best_driver;
    }

    string findBestNewStyleDriver(MeterInfo &mi,
                                  int *best_length,
                                  int *best_understood,
                                  Telegram &t,
                                  AboutTelegram &about,
                                  vector<uchar> &input_frame,
                                  bool simulated,
                                  string only)
    {
        string best_driver = "";

        for (DriverInfo ndr : all_registered_drivers_list_)
        {
            string driver_name = toString(ndr);
            if (only != "")
            {
                if (driver_name != only) continue;
                return driver_name;
            }

            if (only == "" &&
                !isMeterDriverReasonableForMedia(MeterDriver::AUTO, driver_name, t.dll_type) &&
                !isMeterDriverReasonableForMedia(MeterDriver::AUTO, driver_name, t.tpl_type))
            {
                // Sanity check, skip this driver since it is not relevant for this media.
                continue;
            }

            debug("Testing new style driver %s...\n", driver_name.c_str());
            mi.driver = MeterDriver::UNKNOWN;
            mi.driver_name = driver_name;

            auto meter = createMeter(&mi);

            bool match = false;
            string id;
            bool h = meter->handleTelegram(about, input_frame, simulated, &id, &match, &t);

            if (!match)
            {
                debug("no match!\n");
            }
            else if (!h)
            {
                // Oups, we added a new meter object tailored for this telegram
                // but it still did not handle it! This can happen if the wrong
                // decryption key was used. But it is ok if analyzing....
                debug("Newly created meter (%s %s %s) did not handle telegram!\n",
                      meter->name().c_str(), meter->idsc().c_str(), meter->driverName().str().c_str());
            }
            else
            {
                int l = 0;
                int u = 0;
                t.analyzeParse(OutputFormat::NONE, &l, &u);
                if (analyze_verbose_ && only == "") printf("(verbose) new %02d/%02d %s\n", u, l, driver_name.c_str());
                if (u > *best_understood)
                {
                    *best_understood = u;
                    *best_length = l;
                    best_driver = ndr.name().str();
                    if (analyze_verbose_ && only == "") printf("(verbose) new best so far: %s %02d/%02d\n", best_driver.c_str(), u, l);
                }
            }
        }
        return best_driver;
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

        if (meter_templates_.size() > 0)
        {
            error("You cannot specify a meter quadruple when analyzing.\n"
                  "Instead use --analyze=<format>:<driver>:<key>\n"
                  "where <formt> <driver> <key> are all optional.\n"
                  "E.g.        --analyze=terminal:multical21:001122334455667788001122334455667788\n"
                  "            --analyze=001122334455667788001122334455667788\n"
                  "            --analyze\n");
        }

        // Overwrite the id with the id from the telegram to be analyzed.
        MeterInfo mi;
        mi.key = analyze_key_;
        mi.ids.clear();
        mi.ids.push_back(t.ids.back());
        mi.idsc = t.ids.back();

        // This will be the driver that will actually decode and print with.
        string using_driver = "";
        int using_length = 0;
        int using_understood = 0;

        // Driver that understands most of the telegram content.
        string best_driver = "";
        int best_length = 0;
        int best_understood = 0;

        int old_best_length = 0;
        int old_best_understood = 0;
        string best_old_driver = findBestOldStyleDriver(mi, &old_best_length, &old_best_understood, t, about, input_frame, simulated, "");

        int new_best_length = 0;
        int new_best_understood = 0;
        string best_new_driver = findBestNewStyleDriver(mi, &new_best_length, &new_best_understood, t, about, input_frame, simulated, "");

        mi.driver = MeterDriver::UNKNOWN;
        mi.driver_name = DriverName("");

        // Use the existing mapping from mfct/media/version to driver.
        DriverInfo auto_di = pickMeterDriver(&t);
        string auto_driver = auto_di.name().str();
        if (auto_driver == "")
        {
            auto_driver = toString(auto_di.driver());
            if (auto_driver == "unknown")
            {
                auto_driver = "";
            }
        }

        // Will be non-empty if an explicit driver has been selected.
        string force_driver = analyze_driver_;
        int force_length = 0;
        int force_understood = 0;

        // If an auto driver is found and no other driver has been forced, use the auto driver.
        if (force_driver == "" && auto_driver != "")
        {
            force_driver = auto_driver;
        }

        if (force_driver != "")
        {
            using_driver = findBestOldStyleDriver(mi, &force_length, &force_understood, t, about, input_frame, simulated,
                                                  force_driver);

            if (using_driver != "")
            {
                mi.driver = toMeterDriver(using_driver);
                mi.driver_name = DriverName("");
                using_driver += "(driver should be upgraded)";
            }
            else
            {
                using_driver = findBestNewStyleDriver(mi, &force_length, &force_understood, t, about, input_frame, simulated,
                                                      force_driver);
                mi.driver_name = using_driver;
                mi.driver = MeterDriver::UNKNOWN;
            }
            using_length = force_length;
            using_understood = force_understood;
        }

        if (old_best_understood > new_best_understood)
        {
            best_length = old_best_length;
            best_understood = old_best_understood;
            best_driver = best_old_driver+"(driver should be upgraded)";
            if (using_driver == "")
            {
                mi.driver = toMeterDriver(best_old_driver);
                mi.driver_name = DriverName("");
                using_driver = best_driver;
                using_length = best_length;
                using_understood = best_understood;
            }
        }
        else if (new_best_understood >= old_best_understood)
        {
            best_length = new_best_length;
            best_understood = new_best_understood;
            best_driver = best_new_driver;
            if (using_driver == "")
            {
                mi.driver_name = best_new_driver;
                mi.driver = MeterDriver::UNKNOWN;
                using_driver = best_new_driver;
                using_length = best_length;
                using_understood = best_understood;
            }
        }

        auto meter = createMeter(&mi);

        bool match = false;
        string id;

        meter->handleTelegram(about, input_frame, simulated, &id, &match, &t);

        int u = 0;
        int l = 0;

        string output = t.analyzeParse(analyze_format_, &u, &l);

        string hr, fields, json;
        vector<string> envs, more_json, selected_fields;

        meter->printMeter(&t, &hr, &fields, '\t', &json,
                          &envs, &more_json, &selected_fields, true);

        if (auto_driver == "")
        {
            auto_driver = "not found!";
        }

        printf("Auto driver  : %s\n", auto_driver.c_str());
        printf("Best driver  : %s %02d/%02d\n", best_driver.c_str(), best_understood, best_length);
        printf("Using driver : %s %02d/%02d\n", using_driver.c_str(), using_understood, using_length);

        printf("%s\n", output.c_str());

        printf("%s\n", json.c_str());
    }

    MeterManagerImplementation(bool daemon) : is_daemon_(daemon) {}
    ~MeterManagerImplementation() {}
};

shared_ptr<MeterManager> createMeterManager(bool daemon)
{
    return shared_ptr<MeterManager>(new MeterManagerImplementation(daemon));
}

MeterCommonImplementation::MeterCommonImplementation(MeterInfo &mi,
                                                     string driver) :
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

MeterCommonImplementation::MeterCommonImplementation(MeterInfo &mi,
                                                     DriverInfo &di) :
    type_(di.type()), driver_(di.name().str()), driver_name_(di.name()), bus_(mi.bus), name_(mi.name)
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

    link_modes_.unionLinkModeSet(di.linkModes());
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
    return toMeterDriver(driver_);
}

DriverName MeterCommonImplementation::driverName()
{
    if (driver_name_.str() == "")
    {
        return DriverName(toString(driver()));
    }
    return driver_name_;
}

void MeterCommonImplementation::setMeterType(MeterType mt)
{
    type_ = mt;
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
    prints_.push_back(
        FieldInfo(vname,
                  vquantity,
                  defaultUnitForQuantity(vquantity),
                  NoDifVifKey,
                  VifScaling::Auto,
                  MeasurementType::Unknown,
                  ValueInformation::None,
                  AnyStorageNr,
                  AnyTariffNr,
                  IndexNr(1),
                  help,
                  field,
                  json,
                  false,
                  field_name,
                  getValueFunc,
                  NULL,
                  NULL,
                  NULL,
                  NULL,
                  NULL,
                  NoLookup
            ));
}

void MeterCommonImplementation::addPrint(string vname, Quantity vquantity, Unit unit,
                                         function<double(Unit)> getValueFunc, string help, bool field, bool json)
{
    string default_unit = unitToStringLowerCase(defaultUnitForQuantity(vquantity));
    string field_name = vname+"_"+default_unit;
    fields_.push_back(field_name);
    prints_.push_back(
        FieldInfo(vname,
                  vquantity,
                  unit,
                  NoDifVifKey,
                  VifScaling::Auto,
                  MeasurementType::Unknown,
                  ValueInformation::None,
                  AnyStorageNr,
                  AnyTariffNr,
                  IndexNr(1),
                  help,
                  field,
                  json,
                  false,
                  field_name,
                  getValueFunc,
                  NULL,
                  NULL,
                  NULL,
                  NULL,
                  NULL,
                  NoLookup
            ));
}

void MeterCommonImplementation::addPrint(string vname, Quantity vquantity,
                                         function<string()> getValueFunc,
                                         string help, bool field, bool json)
{
    prints_.push_back(
        FieldInfo(vname,
                  vquantity,
                  defaultUnitForQuantity(vquantity),
                  NoDifVifKey,
                  VifScaling::Auto,
                  MeasurementType::Unknown,
                  ValueInformation::None,
                  AnyStorageNr,
                  AnyTariffNr,
                  IndexNr(1),
                  help,
                  field,
                  json,
                  false,
                  vname,
                  NULL,
                  getValueFunc,
                  NULL,
                  NULL,
                  NULL,
                  NULL,
                  NoLookup
            ));
}

void MeterCommonImplementation::addFieldWithExtractor(
    string vname,
    Quantity vquantity,
    DifVifKey dif_vif_key,
    VifScaling vif_scaling,
    MeasurementType mt,
    ValueInformation vi,
    StorageNr s,
    TariffNr t,
    IndexNr i,
    int print_properties,
    string help,
    function<void(Unit,double)> setValueFunc,
    function<double(Unit)> getValueFunc)
{
    string default_unit = unitToStringLowerCase(defaultUnitForQuantity(vquantity));
    string field_name = vname+"_"+default_unit;
    fields_.push_back(field_name);

    // Compose the extract function.
    function<bool(FieldInfo *p,Meter *m, Telegram *t)> extract =
              [](FieldInfo *fi, Meter *m, Telegram *t)
              {
                  bool found = false;
                  string key = fi->difVifKey().str();
                  int offset {};
                  if (key == "")
                  {
                      // Search for key.
                      bool ok = findKeyWithNr(fi->measurementType(),
                                              fi->valueInformation(),
                                              fi->storageNr().intValue(),
                                              fi->tariffNr().intValue(),
                                              fi->indexNr().intValue(),
                                              &key,
                                              &t->values);
                      if (!ok) return false;
                  }
                  double extracted_double_value = NAN;
                  if (extractDVdouble(&t->values,
                                      key,
                                      &offset,
                                      &extracted_double_value,
                                      fi->vifScaling() == VifScaling::Auto ||
                                      fi->vifScaling() == VifScaling::AutoSigned,
                                      fi->vifScaling() == VifScaling::NoneSigned ||
                                      fi->vifScaling() == VifScaling::AutoSigned))
                  {
                      Unit decoded_unit = fi->defaultUnit();
                      if (fi->valueInformation() != ValueInformation::Any &&
                          fi->valueInformation() != ValueInformation::AnyVolumeVIF &&
                          fi->valueInformation() != ValueInformation::AnyEnergyVIF &&
                          fi->valueInformation() != ValueInformation::AnyPowerVIF &&
                          fi->valueInformation() != ValueInformation::None)
                      {
                          decoded_unit = toDefaultUnit(fi->valueInformation());
                      }
                      fi->setValueDouble(decoded_unit, extracted_double_value);
                      t->addMoreExplanation(offset, fi->renderJson(&m->conversions()));
                      found = true;
                  }
                  return found;
              };

    prints_.push_back(
        FieldInfo(vname,
                  vquantity,
                  defaultUnitForQuantity(vquantity),
                  dif_vif_key,
                  vif_scaling,
                  mt,
                  vi,
                  s,
                  t,
                  i,
                  help,
                  (print_properties & PrintProperty::FIELD) != 0,
                  (print_properties & PrintProperty::JSON) != 0,
                  (print_properties & PrintProperty::IMPORTANT) != 0,
                  field_name,
                  getValueFunc,
                  NULL,
                  setValueFunc,
                  NULL,
                  extract,
                  NULL,
                  NoLookup
            ));
}

void MeterCommonImplementation::addField(
    string vname,
    Quantity vquantity,
    int print_properties,
    string help,
    function<void(Unit,double)> setValueFunc,
    function<double(Unit)> getValueFunc)
{
    string default_unit = unitToStringLowerCase(defaultUnitForQuantity(vquantity));
    string field_name = vname+"_"+default_unit;
    fields_.push_back(field_name);

    prints_.push_back(
        FieldInfo(vname,
                  vquantity,
                  defaultUnitForQuantity(vquantity),
                  DifVifKey(""),
                  VifScaling::None,
                  MeasurementType::Unknown,
                  ValueInformation::Volume,
                  StorageNr(0),
                  TariffNr(0),
                  0,
                  help,
                  (print_properties & PrintProperty::FIELD) != 0,
                  (print_properties & PrintProperty::JSON) != 0,
                  (print_properties & PrintProperty::IMPORTANT) != 0,
                  field_name,
                  getValueFunc,
                  NULL,
                  setValueFunc,
                  NULL,
                  NULL,
                  NULL,
                  NoLookup
            ));
}

void MeterCommonImplementation::addStringFieldWithExtractor(
    string vname,
    Quantity vquantity,
    DifVifKey dif_vif_key,
    MeasurementType mt,
    ValueInformation vi,
    StorageNr s,
    TariffNr t,
    IndexNr i,
    int print_properties,
    string help,
    function<void(string)> setValueFunc,
    function<string()> getValueFunc)
{
    string default_unit = unitToStringLowerCase(defaultUnitForQuantity(vquantity));
    string field_name = vname+"_"+default_unit;
    fields_.push_back(field_name);

    // Compose the extract function.
    function<bool(FieldInfo *p,Meter *m, Telegram *t)> extract =
                  [](FieldInfo *fi, Meter *m, Telegram *t)
                  {
                      bool found = false;
                      string key = fi->difVifKey().str();
                      int offset {};
                      if (key == "")
                      {
                          // Search for key.
                          bool ok = findKeyWithNr(fi->measurementType(),
                                                  fi->valueInformation(),
                                                  fi->storageNr().intValue(),
                                                  fi->tariffNr().intValue(),
                                                  fi->indexNr().intValue(),
                                                  &key,
                                                  &t->values);
                          if (!ok) return false;
                      }
                      if (fi->valueInformation() == ValueInformation::DateTime)
                      {
                          struct tm datetime;
                          extractDVdate(&t->values, key, &offset, &datetime);
                          string extracted_device_date_time = strdatetime(&datetime);
                          fi->setValueString(extracted_device_date_time);
                          t->addMoreExplanation(offset, fi->renderJsonText());
                          found = true;
                      }
                      else if (fi->valueInformation() == ValueInformation::Date)
                      {
                          struct tm date;
                          extractDVdate(&t->values, key, &offset, &date);
                          string extracted_device_date = strdate(&date);
                          fi->setValueString(extracted_device_date);
                          t->addMoreExplanation(offset, fi->renderJsonText());
                          found = true;
                      }
                      else if (fi->valueInformation() == ValueInformation::EnhancedIdentification ||
                               fi->valueInformation() == ValueInformation::FabricationNo)
                      {
                          string extracted_id;
                          extractDVReadableString(&t->values, key, &offset, &extracted_id);
                          fi->setValueString(extracted_id);
                          t->addMoreExplanation(offset, fi->renderJsonText());
                          found = true;
                      }
                      else
                      {
                          error("Internal error: Cannot extract text string from vif %s in %s:%d\n",
                                toString(fi->valueInformation()),
                                __FILE__, __LINE__);

                      }
                      return found;
                  };

    prints_.push_back(
        FieldInfo(vname,
                  vquantity,
                  defaultUnitForQuantity(vquantity),
                  dif_vif_key,
                  VifScaling::None,
                  mt,
                  vi,
                  s,
                  t,
                  i,
                  help,
                  (print_properties & PrintProperty::FIELD) != 0,
                  (print_properties & PrintProperty::JSON) != 0,
                  (print_properties & PrintProperty::IMPORTANT) != 0,
                  field_name,
                  NULL,
                  getValueFunc,
                  NULL,
                  setValueFunc,
                  NULL,
                  extract,
                  NoLookup
            ));
}

void MeterCommonImplementation::addStringFieldWithExtractorAndLookup(
    string vname,
    Quantity vquantity,
    DifVifKey dif_vif_key,
    MeasurementType mt,
    ValueInformation vi,
    StorageNr s,
    TariffNr t,
    IndexNr i,
    int print_properties,
    string help,
    function<void(string)> setValueFunc,
    function<string()> getValueFunc,
    Translate::Lookup lookup)
{
    string default_unit = unitToStringLowerCase(defaultUnitForQuantity(vquantity));
    string field_name = vname+"_"+default_unit;
    fields_.push_back(field_name);

    // Compose the extract function.
    function<bool(FieldInfo *p,Meter *m, Telegram *t)> extract =
                  [](FieldInfo *fi, Meter *m, Telegram *t)
                  {
                      bool found = false;
                      string key = fi->difVifKey().str();
                      int offset {};
                      if (key == "")
                      {
                          // Search for key.
                          bool ok = findKeyWithNr(fi->measurementType(),
                                                  fi->valueInformation(),
                                                  fi->storageNr().intValue(),
                                                  fi->tariffNr().intValue(),
                                                  fi->indexNr().intValue(),
                                                  &key,
                                                  &t->values);
                          if (!ok) return false;
                      }
                      uint64_t extracted_bits {};
                      if (extractDVlong(&t->values, key, &offset, &extracted_bits))
                      {
                          string translated_bits = fi->lookup().translate(extracted_bits);
                          fi->setValueString(translated_bits);
                          t->addMoreExplanation(offset, fi->renderJsonText());
                          found = true;
                      }
                      return found;
                  };

    prints_.push_back(
        FieldInfo(vname,
                  vquantity,
                  defaultUnitForQuantity(vquantity),
                  dif_vif_key,
                  VifScaling::None,
                  mt,
                  vi,
                  s,
                  t,
                  i,
                  help,
                  (print_properties & PrintProperty::FIELD) != 0,
                  (print_properties & PrintProperty::JSON) != 0,
                  (print_properties & PrintProperty::IMPORTANT) != 0,
                  field_name,
                  NULL,
                  getValueFunc,
                  NULL,
                  setValueFunc,
                  NULL,
                  extract,
                  lookup
            ));
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

vector<FieldInfo> MeterCommonImplementation::prints()
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

bool needsPolling(MeterDriver d, DriverName& dn)
{
    if (d != MeterDriver::UNKNOWN && d != MeterDriver::AUTO)
    {
#define X(mname,linkmodes,info,driver,cname) if (d == MeterDriver::driver && 0 != ((linkmodes) & MBUS_bit)) return true;
LIST_OF_METERS
#undef X
    return false;
    }

    if (all_registered_drivers_.count(dn.str()) == 0) return false;

    DriverInfo& di = all_registered_drivers_[dn.str()];

    // Return true for MBUS,S2,C2,T2 meters. Currently only mbus is implemented.
    return di.linkModes().has(LinkMode::MBUS);
}

const char *toString(MeterType type)
{
#define X(tname) if (type == MeterType::tname) return #tname;
LIST_OF_METER_TYPES
#undef X
    return "unknown";
}

string toString(MeterDriver mt)
{
#define X(mname,link,info,type,cname) if (mt == MeterDriver::type) return #mname;
LIST_OF_METERS
#undef X
    return "unknown";
}

string toString(DriverInfo &di)
{
    if (di.driver() != MeterDriver::UNKNOWN &&
        di.driver() != MeterDriver::AUTO) return toString(di.driver());
    return di.name().str();
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

string concatAllFields(Meter *m, Telegram *t, char c, vector<FieldInfo> &prints, vector<Unit> &cs, bool hr,
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
    for (FieldInfo p : prints)
    {
        if (p.field())
        {
            if (p.hasGetValueDouble())
            {
                Unit u = replaceWithConversionUnit(p.defaultUnit(), cs);
                double v = p.getValueDouble(u);
                if (hr) {
                    s += valueToString(v, u);
                    s += " "+unitToStringHR(u);
                } else {
                    s += to_string(v);
                }
            }
            if (p.hasGetValueString())
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
                         vector<FieldInfo> &prints, vector<Unit> &cs)
{
    for (FieldInfo p : prints)
    {
        if (p.hasGetValueString())
        {
            // Strings are simply just print them.
            if (field == p.vname())
            {
                *buf += p.getValueString() + c;
                return true;
            }
        }
        else if (p.hasGetValueDouble())
        {
            // Doubles have to be converted into the proper unit.
            string default_unit = unitToStringLowerCase(p.defaultUnit());
            string var = p.vname()+"_"+default_unit;
            if (field == var)
            {
                // Default unit.
                *buf += valueToString(p.getValueDouble(p.defaultUnit()), p.defaultUnit()) + c;
                return true;
            }
            else
            {
                // Added conversion unit.
                Unit u = replaceWithConversionUnit(p.defaultUnit(), cs);
                if (u != p.defaultUnit())
                {
                    string unit = unitToStringLowerCase(u);
                    string var = p.vname()+"_"+unit;
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


string concatFields(Meter *m, Telegram *t, char c, vector<FieldInfo> &prints, vector<Unit> &cs, bool hr,
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
        if (out_analyzed != NULL) *out_analyzed = t;
        // Ignoring telegram since it could not be parsed.
        return false;
    }

    char log_prefix[256];
    snprintf(log_prefix, 255, "(%s) log", meterDriver().c_str());
    logTelegram(t.original, t.frame, t.header_size, t.suffix_size);

    // Invoke standardized field extractors!
    processFieldExtractors(&t);
    // Invoke tailor made meter specific parsing!
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

void MeterCommonImplementation::processFieldExtractors(Telegram *t)
{
    for (auto &fi : prints_)
    {
        fi.performExtraction(this, t);
    }
}

void MeterCommonImplementation::processContent(Telegram *t)
{
}


FieldInfo *MeterCommonImplementation::findFieldInfo(string vname)
{
    FieldInfo *found = NULL;
    for (FieldInfo &p : prints_)
    {
        if (p.vname() == vname)
        {
            found = &p;
            break;
        }
    }

    return found;
}

string MeterCommonImplementation::renderJsonOnlyDefaultUnit(string vname)
{
    FieldInfo *fi = findFieldInfo(vname);

    if (fi == NULL) return "unknown field "+vname;
    return fi->renderJsonOnlyDefaultUnit();
}

string FieldInfo::renderJsonOnlyDefaultUnit()
{
    return renderJson(NULL);
}

string FieldInfo::renderJsonText()
{
    return renderJson(NULL);
}

string FieldInfo::renderJson(vector<Unit> *conversions)
{
    string s;

    string default_unit = unitToStringLowerCase(defaultUnit());
    string var = vname();
    if (hasGetValueString())
    {
        s += "\""+var+"\":\""+getValueString()+"\"";
    }
    else if (hasGetValueDouble())
    {
        s += "\""+var+"_"+default_unit+"\":"+valueToString(getValueDouble(defaultUnit()), defaultUnit());

        if (conversions != NULL)
        {
            Unit u = replaceWithConversionUnit(defaultUnit(), *conversions);
            if (u != defaultUnit())
            {
                string unit = unitToStringLowerCase(u);
                // Appending extra conversion unit.
                s += ",\""+var+"_"+unit+"\":"+valueToString(getValueDouble(u), u);
            }
        }
    }
    else
    {
        s = "?";
    }

    return s;
}

void MeterCommonImplementation::printMeter(Telegram *t,
                                           string *human_readable,
                                           string *fields, char separator,
                                           string *json,
                                           vector<string> *envs,
                                           vector<string> *extra_constant_fields,
                                           vector<string> *selected_fields,
                                           bool pretty_print_json)
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

    string indent = "";
    string newline = "";

    if (pretty_print_json)
    {
        indent = "    ";
        newline ="\n";
    }
    string s;
    s += "{"+newline;
    s += indent+"\"media\":\""+media+"\","+newline;
    s += indent+"\"meter\":\""+meterDriver()+"\","+newline;
    s += indent+"\"name\":\""+name()+"\","+newline;
    if (t->ids.size() > 0)
    {
        s += indent+"\"id\":\""+t->ids.back()+"\","+newline;
    }
    else
    {
        s += indent+"\"id\":\"\","+newline;
    }
    for (FieldInfo& p : prints_)
    {
        if (p.json())
        {
            s += indent+p.renderJson(&conversions())+","+newline;
        }
    }
    s += indent+"\"timestamp\":\""+datetimeOfUpdateRobot()+"\"";

    if (t->about.device != "")
    {
        s += ","+newline;
        s += indent+"\"device\":\""+t->about.device+"\","+newline;
        s += indent+"\"rssi_dbm\":"+to_string(t->about.rssi_dbm);
    }
    for (string extra_field : meterExtraConstantFields())
    {
        s += ","+newline;
        s += indent+makeQuotedJson(extra_field);
    }
    for (string extra_field : *extra_constant_fields)
    {
        s += ","+newline;
        s += indent+makeQuotedJson(extra_field);
    }
    s += newline;
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

    for (FieldInfo p : prints_)
    {
        if (p.json())
        {
            string default_unit = unitToStringUpperCase(p.defaultUnit());
            string var = p.vname();
            std::transform(var.begin(), var.end(), var.begin(), ::toupper);
            if (p.hasGetValueString()) {
                string envvar = "METER_"+var+"="+p.getValueString();
                envs->push_back(envvar);
            }
            if (p.hasGetValueDouble()) {
                string envvar = "METER_"+var+"_"+default_unit+"="+valueToString(p.getValueDouble(p.defaultUnit()), p.defaultUnit());
                envs->push_back(envvar);

                Unit u = replaceWithConversionUnit(p.defaultUnit(), conversions_);
                if (u != p.defaultUnit())
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

    for (auto &p : all_registered_drivers_)
    {
        if (p.second.detect(manufacturer, media, version))
        {
            drivers->push_back(p.second.name().str());
        }
    }
}

bool isMeterDriverValid(MeterDriver type, int manufacturer, int media, int version)
{
#define X(TY,MA,ME,VE) { if (type == MeterDriver::TY && manufacturer == MA && (media == ME || ME == -1) && (version == VE || VE == -1)) { return true; }}
METER_DETECTION
#undef X

    for (auto &p : all_registered_drivers_)
    {
        if (p.second.detect(manufacturer, media, version))
        {
            return true;
        }
    }

    return false;
}

bool isMeterDriverReasonableForMedia(MeterDriver type, string driver_name, int media)
{
    if (media == 0x37) return false;  // Skip converter meter side since they do not give any useful information.
    if (driver_name == "")
    {
#define X(TY,MA,ME,VE) { if (type == MeterDriver::TY  && isCloseEnough(media, ME)) { return true; }}
METER_DETECTION
#undef X
    }

    for (auto &p : all_registered_drivers_)
    {
        if (p.first == driver_name && p.second.isValidMedia(media))
        {
            return true;
        }
    }

    return false;
}

vector<DriverInfo>& allRegisteredDrivers()
{
    return all_registered_drivers_list_;
}

DriverInfo pickMeterDriver(Telegram *t)
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

    for (auto &p : all_registered_drivers_)
    {
        if (p.second.detect(manufacturer, media, version))
        {
            return p.second;
        }
    }

    return MeterDriver::UNKNOWN;
}

shared_ptr<Meter> createMeter(MeterInfo *mi)
{
    shared_ptr<Meter> newm;

    const char *keymsg = (mi->key[0] == 0) ? "not-encrypted" : "encrypted";

    if (all_registered_drivers_.count(mi->driver_name.str()) != 0)
    {
        DriverInfo& di = all_registered_drivers_[mi->driver_name.str()];
        shared_ptr<Meter> newm = di.construct(*mi);
        newm->addConversions(mi->conversions);
        verbose("(meter) constructed \"%s\" \"%s\" \"%s\" %s\n",
                mi->name.c_str(),
                di.name().str().c_str(),
                mi->idsc.c_str(),
                keymsg);
        return newm;
    }

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

bool is_driver_extras(string t, MeterDriver *out_driver, DriverName *out_driver_name, string *out_extras)
{
    // piigth(jump=foo)
    // multical21
    DriverInfo di;
    size_t ps = t.find('(');
    size_t pe = t.find(')');

    size_t te = 0; // Position after type end.

    bool found_parentheses = (ps != string::npos && pe != string::npos);

    if (!found_parentheses)
    {
        if (lookupDriverInfo(t, &di))
        {
            *out_driver_name = di.name();
            // We found a registered driver.
            *out_driver = MeterDriver::AUTO; // To go away!
            *out_extras = "";
            return true;
        }
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

    bool found = lookupDriverInfo(type, &di);

    MeterDriver md = toMeterDriver(type);
    if (found)
    {
        *out_driver_name = di.name();
    }
    else
    {
        if (md == MeterDriver::UNKNOWN) return false;
    }
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
        if (!driverextras_checked && is_driver_extras(p, &driver, &driver_name, &extras))
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


void FieldInfo::performExtraction(Meter *m, Telegram *t)
{
    if (extract_double_)
    {
        extract_double_(this, m, t);
    }

    if (extract_string_)
    {
        extract_string_(this, m, t);
    }
}

DriverName MeterInfo::driverName()
{
    if (driver_name.str() == "")
    {
        return DriverName(toString(driver));
    }
    return driver_name;
}
