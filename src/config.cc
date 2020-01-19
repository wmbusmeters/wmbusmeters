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
    key = eatToSkipWhitespace(buf, i, '=', 64, &eof, &err);
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
    string name;
    string type;
    string id;
    string key;
    string linkmodes;
    vector<string> shells;
    vector<string> jsons;

    debug("(config) loading meter file %s\n", file.c_str());
    for (;;) {
        pair<string,string> p = getNextKeyValue(buf, i);

        if (p.first == "") break;

        if (p.first == "name") name = p.second;
        else
        if (p.first == "type") type = p.second;
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
            shells.push_back(p.second);
        }
        else
        if (startsWith(p.first, "json_"))
        {
            string keyvalue = p.first.substr(5)+"="+p.second;
            jsons.push_back(keyvalue);
        }
        else
            warning("Found invalid key \"%s\" in meter config file\n", p.first.c_str());

        if (p.first != "key") {
            debug("(config) %s=%s\n", p.first.c_str(), p.second.c_str());
        }
    }
    MeterType mt = toMeterType(type);
    bool use = true;

    LinkModeSet modes;
    size_t colon = type.find(':');
    if (colon != string::npos)
    {
        // The config can be supplied after the type, like this:
        // apator162:c1
        string modess = type.substr(colon+1);
        type = type.substr(0, colon);
        mt = toMeterType(type);
        if (mt == MeterType::UNKNOWN) {
            warning("Not a valid meter type \"%s\"\n", type.c_str());
            use = false;
        }
        modes = parseLinkModes(modess);
        LinkModeSet default_modes = toMeterLinkModeSet(type);
        if (!default_modes.hasAll(modes))
        {
            string want = modes.hr();
            string has = default_modes.hr();
            error("(cmdline) cannot set link modes to: %s because meter %s only transmits on: %s\n",
                  want.c_str(), type.c_str(), has.c_str());
        }

        string modeshr = modes.hr();
        debug("(cmdline) setting link modes to %s for meter %s\n",
              modeshr.c_str(), name.c_str());
    }
    else {
        modes = toMeterLinkModeSet(type);
    }

    if (mt == MeterType::UNKNOWN) {
        warning("Not a valid meter type \"%s\"\n", type.c_str());
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
        c->meters.push_back(MeterInfo(name, type, id, key, modes, shells, jsons));
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
    else if (loglevel == "silent") { c->silence = true; }
    else if (loglevel == "normal") { }
    else {
        warning("No such log level: \"%s\"\n", loglevel.c_str());
    }
}

void handleDevice(Configuration *c, string device)
{
    // device can be:
    // /dev/ttyUSB00
    // auto
    // rtlwmbus:/usr/bin/rtl_sdr -f 868.9M -s 1600000 - | /usr/bin/rtl_wmbus
    // simulation....txt (read telegrams from file)
    size_t p = device.find (':');
    if (p != string::npos)
    {
        c->device_extra = device.substr(p+1);
        c->device = device.substr(0,p);
    } else {
        c->device = device;
    }
}

void handleListenTo(Configuration *c, string mode)
{
    LinkModeSet lms = parseLinkModes(mode.c_str());
    if (lms.bits() == 0) {
        error("Unknown link mode \"%s\"!\n", mode.c_str());
    }
    if (c->link_mode_configured) {
        error("You have already specified a link mode!\n");
    }
    c->listen_to_link_modes = lms;
    c->link_mode_configured = true;
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

void handleReopenAfter(Configuration *c, string s)
{
    if (s.length() >= 1)
    {
        c->reopenafter = parseTime(s.c_str());
        if (c->reopenafter <= 0)
        {
            warning("Not a valid time to reopen after. \"%s\"\n", s.c_str());
        }
    }
    else
    {
        warning("Reopen after must be a valid number of seconds.\n");
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
    const char *tok = strtok(buf, ",");
    while (tok != NULL)
    {
        Unit u = toUnit(tok);
        if (u == Unit::Unknown)
        {
            warning("(warning) not a valid conversion unit: %s\n", tok);
        }
        c->conversions.push_back(u);
        tok = strtok(NULL, ",");
    }
}

void handleShell(Configuration *c, string cmdline)
{
    c->shells.push_back(cmdline);
}

void handleJson(Configuration *c, string json)
{
    c->jsons.push_back(json);
}

unique_ptr<Configuration> loadConfiguration(string root, string device_override, string listento_override)
{
    Configuration *c = new Configuration;

    // JSon is default when configuring from config files.
    c->json = true;

    vector<char> global_conf;
    bool ok = loadFile(root+"/etc/wmbusmeters.conf", &global_conf);
    global_conf.push_back('\n');

    if (!ok) exit(1);

    auto i = global_conf.begin();

    for (;;) {
        auto p = getNextKeyValue(global_conf, i);

        if (p.first == "") break;
        if (p.first == "loglevel") handleLoglevel(c, p.second);
        else if (p.first == "device") handleDevice(c, p.second);
        else if (p.first == "listento") handleListenTo(c, p.second);
        else if (p.first == "logtelegrams") handleLogtelegrams(c, p.second);
        else if (p.first == "meterfiles") handleMeterfiles(c, p.second);
        else if (p.first == "meterfilesaction") handleMeterfilesAction(c, p.second);
        else if (p.first == "meterfilesnaming") handleMeterfilesNaming(c, p.second);
        else if (p.first == "meterfilestimestamp") handleMeterfilesTimestamp(c, p.second);
        else if (p.first == "logfile") handleLogfile(c, p.second);
        else if (p.first == "format") handleFormat(c, p.second);
        else if (p.first == "reopenafter") handleReopenAfter(c, p.second);
        else if (p.first == "separator") handleSeparator(c, p.second);
        else if (p.first == "addconversions") handleConversions(c, p.second);
        else if (p.first == "shell") handleShell(c, p.second);
        else if (startsWith(p.first, "json_"))
        {
            string s = p.first.substr(5);
            string keyvalue = s+"="+p.second;
            handleJson(c, keyvalue);
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
        debug("(config) overriding device with \"%s\"\n", device_override.c_str());
        handleDevice(c, device_override);
    }
    if (listento_override != "")
    {
        debug("(config) overriding listento with \"%s\"\n", listento_override.c_str());           handleListenTo(c, listento_override);
    }

    return unique_ptr<Configuration>(c);
}

LinkModeCalculationResult calculateLinkModes(Configuration *config, WMBus *wmbus)
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
        debug("(config) meter %s link mode(s): %s\n", m.type.c_str(), meter.c_str());
    }
    string metersu = meters_union.hr();
    debug("(config) all possible link modes that the meters might transmit on: %s\n", metersu.c_str());
    if (meters_union.bits() == 0) {
        if (!config->link_mode_configured)
        {
            string msg;
            strprintf(msg,"(config) No meters supplied. You must supply which link modes to listen to. Eg. --listento=<modes>");
            debug("%s\n", msg.c_str());
            return { LinkModeCalculationResultType::NoMetersMustSupplyModes , msg};
        }
    }
    if (!config->link_mode_configured)
    {
        // A listen_to link mode has not been set explicitly. Pick a listen_to link
        // mode that is supported by the wmbus dongle and works for the meters.
        config->listen_to_link_modes = wmbus->supportedLinkModes();
        config->listen_to_link_modes.disjunctionLinkModeSet(meters_union);
        if (!wmbus->canSetLinkModes(config->listen_to_link_modes))
        {
            // The automatically calculated link modes cannot be set in the dongle.
            // Ie the dongle needs help....
            string msg;
            strprintf(msg,"(config) Automatic deduction of which link mode to listen to failed since the meters might transmit using: %s\n"
                      "(config) But the dongle can only listen to: %s Please supply the exact link mode(s) to listen to, eg: --listento=<mode>\n"
                      "(config) and/or specify the expected transmit modes for the meters, eg: apator162:t1\n"
                      "(config) or use a dongle that can listen to all the required link modes at the same time.",
                      metersu.c_str(), dongle.c_str());

            debug("%s\n", msg.c_str());
            return { LinkModeCalculationResultType::AutomaticDeductionFailed , msg};
        }
        config->link_mode_configured = true;
    }
    else
    {
        string listen = config->listen_to_link_modes.hr();
        debug("(config) explicitly listening to: %s\n", listen.c_str());
    }

    string listen = config->listen_to_link_modes.hr();
    if (!wmbus->canSetLinkModes(config->listen_to_link_modes))
    {
        string msg;
        strprintf(msg, "(config) You have specified to listen to the link modes: %s but the dongle can only listen to: %s",
                  listen.c_str(), dongle.c_str());
        debug("%s\n", msg.c_str());
        return { LinkModeCalculationResultType::DongleCannotListenTo , msg};
    }

    if (!config->listen_to_link_modes.hasAll(meters_union))
    {
        string msg;
        strprintf(msg, "(config) You have specified to listen to the link modes: %s but the meters might transmit on: %s\n"
                  "(config) Therefore you might miss telegrams! Please specify the expected transmit mode for the meters, eg: apator162:t1\n"
                  "(config) Or use a dongle that can listen to all the required link modes at the same time.",
                  listen.c_str(), metersu.c_str());
        debug("%s\n", msg.c_str());
        return { LinkModeCalculationResultType::MightMissTelegrams, msg};
    }

    debug("(config) listen link modes calculated to be: %s\n", listen.c_str());

    return { LinkModeCalculationResultType::Success, "" };
}
