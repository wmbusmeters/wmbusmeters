/*
 Copyright (C) 2019-2020 Fredrik Öhrström

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
#include"units.h"

#include<vector>
#include<string>
#include<string.h>

using namespace std;

pair<string,string> getNextKeyValue(vector<char> &buf, vector<char>::iterator &i)
{
    bool eof, err;
    string key, value;
    if (*i == '#')
    {
        string comment = eatToSkipWhitespace(buf, i, '\n', 4096, &eof, &err);
        return { comment, "" };
    }
    key = eatToSkipWhitespace(buf, i, '=', 4096, &eof, &err);
    if (eof || err) goto nomore;
    value = eatToSkipWhitespace(buf, i, '\n', 4096, &eof, &err);
    if (err) goto nomore;

    return { key, value };

    nomore:

    return { "", "" };
}

void parseMeterConfig(Configuration *c, vector<char> &buf, string file)
{
    auto i = buf.begin();
    string bus;
    string name;
    string driver = "auto";
    string id;
    string key = "";
    string linkmodes;
    int bps {};
    vector<string> telegram_shells;
    vector<string> alarm_shells;
    vector<string> extra_constant_fields;

    debug("(config) loading meter file %s\n", file.c_str());
    for (;;) {
        pair<string,string> p = getNextKeyValue(buf, i);

        if (p.first == "") break;

        // If the key starts with # then the line is a comment. Ignore it.
        if (p.first.length() > 0 && p.first[0] == '#') continue;

        if (p.first == "bus")
        {
            // Instead of specifying the exact mbus device /dev/ttyUSB17 for each meter,
            // you specify the alias for the bus. Assuming you specified
            // device=main=/dev/ttyUSB17:mbus:2400
            // in the wmbusmeters.conf file, then
            // you should specify bus=main in the meter file
            // to use the main bus.
            //
            // For dual communication wmbus meters, you also have to specify
            // the alias to the wmbus device that should be used to transmit to the meter.
            bus = p.second;
            if (!isValidAlias(bus))
            {
                warning("Found invalid meter bus \"%s\" in meter config file, skipping meter.\n", bus.c_str());
                return;
            }
        }
        if (p.first == "name")
        {
            name = p.second;
            if (name.find(":") != string::npos)
            {
                // Oups, names are not allowed to contain the :
                warning("Found invalid meter name \"%s\" in meter config file, must not contain a ':', skipping meter.\n", name.c_str());
                return;
            }
        }
        else
        if (p.first == "type") driver = p.second;
        else
        if (p.first == "driver") driver = p.second;
        else
        if (p.first == "id") id = p.second;
        else
        if (p.first == "key")
        {
            key = p.second;
            debug("(config) key=<notprinted>\n");
        }
        else
        if (p.first == "shell") {
            telegram_shells.push_back(p.second);
        }
        else
        if (p.first == "alarmshell") {
            alarm_shells.push_back(p.second);
        }
        else
        if (startsWith(p.first, "json_") ||
            startsWith(p.first, "field_"))
        {
            int off = 5;
            if (startsWith(p.first, "field_")) { off = 6; }
            string keyvalue = p.first.substr(off)+"="+p.second;
            extra_constant_fields.push_back(keyvalue);
        }
        else
            warning("Found invalid key \"%s\" in meter config file\n", p.first.c_str());

        if (p.first != "key") {
            debug("(config) %s=%s\n", p.first.c_str(), p.second.c_str());
        }
    }
    bool use = true;

    LinkModeSet modes;
    MeterDriver mt = toMeterDriver(driver);
    size_t colon = driver.find(':');
    if (colon == string::npos)
    {
        // No suffixes to control the driver.
        modes = toMeterLinkModeSet(driver);
    }
    else
    {
        // The config can be supplied after the driver, like this:
        // apator162:c1
        // or
        // pith:mainbus:2400
        string modess = driver.substr(colon+1);
        driver = driver.substr(0, colon);
        mt = toMeterDriver(driver);
        if (mt == MeterDriver::UNKNOWN) {
            warning("Not a valid meter driver \"%s\"\n", driver.c_str());
            use = false;
        }
        modes = parseLinkModes(modess);
        LinkModeSet default_modes = toMeterLinkModeSet(driver);
        if (!modes.empty() && !default_modes.hasAll(modes))
        {
            string want = modes.hr();
            string has = default_modes.hr();
            error("(cmdline) cannot set link modes to: %s because meter %s only transmits on: %s\n",
                  want.c_str(), driver.c_str(), has.c_str());
        }

        string modeshr = modes.hr();
        debug("(cmdline) setting link modes to %s for meter %s\n",
              modeshr.c_str(), name.c_str());
    }

    if (mt == MeterDriver::UNKNOWN) {
        warning("Not a valid meter driver \"%s\"\n", driver.c_str());
        use = false;
    }
    if (!isValidMatchExpressions(id, true)) {
        warning("Not a valid meter id nor a valid meter match expression \"%s\"\n", id.c_str());
        use = false;
    }
    if (!isValidKey(key, mt)) {
        warning("Not a valid meter key \"%s\"\n", key.c_str());
        use = false;
    }
    if (use) {
        vector<string> ids = splitMatchExpressions(id);
        c->meters.push_back(MeterInfo(bus, name, mt, "", ids, key, modes, bps, telegram_shells, extra_constant_fields));
    }

    return;
}

void handleLoglevel(Configuration *c, string loglevel)
{
    if (loglevel == "verbose") { c->verbose = true; }
    else if (loglevel == "debug")
    {
        c->debug = true;
        // Kick in debug immediately.
        debugEnabled(c->debug);
    }
    else if (loglevel == "trace")
    {
        c->trace = true;
        // Kick in trace immediately.
        traceEnabled(c->trace);
    }
    else if (loglevel == "silent") { c->silent = true; }
    else if (loglevel == "normal") { }
    else {
        warning("No such log level: \"%s\"\n", loglevel.c_str());
    }
}

void handleInternalTesting(Configuration *c, string value)
{
    if (value == "true")
    {
        c->internaltesting = true;
    }
    else if (value == "false")
    {
        c->internaltesting = false;
    }
    else {
        warning("Internaltesting should be either true or false, not \"%s\"\n", value.c_str());
    }
}

void handleIgnoreDuplicateTelegrams(Configuration *c, string value)
{
    if (value == "true")
    {
        c->ignore_duplicate_telegrams = true;
    }
    else if (value == "false")
    {
        c->ignore_duplicate_telegrams = false;
    }
    else {
        warning("ignoreduplicates should be either true or false, not \"%s\"\n", value.c_str());
    }
}

void handleResetAfter(Configuration *c, string s)
{
    if (s.length() >= 1)
    {
        c->resetafter = parseTime(s.c_str());
        if (c->resetafter <= 0)
        {
            warning("Not a valid time to reset wmbus devices after. \"%s\"\n", s.c_str());
        }
    }
    else
    {
        warning("Reset after must be a valid number of seconds.\n");
    }
}

bool handleDevice(Configuration *c, string devicefile)
{
    SpecifiedDevice specified_device;
    bool ok = specified_device.parse(devicefile);
    if (!ok && SpecifiedDevice::isLikelyDevice(devicefile))
    {
        error("Not a valid device \"%s\"\n", devicefile.c_str());
    }

    if (!ok) return false;

    // Number the devices
    specified_device.index = c->supplied_bus_devices.size();

    if (specified_device.type == WMBusDeviceType::DEVICE_MBUS)
    {
        if (!specified_device.linkmodes.empty())
        {
            error("An mbus device must not have linkmode set. \"%s\"\n", devicefile.c_str());
        }
    }

    if (specified_device.linkmodes.empty())
    {
        // No linkmode set, but if simulation, stdin and file,
        // then assume that it will produce telegrams on all linkmodes.
        if (specified_device.is_simulation || specified_device.is_stdin || specified_device.is_file)
        {
            // Essentially link mode calculations are now irrelevant.
            specified_device.linkmodes.addLinkMode(LinkMode::Any);
        }
        else
        if (specified_device.type == WMBusDeviceType::DEVICE_RTLWMBUS ||
            specified_device.type == WMBusDeviceType::DEVICE_RTL433)
        {
            c->all_device_linkmodes_specified.addLinkMode(LinkMode::C1);
            c->all_device_linkmodes_specified.addLinkMode(LinkMode::T1);
        }
    }

    c->all_device_linkmodes_specified.unionLinkModeSet(specified_device.linkmodes);

    if (specified_device.is_stdin ||
        specified_device.is_file ||
        specified_device.is_simulation ||
        specified_device.command != "")
    {
        if (c->single_device_override)
        {
            error("You can only specify one stdin or one file or one command!\n");
        }
        if (c->use_auto_device_detect)
        {
            error("You cannot mix auto with stdin or a file.\n");
        }
        if (specified_device.is_simulation) c->simulation_found = true;
        c->single_device_override = true;
    }

    if (specified_device.type == WMBusDeviceType::DEVICE_AUTO)
    {
        c->use_auto_device_detect = true;
        c->auto_device_linkmodes = specified_device.linkmodes;

#if defined(__APPLE__) && defined(__MACH__)
        error("You cannot use auto on macosx. You must specify the device tty or rtlwmbus.\n");
#endif
    }
    else
    {
        c->supplied_bus_devices.push_back(specified_device);
        if (specified_device.type == WMBusDeviceType::DEVICE_MBUS)
        {
            c->num_mbus_devices++;
        }
        else
        {
            c->num_wmbus_devices++;
        }
    }

    return true;
}

bool handleDoNotProbe(Configuration *c, string devicefile)
{
    c->do_not_probe_ttys.insert(devicefile);
    return true;
}

void handleListenTo(Configuration *c, string mode)
{
    LinkModeSet lms = parseLinkModes(mode.c_str());
    if (lms.empty())
    {
        error("Unknown link modes \"%s\"!\n", mode.c_str());
    }
    if (!c->default_device_linkmodes.empty())
    {
        error("You have already specified the default link modes!\n");
    }

    c->default_device_linkmodes = lms;
}

void handleLogtelegrams(Configuration *c, string logtelegrams)
{
    if (logtelegrams == "true") { c->logtelegrams = true; }
    else if (logtelegrams == "false") { c->logtelegrams = false;}
    else {
        warning("No such logtelegrams setting: \"%s\"\n", logtelegrams.c_str());
    }
}

void handleMeterfiles(Configuration *c, string meterfiles)
{
    if (meterfiles.length() > 0)
    {
        c->meterfiles_dir = meterfiles;
        c->meterfiles = true;
        if (!checkIfDirExists(c->meterfiles_dir.c_str())) {
            warning("Cannot write meter files into dir \"%s\"\n", c->meterfiles_dir.c_str());
        }
    }
}

void handleMeterfilesAction(Configuration *c, string meterfilesaction)
{
    if (meterfilesaction == "overwrite")
    {
        c->meterfiles_action = MeterFileType::Overwrite;
    } else if (meterfilesaction == "append")
    {
        c->meterfiles_action = MeterFileType::Append;
    } else {
        warning("No such meter file action \"%s\"\n", meterfilesaction.c_str());
    }
}

void handleMeterfilesNaming(Configuration *c, string type)
{
    if (type == "name")
    {
        c->meterfiles_naming = MeterFileNaming::Name;
    }
    else if (type == "id")
    {
        c->meterfiles_naming = MeterFileNaming::Id;
    }
    else if (type == "name-id")
    {
        c->meterfiles_naming = MeterFileNaming::NameId;
    }
    else
    {
        warning("No such meter file naming \"%s\"\n", type.c_str());
    }
}

void handleMeterfilesTimestamp(Configuration *c, string type)
{
    if (type == "day")
    {
        c->meterfiles_timestamp = MeterFileTimestamp::Day;
    }
    else if (type == "hour")
    {
        c->meterfiles_timestamp = MeterFileTimestamp::Hour;
    }
    else if (type == "minute")
    {
        c->meterfiles_timestamp = MeterFileTimestamp::Minute;
    }
    else if (type == "micros")
    {
        c->meterfiles_timestamp = MeterFileTimestamp::Micros;
    }
    else if (type == "never")
    {
        c->meterfiles_timestamp = MeterFileTimestamp::Never;
    }
    else
    {
        warning("No such meter file timestamp \"%s\"\n", type.c_str());
    }
}

void handleLogfile(Configuration *c, string logfile)
{
    if (logfile.length() > 0)
    {
        c->use_logfile = true;
        c->logfile = logfile;
    }
}

void handleFormat(Configuration *c, string format)
{
    if (format == "hr")
    {
        c->json = false;
        c->fields = false;
    } else if (format == "json")
    {
        c->json = true;
        c->fields = false;
    }
    else if (format == "fields")
    {
        c->json = false;
        c->fields = true;
        c->separator = ';';
    } else {
        warning("Unknown output format: \"%s\"\n", format.c_str());
    }
}

void handleAlarmTimeout(Configuration *c, string s)
{
    if (s.length() >= 1)
    {
        c->alarm_timeout = parseTime(s.c_str());
        if (c->alarm_timeout <= 0)
        {
            warning("Not a valid time for alarm timeout. \"%s\"\n", s.c_str());
        }
    }
    else
    {
        warning("Alarm timeout must be a valid number of seconds.\n");
    }
}

void handleAlarmExpectedActivity(Configuration *c, string s)
{
    if (!isValidTimePeriod(s))
    {
        warning("Not a valid time period string. \"%s\"\n", s.c_str());
    }
    else
    {
        c->alarm_expected_activity = s;
    }
}

void handleSeparator(Configuration *c, string s)
{
    if (s.length() == 1) {
        c->separator = s[0];
    } else {
        warning("Separator must be a single character.\n");
    }
}

void handleConversions(Configuration *c, string s)
{
    char buf[s.length()+1];
    strcpy(buf, s.c_str());
    char *saveptr  {};
    const char *tok = strtok_r(buf, ",", &saveptr);
    while (tok != NULL)
    {
        Unit u = toUnit(tok);
        if (u == Unit::Unknown)
        {
            warning("(warning) not a valid conversion unit: %s\n", tok);
        }
        c->conversions.push_back(u);
        tok = strtok_r(NULL, ",", &saveptr);
    }
}

void handleLogTimestamps(Configuration *c, string ts)
{
    if (ts == "never")
    {
        c->addtimestamps = AddLogTimestamps::Never;
    }
    else if (ts == "always")
    {
        c->addtimestamps = AddLogTimestamps::Always;
    }
    else if (ts == "important")
    {
        c->addtimestamps = AddLogTimestamps::Important;
    }
    else
    {
        warning("(warning) No such timestamp setting \"%s\" possible values are: never always important\n",
                ts.c_str());
    }
}

void handleSelectedFields(Configuration *c, string s)
{
    if (c->selected_fields.size() > 0)
    {
        warning("(warning) selectfields already used! Ignoring selectfields %s", s.c_str());
        return;
    }
    char buf[s.length()+1];
    strcpy(buf, s.c_str());
    char *saveptr {};
    const char *tok = strtok_r(buf, ",", &saveptr);
    while (tok != NULL)
    {
        c->selected_fields.push_back(tok);
        tok = strtok_r(NULL, ",", &saveptr);
    }
}

void handleShell(Configuration *c, string cmdline)
{
    c->telegram_shells.push_back(cmdline);
}

void handleAlarmShell(Configuration *c, string cmdline)
{
    c->alarm_shells.push_back(cmdline);
}

void handleExtraConstantField(Configuration *c, string field)
{
    c->extra_constant_fields.push_back(field);
}

shared_ptr<Configuration> loadConfiguration(string root, string device_override, string listento_override)
{
    Configuration *c = new Configuration;

    // JSon is default when configuring from config files.
    c->json = true;

    vector<char> global_conf;
    string conf_file = root+"/etc/wmbusmeters.conf";
    debug("(config) loading %s\n", conf_file.c_str());
    bool ok = loadFile(conf_file, &global_conf);
    global_conf.push_back('\n');

    if (!ok) exit(1);

    auto i = global_conf.begin();

    for (;;) {
        auto p = getNextKeyValue(global_conf, i);

        debug("(config) \"%s\" \"%s\"\n", p.first.c_str(), p.second.c_str());
        if (p.first == "") break;
        // If the key starts with # then the line is a comment. Ignore it.
        if (p.first.length() > 0 && p.first[0] == '#') continue;
        if (p.first == "loglevel") handleLoglevel(c, p.second);
        else if (p.first == "internaltesting") handleInternalTesting(c, p.second);
        else if (p.first == "ignoreduplicates") handleIgnoreDuplicateTelegrams(c, p.second);
        else if (p.first == "device") handleDevice(c, p.second);
        else if (p.first == "donotprobe") handleDoNotProbe(c, p.second);
        else if (p.first == "listento") handleListenTo(c, p.second);
        else if (p.first == "logtelegrams") handleLogtelegrams(c, p.second);
        else if (p.first == "meterfiles") handleMeterfiles(c, p.second);
        else if (p.first == "meterfilesaction") handleMeterfilesAction(c, p.second);
        else if (p.first == "meterfilesnaming") handleMeterfilesNaming(c, p.second);
        else if (p.first == "meterfilestimestamp") handleMeterfilesTimestamp(c, p.second);
        else if (p.first == "logfile") handleLogfile(c, p.second);
        else if (p.first == "format") handleFormat(c, p.second);
        else if (p.first == "alarmtimeout") handleAlarmTimeout(c, p.second);
        else if (p.first == "alarmexpectedactivity") handleAlarmExpectedActivity(c, p.second);
        else if (p.first == "separator") handleSeparator(c, p.second);
        else if (p.first == "addconversions") handleConversions(c, p.second);
        else if (p.first == "logtimestamps") handleLogTimestamps(c, p.second);
        else if (p.first == "selectfields") handleSelectedFields(c, p.second);
        else if (p.first == "shell") handleShell(c, p.second);
        else if (p.first == "resetafter") handleResetAfter(c, p.second);
        else if (p.first == "alarmshell") handleAlarmShell(c, p.second);
        else if (startsWith(p.first, "json_") ||
                 startsWith(p.first, "field_"))
        {
            int off = 5;
            if (startsWith(p.first, "field_")) { off = 6; }
            string s = p.first.substr(off);
            string keyvalue = s+"="+p.second;
            handleExtraConstantField(c, keyvalue);
        }
        else
        {
            warning("No such key: %s\n", p.first.c_str());
        }
    }

    vector<string> meters;
    listFiles(root+"/etc/wmbusmeters.d", &meters);

    for (auto& f : meters)
    {
        vector<char> meter_conf;
        string file = root+"/etc/wmbusmeters.d/"+f;
        loadFile(file.c_str(), &meter_conf);
        meter_conf.push_back('\n');
        parseMeterConfig(c, meter_conf, file);
    }

    if (device_override != "")
    {
        // There is an override, therefore we
        // drop any already loaded devices from the config file.
        c->use_auto_device_detect = false;
        c->supplied_bus_devices.clear();

        if (startsWith(device_override, "/dev/rtlsdr"))
        {
            debug("(config) use rtlwmbus instead of raw device %s\n", device_override.c_str());
            device_override = "rtlwmbus";
        }
        debug("(config) overriding device with \"%s\"\n", device_override.c_str());
        handleDevice(c, device_override);
    }
    if (listento_override != "")
    {
        debug("(config) overriding listento with \"%s\"\n", listento_override.c_str());
        handleListenTo(c, listento_override);
    }

    return shared_ptr<Configuration>(c);
}

LinkModeCalculationResult calculateLinkModes(Configuration *config, WMBus *wmbus, bool link_modes_matter)
{
    int n = wmbus->numConcurrentLinkModes();
    string num = to_string(n);
    if (n == 0) num = "any combination";
    string dongles = wmbus->supportedLinkModes().hr();
    string dongle;
    strprintf(dongle, "%s of %s", num.c_str(), dongles.c_str());

    // Calculate the possible listen_to linkmodes for this collection of meters.
    LinkModeSet meters_union = UNKNOWN_bit;
    for (auto &m : config->meters)
    {
        meters_union.unionLinkModeSet(m.link_modes);
        string meter = m.link_modes.hr();
        debug("(config) meter %s link mode(s): %s\n", toString(m.driver).c_str(), meter.c_str());
    }
    string metersu = meters_union.hr();
    debug("(config) all possible link modes that the meters might transmit on: %s\n", metersu.c_str());
    if (meters_union.empty())
    {
        if (link_modes_matter && config->all_device_linkmodes_specified.empty())
        {
            string msg;
            strprintf(msg,"(config) No meters supplied. You must supply which link modes to listen to. 22 Eg. auto:t1");
            debug("%s\n", msg.c_str());
            return { LinkModeCalculationResultType::NoMetersMustSupplyModes , msg};
        }
        return { LinkModeCalculationResultType::Success, "" };
    }
    string all_lms = config->all_device_linkmodes_specified.hr();
    verbose("(config) all specified link modes: %s\n", all_lms.c_str());

    /*
    string listen = config->linkmodes.hr();
    if (!wmbus->canSetLinkModes(config->linkmodes))
    {
        string msg;
        strprintf(msg, "(config) You have specified to listen to the link modes: %s but the dongle can only listen to: %s",
                  listen.c_str(), dongle.c_str());
        debug("%s\n", msg.c_str());
        return { LinkModeCalculationResultType::DongleCannotListenTo , msg};
    }
    */
    /*
    string listen = config->linkmodes.hr();
    if (!wmbus->canSetLinkModes(config->linkmodes))
    {
        string msg;
        strprintf(msg, "(config) You have specified to listen to the link modes: %s but the dongle can only listen to: %s",
                  listen.c_str(), dongle.c_str());
        debug("%s\n", msg.c_str());
        return { LinkModeCalculationResultType::DongleCannotListenTo , msg};
    }
    */
    if (!config->all_device_linkmodes_specified.hasAll(meters_union))
    {
        string msg;
        strprintf(msg, "(config) You have specified to listen to the link modes: %s but the meters might transmit on: %s\n"
                  "(config) Therefore you might miss telegrams! Please specify the expected transmit mode for the meters, eg: apator162:t1\n"
                  "(config) Or use a dongle that can listen to all the required link modes at the same time.",
                  all_lms.c_str(), metersu.c_str());
        debug("%s\n", msg.c_str());
        return { LinkModeCalculationResultType::MightMissTelegrams, msg};
    }

    return { LinkModeCalculationResultType::Success, "" };
}
