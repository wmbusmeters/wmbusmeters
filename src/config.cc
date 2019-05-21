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
    vector<string> shells;

    debug("(config) loading meter file %s\n", file.c_str());
    for (;;) {
        auto p = getNextKeyValue(buf, i);

        if (p.first == "") break;

        if (p.first == "name") name = p.second;
        else
        if (p.first == "type") type = p.second;
        else
        if (p.first == "id") id = p.second;
        else
        if (p.first == "key") {
            key = p.second;
            debug("(config) key=<notprinted>\n");
        }
        else
        if (p.first == "shell") {
            shells.push_back(p.second);
        }
        else
            warning("Found invalid key \"%s\" in meter config file\n", p.first.c_str());

        if (p.first != "key") {
            debug("(config) %s=%s\n", p.first.c_str(), p.second.c_str());
        }
    }

    MeterType mt = toMeterType(type);
    bool use = true;
    if (mt == MeterType::UNKNOWN) {
        warning("Not a valid meter type \"%s\"\n", type.c_str());
        use = false;
    }
    if (!isValidId(id)) {
        warning("Not a valid meter id \"%s\"\n", id.c_str());
        use = false;
    }
    if (!isValidKey(key)) {
        warning("Not a valid meter key \"%s\"\n", key.c_str());
        use = false;
    }
    if (use) {
        c->meters.push_back(MeterInfo(name, type, id, key, shells));
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
    // rtlwmbus:/usr/bin/rtl_sdr -f 868.9M -s 1600000 - 2>/dev/null | /usr/bin/rtl_wmbus
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

unique_ptr<Configuration> loadConfiguration(string root)
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
        else if (p.first == "logtelegrams") handleLogtelegrams(c, p.second);
        else if (p.first == "meterfiles") handleMeterfiles(c, p.second);
        else if (p.first == "meterfilesaction") handleMeterfilesAction(c, p.second);
        else if (p.first == "logfile") handleLogfile(c, p.second);
        else if (p.first == "format") handleFormat(c, p.second);
        else if (p.first == "separator") handleSeparator(c, p.second);
        else if (p.first == "addconversions") handleConversions(c, p.second);
        else if (p.first == "shell") handleShell(c, p.second);
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

    return unique_ptr<Configuration>(c);
}
