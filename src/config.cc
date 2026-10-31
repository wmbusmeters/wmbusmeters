/*
 Copyright (C) 2019-2026 Fredrik Öhrström (gpl-3.0-or-later)

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
#include"drivers.h"
#include"meters.h"
#include"units.h"
#include"xmq.h"

#include"utils/fs.h"

#include<vector>
#include<string>
#include<string.h>

using namespace std;

pair<string,string> getNextKeyValue(vector<char> &buf, vector<char>::iterator &i)
{
    // Enhanced parsing:
    // - Read whole lines, skip empty & comment (#) lines.
    // - Support CRLF (strip trailing '\r').
    // - Require '='; warn and skip malformed lines.
    // - Trim leading/trailing whitespace only (internal spaces preserved).
    bool eof = false, err = false;
    while (i != buf.end())
    {
        string line = eatToSkipWhitespace(buf, i, '\n', 4096, &eof, &err);
        if (err) break;

        if (!line.empty() && line.back() == '\r') line.pop_back(); // CRLF support

        if (line.empty() || line[0] == '#') continue;

        auto pos = line.find('=');
        if (pos == string::npos)
        {
            warning("Invalid line in config (no '='): \"%s\"\n", line.c_str());
            continue;
        }

        string key = line.substr(0, pos);
        string value = line.substr(pos + 1);

        auto trim = [](string &s)
            {
                if (s.empty()) return;
                size_t end = s.find_last_not_of(" \t\r\n");
                if (end == string::npos) { s.clear(); return; }
                s.erase(end + 1);
                size_t start = s.find_first_not_of(" \t\r\n");
                if (start > 0) s.erase(0, start);
            };
        trim(key);
        trim(value);

        if (key.empty())
        {
            warning("Empty key in config line ignored.\n");
            continue;
        }

        return { key, value };
    }
    return { "", "" };
}

static bool isXmqConfigWithRoot(std::vector<char> &buf, const char *root)
{
    // Scan buf for first left brace { and check if the name right before it
    // (ignoring whitespace) is the expected root node name.
    size_t n = strlen(root);
    for (size_t i = 0; i < buf.size(); ++i)
    {
        if (buf[i] != '{') continue;
        // Skip whitespace before the brace to find the end of the node name.
        size_t name_end = i; // exclusive
        while (name_end > 0 && (buf[name_end-1] == ' ' || buf[name_end-1] == '\t' ||
                buf[name_end-1] == '\n' || buf[name_end-1] == '\r' ||
                buf[name_end-1] == '\f' || buf[name_end-1] == '\v')) --name_end;
        bool ok = false;
        if (name_end >= n)
        {
            size_t name_start = name_end - n;
            if (memcmp(buf.data() + name_start, root, n) == 0 &&
                (name_start == 0 || buf[name_start-1] == ' ' || buf[name_start-1] == '\t' ||
                 buf[name_start-1] == '\n' || buf[name_start-1] == '\r' ||
                 buf[name_start-1] == '\f' || buf[name_start-1] == '\v'))
            {
                ok = true;
            }
        }
        // First { found, this is as far as we need to look.
        return ok;
    }
    return false;
}

bool isWMBusMetersConfigXMQ(std::vector<char> &buf)
{
    return isXmqConfigWithRoot(buf, "wmbusmeters");
}

bool isMeterConfigXMQ(std::vector<char> &buf)
{
    return isXmqConfigWithRoot(buf, "meter");
}

struct MeterConfigParsing
{
    Configuration *c;
    string bus;
    string name;
    string driver = "auto";
    string address_expressions;
    string key;
    int poll_interval = 0;
    IdentityMode identity_mode {};
    vector<string> telegram_shells;
    vector<string> new_meter_shells;
    vector<string> alarm_shells;
    vector<string> extra_constant_fields;
    vector<string> extra_calculated_fields;
    vector<string> selected_fields;
};

// Handle one key=value pair from a meter config file.
// Return true to keep parsing, false to abort parsing of this meter config file.
static bool handleMeterConfigKey(MeterConfigParsing &m, string key, string value)
{
    if (key != "key") {
        debug("(config) %s=%s\n", key.c_str(), value.c_str());
    }

    // If the key starts with # then the line is a comment. Ignore it.
    if (key.length() > 0 && key[0] == '#') return true;

    if (key == "bus")
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
        m.bus = value;
        if (!isValidAlias(m.bus))
        {
            warning("Found invalid meter bus \"%s\" in meter config file, skipping meter.\n", m.bus.c_str());
            return false;
        }
    }
    else if (key == "name")
    {
        m.name = value;
        if (m.name.find(":") != string::npos)
        {
            // Oups, names are not allowed to contain the :
            warning("Found invalid meter name \"%s\" in meter config file, must not contain a ':', skipping meter.\n", m.name.c_str());
            return false;
        }
    }
    else if (key == "type")
    {
        m.driver = value;
    }
    else if (key == "driver")
    {
        m.driver = value;
    }
    else if (key == "id")
    {
        m.address_expressions = value;
    }
    else if (key == "key")
    {
        m.key = value;
        debug("(config) key=<notprinted>\n");
    }
    else if (key == "pollinterval")
    {
        if (m.poll_interval != 0)
        {
            errorAndExit(EXIT_CONFIG_ERROR, "You have already specified a poll interval for this meter!\n");
        }
        m.poll_interval = parseTime(value);

        if (m.poll_interval == 0)
        {
            errorAndExit(EXIT_CONFIG_ERROR, "Poll interval must be non-zero \"%s\"!\n", value.c_str());
        }
    }
    else if (key == "identitymode")
    {
        m.identity_mode = toIdentityMode(value.c_str());

        if (m.identity_mode == IdentityMode::INVALID)
        {
            errorAndExit(EXIT_CONFIG_ERROR, "Invalid identity mode: \"%s\"!\n", value.c_str());
        }
    }
    else if (key == "shell")
    {
        m.telegram_shells.push_back(value);
    }
    else if (key == "metershell")
    {
        m.new_meter_shells.push_back(value);
    }
    else if (key == "alarmshell")
    {
        m.alarm_shells.push_back(value);
    }
    else if (key == "selectedfields")
    {
        if (m.selected_fields.size() > 0)
        {
            warning("(warning) selectfields already used! Ignoring selectfields %s", value.c_str());
            return false;
        }
        m.selected_fields = splitString(value, ',');
    }
    else if (startsWith(key, "json_") ||
             startsWith(key, "field_"))
    {
        int off = 5;
        if (startsWith(key, "field_")) { off = 6; }
        string keyvalue = key.substr(off)+"="+value;
        m.extra_constant_fields.push_back(keyvalue);
    }
    else if (startsWith(key, "calculate_"))
    {
        string keyvalue = key.substr(10)+"="+value;
        m.extra_calculated_fields.push_back(keyvalue);
    }
    else
    {
        warning("Found invalid key \"%s\" in meter config file\n", key.c_str());
    }

    return true;
}

struct MeterConfigEach
{
    MeterConfigParsing *m;
    bool keep_going;
};

static XMQProceed meterConfigEach(XMQDoc *doc, XMQNode *node, void *user_data)
{
    MeterConfigEach *each = (MeterConfigEach*) user_data;

    const char *key_c = xmqGetName(node);
    if (!key_c) return each->keep_going ? XMQ_CONTINUE : XMQ_STOP;

    string key = key_c;

    // Comment lines are stored as nodes with names //..., ignore them.
    if (key.length() > 1 && key[0] == '/' && key[1] == '/') return XMQ_CONTINUE;

    const char *value_c = xmqGetContent(node);
    string value = value_c ? value_c : "";

    if (!handleMeterConfigKey(*each->m, key, value))
    {
        each->keep_going = false;
        return XMQ_STOP;
    }

    return XMQ_CONTINUE;
}

void parseMeterConfig(Configuration *c, vector<char> &buf, string file)
{
    MeterConfigParsing m;
    m.c = c;

    debug("(config) loading meter file %s\n", file.c_str());

    bool abort_meter = false;

    if (isMeterConfigXMQ(buf))
    {
        // The meter config file is an xmq file with a meter { ... } node,
        // extract the keys and values with the xmq api.
        XMQReturnDoc rd = xmqNewDoc();
        bool ok = rd.status == XMQ_OK;
        if (ok)
        {
            XMQDoc *doc = rd.doc;
            bool pok = xmqParseBuffer(doc, buf.data(), buf.data()+buf.size(), NULL, 0);
            XMQNode *mn = pok ? xmqGetNode(doc, "/meter") : NULL;
            if (pok && mn)
            {
                MeterConfigEach each;
                each.m = &m;
                each.keep_going = true;
                xmqForeachRel(doc, "*", meterConfigEach, &each, mn);
                abort_meter = !each.keep_going;
            }
            if (!pok)
            {
                warning("Failed to parse meter config file \"%s\" as xmq: %s\n",
                        file.c_str(),
                        xmqDocError(doc) ? xmqDocError(doc) : "");
            }
            xmqFreeDoc(doc);
        }
    }
    else
    {
        // This is a meter config file in the old key=value format.
        auto i = buf.begin();

        for (;;)
        {
            pair<string,string> p = getNextKeyValue(buf, i);

            if (p.first == "") break;

            if (!handleMeterConfigKey(m, p.first, p.second))
            {
                abort_meter = true;
                break;
            }
        }
    }

    if (abort_meter) return;

    bool use = true;

    MeterInfo mi;

    // Modified meter id validation:
    // Accept pure numeric non-empty IDs silently (common case).
    // Only warn if not numeric and not a valid sequence of match expressions.
    {
        bool numeric_id = !m.address_expressions.empty() &&
            m.address_expressions.find_first_not_of("0123456789") == string::npos;
        if (!numeric_id)
        {
            if (!isValidSequenceOfAddressExpressions(m.address_expressions))
            {
                warning("In config, not a valid meter id nor a valid sequence of match expression \"%s\"\n", m.address_expressions.c_str());
                use = false;
            }
        }
    }

    if (use)
    {
        mi.parse(m.name, m.driver, m.address_expressions, m.key); // sets driver, extras, name, bus, bps, link_modes, ids, name, key
        if (m.bus != "")
        {
            // The bus key sets the bus alias for this meter,
            // overriding any bus specified in the driver string.
            if (mi.bus != "" && mi.bus != m.bus)
            {
                warning("In config, meter %s has both bus \"%s\" in the driver string and bus \"%s\" in the bus key, using the bus key.\n",
                        m.name.c_str(), mi.bus.c_str(), m.bus.c_str());
            }
            mi.bus = m.bus;
        }
        mi.poll_interval = m.poll_interval;
        mi.identity_mode = m.identity_mode;

        if (!isValidKey(m.key, mi))
        {
            warning("In config, not a valid meter key in config \"%s\"\n", m.key.c_str());
            use = false;
        }

        if (use)
        {
            mi.extra_constant_fields = m.extra_constant_fields;
            mi.extra_calculated_fields = m.extra_calculated_fields;
            mi.shells = m.telegram_shells;
            mi.new_meter_shells = m.new_meter_shells;
            mi.selected_fields = m.selected_fields;
            c->meters.push_back(mi);
        }
    }

    return;
}

void handleLoglevel(Configuration *c, string loglevel)
{
    if (loglevel == "verbose")
    {
        c->silent = false;
        c->verbose = true;
        c->debug = false;
        c->trace = false;
        verboseEnabled(c->verbose);
    }
    else if (loglevel == "debug")
    {
        c->silent = false;
        c->verbose = false;
        c->debug = true;
        c->trace = false;
        // Kick in debug immediately.
        debugEnabled(c->debug);
    }
    else if (loglevel == "trace")
    {
        c->silent = false;
        c->verbose = false;
        c->debug = false;
        c->trace = true;
        // Kick in trace immediately.
        traceEnabled(c->trace);
    }
    else if (loglevel == "silent")
    {
        c->silent = true;
        c->verbose = false;
        c->debug = false;
        c->trace = false;
    }
    else if (loglevel == "normal")
    {
        c->silent = false;
        c->verbose = false;
        c->debug = false;
        c->trace = false;
    }
    else
    {
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

void handleNoNet(Configuration *c, string value)
{
    if (value == "true")
    {
        c->no_net = true;
    }
    else if (value == "false")
    {
        c->no_net = false;
    }
    else
    {
        warning("nonet should be either true or false, not \"%s\"\n", value.c_str());
    }
}

void handleBasicAuth(Configuration *c, string value)
{
    c->basic_auth_cred = value;
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

void handleAddTelegramDetails(Configuration *c, string value)
{
    if (value == "never")
    {
        c->telegram_details = TelegramDetails::NEVER;
    }
    else if (value == "first")
    {
        c->telegram_details = TelegramDetails::FIRST;
    }
    else if (value == "always")
    {
        c->telegram_details = TelegramDetails::ALWAYS;
    }
    else {
        warning("addtelegramdetails should be either never,first or always, not \"%s\"\n", value.c_str());
    }
}

void handleAddTelegramHex(Configuration *c, string value)
{
    if (value == "true")
    {
        c->add_telegram_hex = true;
    }
    else if (value == "false")
    {
        c->add_telegram_hex = false;
    }
    else {
        warning("addtelegramhex should be either true or false, not \"%s\"\n", value.c_str());
    }
}

void handleAddTelegramStructured(Configuration *c, string value)
{
    if (value == "true")
    {
        c->add_telegram_structured = true;
    }
    else if (value == "false")
    {
        c->add_telegram_structured = false;
    }
    else {
        warning("addtelegramstructured should be either true or false, not \"%s\"\n", value.c_str());
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

bool handleDeviceOrHex(Configuration *c, string devicefilehex)
{
    bool invalid_hex = false;
    bool is_hex = isHexStringFlex(devicefilehex, &invalid_hex);
    if (is_hex)
    {
        if (invalid_hex)
        {
            errorAndExit(EXIT_CONFIG_ERROR, "Bad hex: \"%s\"\nHex string must have an even length of hexadecimal characters.\n", devicefilehex.c_str());
        }
    }

    SpecifiedDevice specified_device;
    bool ok = specified_device.parse(devicefilehex);
    if (!ok && SpecifiedDevice::isLikelyDevice(devicefilehex))
    {
        errorAndExit(EXIT_CONFIG_ERROR, "Not a valid device \"%s\"\n", devicefilehex.c_str());
    }

    if (!ok) return false;

    // Number the devices
    specified_device.index = c->supplied_bus_devices.size();
    // Probe for the specified devices.
    c->probe_for.insert(specified_device.type);

    if (specified_device.type == BusDeviceType::DEVICE_MBUS)
    {
        if (!specified_device.linkmodes.empty())
        {
            errorAndExit(EXIT_CONFIG_ERROR, "An mbus device must not have linkmode set. \"%s\"\n", devicefilehex.c_str());
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
        if (specified_device.type == BusDeviceType::DEVICE_RTLWMBUS ||
            specified_device.type == BusDeviceType::DEVICE_RTL433)
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
            errorAndExit(EXIT_CONFIG_ERROR, "You can only specify one stdin or one file or one command!\n");
        }
        if (c->use_auto_device_detect)
        {
            errorAndExit(EXIT_CONFIG_ERROR, "You cannot mix auto with stdin or a file.\n");
        }
        if (specified_device.is_simulation) c->simulation_found = true;
        c->single_device_override = true;
    }

    if (specified_device.type == BusDeviceType::DEVICE_AUTO)
    {
        c->use_auto_device_detect = true;
        c->auto_device_linkmodes = specified_device.linkmodes;
    }
    else
    {
        c->supplied_bus_devices.push_back(specified_device);
        if (specified_device.type == BusDeviceType::DEVICE_MBUS)
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
        errorAndExit(EXIT_CONFIG_ERROR, "Unknown link modes \"%s\"!\n", mode.c_str());
    }
    if (!c->default_device_linkmodes.empty())
    {
        errorAndExit(EXIT_CONFIG_ERROR, "You have already specified the default link modes!\n");
    }

    c->default_device_linkmodes = lms;
}

void handleExitAfter(Configuration *c, string after)
{
    if (c->exitafter > 0)
    {
        errorAndExit(EXIT_CONFIG_ERROR, "You have already specified an exit after time!\n");
    }

    c->exitafter = parseTime(after);

    if (c->exitafter == 0)
    {
        errorAndExit(EXIT_CONFIG_ERROR, "Exit after time must be non-zero \"%s\"!\n", after.c_str());
    }
}

void handleOneshot(Configuration *c, string oneshot)
{
    if (oneshot == "true") { c->oneshot = true; }
    else if (oneshot == "false") { c->oneshot = false;}
    else {
        warning("No such oneshot setting: \"%s\"\n", oneshot.c_str());
    }
}

void handleLogtelegrams(Configuration *c, string logtelegrams)
{
    if (logtelegrams == "true") { c->logtelegrams = true; }
    else if (logtelegrams == "false") { c->logtelegrams = false;}
    else {
        warning("No such logtelegrams setting: \"%s\"\n", logtelegrams.c_str());
    }
}

void handleLogsummary(Configuration *c, string logsummary)
{
    if (logsummary == "true") { c->logsummary = true; }
    else if (logsummary == "false") { c->logsummary = false;}
    else {
        warning("No such logsummary setting: \"%s\"\n", logsummary.c_str());
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
    else if (type == "month")
    {
        c->meterfiles_timestamp = MeterFileTimestamp::Month;
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
    c->output_format = XMQ_CONTENT_UNKNOWN;
    c->fields = false;
    if (format == "hr")
    {
        c->separator = '\t';
    }
    else if (format == "json")
    {
        c->output_format = XMQ_CONTENT_JSON;
    }
    else if (format == "xml")
    {
        c->output_format = XMQ_CONTENT_XML;
    }
    else if (format == "xmq")
    {
        c->output_format = XMQ_CONTENT_XMQ;
    }
    else if (format == "fields")
    {
        c->fields = true;
        c->separator = ';';
    }
    else
    {
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
    c->selected_fields = splitString(s, ',');
}

void handleShell(Configuration *c, string cmdline)
{
    c->telegram_shells.push_back(cmdline);
}

void handleMeterShell(Configuration *c, string cmdline)
{
    c->new_meter_shells.push_back(cmdline);
}

void handleAlarmShell(Configuration *c, string cmdline)
{
    c->alarm_shells.push_back(cmdline);
}

void handleRawShell(Configuration *c, string cmdline)
{
    c->raw_shells.push_back(cmdline);
}

void handleExtraConstantField(Configuration *c, string field)
{
    c->extra_constant_fields.push_back(field);
}

void handleExtraCalculatedField(Configuration *c, string field)
{
    c->extra_calculated_fields.push_back(field);
}

// Handle one key=value pair from the main config file.
static void handleGlobalConfigKey(Configuration *c, string key, string value)
{
    debug("(config) \"%s\" \"%s\"\n", key.c_str(), value.c_str());

    // If the key starts with # then the line is a comment. Ignore it.
    if (key.length() > 0 && key[0] == '#') return;

    if (key == "loglevel") handleLoglevel(c, value);
    else if (key == "internaltesting") handleInternalTesting(c, value);
    else if (key == "nonet") handleNoNet(c, value);
    else if (key == "basicauth") handleBasicAuth(c, value);
    else if (key == "ignoreduplicates") handleIgnoreDuplicateTelegrams(c, value);
    else if (key == "addtelegramdetails") handleAddTelegramDetails(c, value);
    else if (key == "addtelegramhex") handleAddTelegramHex(c, value);
    else if (key == "addtelegramstructured") handleAddTelegramStructured(c, value);
    else if (key == "device") handleDeviceOrHex(c, value);
    else if (key == "donotprobe") handleDoNotProbe(c, value);
    else if (key == "listento") handleListenTo(c, value);
    else if (key == "exitafter") handleExitAfter(c, value);
    else if (key == "oneshot") handleOneshot(c, value);
    else if (key == "logtelegrams") handleLogtelegrams(c, value);
    else if (key == "logsummary") handleLogsummary(c, value);
    else if (key == "meterfiles") handleMeterfiles(c, value);
    else if (key == "meterfilesaction") handleMeterfilesAction(c, value);
    else if (key == "meterfilesnaming") handleMeterfilesNaming(c, value);
    else if (key == "meterfilestimestamp") handleMeterfilesTimestamp(c, value);
    else if (key == "logfile") handleLogfile(c, value);
    else if (key == "format") handleFormat(c, value);
    else if (key == "alarmtimeout") handleAlarmTimeout(c, value);
    else if (key == "alarmexpectedactivity") handleAlarmExpectedActivity(c, value);
    else if (key == "separator") handleSeparator(c, value);
    else if (key == "logtimestamps") handleLogTimestamps(c, value);
    else if (key == "selectfields") handleSelectedFields(c, value);
    else if (key == "shell") handleShell(c, value);
    else if (key == "resetafter") handleResetAfter(c, value);
    else if (key == "metershell") handleMeterShell(c, value);
    else if (key == "alarmshell") handleAlarmShell(c, value);
    else if (key == "rawshell") handleRawShell(c, value);
    else if (startsWith(key, "json_") ||
             startsWith(key, "field_"))
    {
        int off = 5;
        if (startsWith(key, "field_")) { off = 6; }
        string s = key.substr(off);
        string keyvalue = s+"="+value;
        handleExtraConstantField(c, keyvalue);
    }
    else if (startsWith(key, "calculate_"))
    {
        string keyvalue = key.substr(10)+"="+value;
        handleExtraCalculatedField(c, keyvalue);
    }
    else
    {
        warning("No such key: %s\n", key.c_str());
    }
}

struct GlobalConfigEach
{
    Configuration *c;
};

static XMQProceed wmbusMetersConfigEach(XMQDoc *doc, XMQNode *node, void *user_data)
{
    GlobalConfigEach *each = (GlobalConfigEach*) user_data;

    const char *key_c = xmqGetName(node);
    if (!key_c) return XMQ_CONTINUE;

    string key = key_c;

    // Comment lines are stored as nodes with names //..., ignore them.
    if (key.length() > 1 && key[0] == '/' && key[1] == '/') return XMQ_CONTINUE;

    const char *value_c = xmqGetContent(node);
    string value = value_c ? value_c : "";

    handleGlobalConfigKey(each->c, key, value);

    return XMQ_CONTINUE;
}

shared_ptr<Configuration> loadConfiguration(string root, ConfigOverrides overrides)
{
    Configuration *c = new Configuration;

    // JSon is default when configuring from config files.
    c->output_format = XMQ_CONTENT_JSON;

    vector<char> global_conf;

    // --useconfig=/ will work to find /etc/wmbusmeters.conf and /etc/wmbusmeters.d
    // --useconfig=/etc will also work to find /etc/wmbusmeters.conf and /etc/wmbusmeters.d
    // The second one is preferable but for backward compatibility the first one is tested first.
    // If there is no /+etc/wmbusmeters.conf then it will look for /+wmbusmeters.conf

    string conf_dir = root;
    string conf_file = root+"/etc/wmbusmeters.conf";
    string conf_meter_dir = root+"/etc/wmbusmeters.d";
    string conf_drivers_dir = root+"/etc/wmbusmeters.drivers.d";

    if (!checkFileExists(conf_file.c_str()))
    {
        conf_dir = root+"/etc";
        conf_file = root+"/wmbusmeters.conf";
        conf_meter_dir = root+"/wmbusmeters.d";
        conf_drivers_dir = root+"/wmbusmeters.drivers.d";
    }

    debug("(config) loading %s\n", conf_file.c_str());

    if (!loadFile(conf_file, &global_conf)) {
        errorAndExit(EXIT_CONFIG_ERROR, "Cannot load config file \"%s\"\n", conf_file.c_str());
    }

    global_conf.push_back('\n');

    bool xmq_ok = false;

    if (isWMBusMetersConfigXMQ(global_conf))
    {
        // The config file is an xmq file with a wmbusmeters { ... } node,
        // load the values using xmqGetString and xmqForEach.
        XMQReturnDoc rd = xmqNewDoc();
        if (rd.status == XMQ_OK)
        {
            XMQDoc *doc = rd.doc;
            bool pok = xmqParseBuffer(doc, global_conf.data(), global_conf.data()+global_conf.size(), NULL, 0);
            XMQNode *rpok = pok ? xmqGetNode(doc, "/wmbusmeters") : NULL;
            if (pok && rpok)
            {
                GlobalConfigEach each;
                each.c = c;
                xmqForeachRel(doc, "*", wmbusMetersConfigEach, &each, rpok);
                xmq_ok = true;
            }
            if (!pok)
            {
                warning("Failed to parse config file \"%s\" as xmq: %s\n",
                        conf_file.c_str(),
                        xmqDocError(doc) ? xmqDocError(doc) : "");
            }
            xmqFreeDoc(doc);
        }
    }

    if (!xmq_ok)
    {
        // This is a config file in the old key=value format.
        auto i = global_conf.begin();

        for (;;) {
            auto p = getNextKeyValue(global_conf, i);

            if (p.first == "") break;

            handleGlobalConfigKey(c, p.first, p.second);
        }
    }

    if (overrides.device_override != "")
    {
        // There is an override, therefore we
        // drop any already loaded devices from the config file.
        c->use_auto_device_detect = false;
        c->supplied_bus_devices.clear();

        if (startsWith(overrides.device_override, "/dev/rtlsdr"))
        {
            debug("(config) use rtlwmbus instead of raw device %s\n", overrides.device_override.c_str());
            overrides.device_override = "rtlwmbus";
        }
        debug("(config) overriding device with \"%s\"\n", overrides.device_override.c_str());
        handleDeviceOrHex(c, overrides.device_override);
    }

    if (overrides.listento_override != "")
    {
        debug("(config) overriding listento with \"%s\"\n", overrides.listento_override.c_str());
        handleListenTo(c, overrides.listento_override);
    }

    if (overrides.exitafter_override != "")
    {
        debug("(config) overriding exitafter with \"%s\"\n", overrides.exitafter_override.c_str());
        handleExitAfter(c, overrides.exitafter_override);
    }

    if (overrides.oneshot_override != "")
    {
        debug("(config) overriding oneshot with true\n");
        handleOneshot(c, "true");
    }

    if (overrides.loglevel_override != "")
    {
        debug("(config) overriding loglevel with %s\n", overrides.loglevel_override.c_str());
        handleLoglevel(c, overrides.loglevel_override);
    }

    if (overrides.logfile_override != "")
    {
        debug("(config) overriding logfile with %s\n", overrides.logfile_override.c_str());
        handleLogfile(c, overrides.logfile_override);
    }

    loadDriversFromDir(conf_drivers_dir);

    // Load meters AFTER the drivers have been loaded. That is better...

    vector<string> meters;
    listFiles(conf_meter_dir, &meters);

    for (auto& f : meters)
    {
        vector<char> meter_conf;
        string file = conf_meter_dir+"/"+f;
        loadFile(file.c_str(), &meter_conf);
        meter_conf.push_back('\n');
        parseMeterConfig(c, meter_conf, file);
    }

    return shared_ptr<Configuration>(c);
}

LinkModeCalculationResult calculateLinkModes(Configuration *config, BusDevice *device, bool link_modes_matter)
{
    int n = device->numConcurrentLinkModes();
    string num = to_string(n);
    if (n == 0) num = "any combination";
    string dongles = device->supportedLinkModes().hr();
    string dongle;
    strprintf(&dongle, "%s of %s", num.c_str(), dongles.c_str());

    // Calculate the possible listen_to linkmodes for this collection of meters.
    LinkModeSet meters_union = UNKNOWN_bit;
    for (auto &m : config->meters)
    {
        meters_union.unionLinkModeSet(m.link_modes);
        string meter = m.link_modes.hr();
        debug("(config) meter %s link mode(s): %s\n", m.driverName().str().c_str(), meter.c_str());
    }
    string metersu = meters_union.hr();
    debug("(config) all possible link modes that the meters might transmit on: %s\n", metersu.c_str());
    if (meters_union.empty())
    {
        if (link_modes_matter && config->all_device_linkmodes_specified.empty())
        {
            string msg;
            strprintf(&msg,"(config) No meters supplied. You must supply which link modes to listen to. 22 Eg. auto:t1");
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
        strprintf(&msg, "(config) You have specified to listen to the link modes: %s but the meters might transmit on: %s\n"
                  "(config) Therefore you might miss telegrams! Please specify the expected transmit mode for the meters, eg: apator162:t1\n"
                  "(config) Or use a dongle that can listen to all the required link modes at the same time.",
                  all_lms.c_str(), metersu.c_str());
        debug("%s\n", msg.c_str());
        return { LinkModeCalculationResultType::MightMissTelegrams, msg};
    }

    return { LinkModeCalculationResultType::Success, "" };
}
