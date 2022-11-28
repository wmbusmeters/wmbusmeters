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

#include"bus.h"
#include"config.h"
#include"meters.h"
#include"meters_common_implementation.h"
#include"units.h"
#include"wmbus.h"
#include"wmbus_utils.h"

#include<algorithm>
#include<cmath>
#include<limits>
#include<memory.h>
#include<numeric>
#include<stdexcept>
#include<time.h>


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

                        if (meter_info.driverName().str() == "auto")
                        {
                            // Look up the proper meter driver!
                            DriverInfo di = pickMeterDriver(&t);
                            if (di.name().str() == "")
                            {
                                if (should_analyze_ == false)
                                {
                                    // We are not analyzing, so warn here.
                                    warnForUnknownDriver(mi.name, &t);
                                }
                            }
                            else
                            {
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
                                mi.driverName().str().c_str(),
                                idsc.c_str());

                        if (is_daemon_)
                        {
                            notice("(wmbusmeters) started meter %d (%s %s %s)\n",
                                   meter->index(),
                                   mi.name.c_str(),
                                   meter_info.idsc.c_str(),
                                   mi.driverName().str().c_str());
                        }
                        else
                        {
                            verbose("(meter) started meter %d (%s %s %s)\n",
                                    meter->index(),
                                    mi.name.c_str(),
                                    meter_info.idsc.c_str(),
                                    mi.driverName().str().c_str());
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

        for (DriverInfo *ndr : allDrivers())
        {
            string driver_name = toString(*ndr);
            if (only != "")
            {
                if (driver_name != only) continue;
                return driver_name;
            }

            if (only == "" &&
                !isMeterDriverReasonableForMedia(driver_name, t.dll_type) &&
                !isMeterDriverReasonableForMedia(driver_name, t.tpl_type))
            {
                // Sanity check, skip this driver since it is not relevant for this media.
                continue;
            }

            debug("Testing driver %s...\n", driver_name.c_str());
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
                    best_driver = ndr->name().str();
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
        string using_driver;
        int using_length = 0;
        int using_understood = 0;

        // Driver that understands most of the telegram content.
        int best_length = 0;
        int best_understood = 0;
        string best_driver = findBestNewStyleDriver(mi, &best_length, &best_understood, t, about, input_frame, simulated, "");

        if (best_driver == "") best_driver = "unknown";

        mi.driver_name = DriverName(best_driver);

        // Default to best driver....
        using_driver = best_driver;
        using_length = best_length;
        using_understood = best_understood;

        // Unless the existing mapping from mfct/media/version to driver overrides best.
        DriverInfo auto_di = pickMeterDriver(&t);
        string auto_driver = auto_di.name().str();

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
            using_driver = findBestNewStyleDriver(mi, &force_length, &force_understood, t, about, input_frame, simulated,
                                                  force_driver);
            using_length = force_length;
            using_understood = force_understood;
        }

        mi.driver_name = using_driver;

        auto meter = createMeter(&mi);

        assert(meter != NULL);

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
