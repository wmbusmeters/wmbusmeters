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

#include"cmdline.h"
#include"drivers.h"
#include"meters.h"
#include"util.h"

#include<string>
#include<unistd.h>

using namespace std;

static bool isDaemon(int argc, char **argv);
static bool checkIfUseConfig(int argc, char **argv);
static shared_ptr<Configuration> parseNormalCommandLine(Configuration *c, int argc, char **argv);
static shared_ptr<Configuration> parseCommandLineWithUseConfig(Configuration *c, int argc, char **argv, bool pid_file_expected);

shared_ptr<Configuration> parseCommandLine(int argc, char **argv)
{
    Configuration * c = new Configuration;

    c->bin_dir = dirname(currentProcessExe());

    if (isDaemon(argc, argv))
    {
        c->daemon = true;
        if (argc < 2)
        {
            error("Usage error: wmbusmetersd must have at least a single argument to the pid file.\n");
        }
        return parseCommandLineWithUseConfig(c, argc, argv, true);
    }

    if (argc < 2)
    {
        c->need_help = true;
        return shared_ptr<Configuration>(c);
    }

    if (checkIfUseConfig(argc, argv))
    {
        return parseCommandLineWithUseConfig(c, argc, argv, false);
    }

    return parseNormalCommandLine(c, argc, argv);
}

void enableEarlyLoggingFromCommandLine(int argc, char **argv)
{
    int i = 1;
    // First find all logging flags, --silent --verbose --normal --debug
    while (argv[i] && argv[i][0] == '-')
    {
        if (!strcmp(argv[i], "--silent")) {
            i++;
            silentLogging(true);
            continue;
        }
        if (!strcmp(argv[i], "--verbose")) {
            verboseEnabled(true);
            i++;
            continue;
        }
        if (!strcmp(argv[i], "--normal")) {
            i++;
            continue;
        }
        if (!strcmp(argv[i], "--debug")) {
            verboseEnabled(true);
            debugEnabled(true);
            i++;
            continue;
        }
        if (!strcmp(argv[i], "--trace")) {
            verboseEnabled(true);
            debugEnabled(true);
            traceEnabled(true);
            i++;
            continue;
        }
        i++;
    }
}

static shared_ptr<Configuration> parseNormalCommandLine(Configuration *c, int argc, char **argv)
{
    int i = 1;
    // First find all logging flags, --silent --verbose --normal --debug
    while (argv[i] && argv[i][0] == '-')
    {
        if (!strcmp(argv[i], "--silent")) {
            c->silent = true;
            i++;
            silentLogging(true);
            continue;
        }
        if (!strcmp(argv[i], "--verbose")) {
            c->verbose = true;
            verboseEnabled(true);
            i++;
            continue;
        }
        if (!strcmp(argv[i], "--normal")) {
            c->silent = false;
            c->verbose = false;
            c->debug = false;
            c->trace = false;
            i++;
            continue;
        }
        if (!strcmp(argv[i], "--debug")) {
            c->debug = true;
            verboseEnabled(true);
            debugEnabled(true);
            i++;
            continue;
        }
        if (!strcmp(argv[i], "--trace")) {
            c->debug = true;
            c->trace = true;
            verboseEnabled(true);
            debugEnabled(true);
            traceEnabled(true);
            i++;
            continue;
        }
        i++;
    }

    // Now do the rest of the arguments.
    i = 1;
    while (argv[i] && argv[i][0] == '-')
    {
        if (!strcmp(argv[i], "-h") || !strcmp(argv[i], "-help") || !strcmp(argv[i], "--help")) {
            c->need_help = true;
            return shared_ptr<Configuration>(c);
        }
        if (!strcmp(argv[i], "--silent") ||
            !strcmp(argv[i], "--verbose") ||
            !strcmp(argv[i], "--normal") ||
            !strcmp(argv[i], "--debug") ||
            !strcmp(argv[i], "--trace"))
        {
            // Handled already.
            i++;
            continue;
        }
        if (!strncmp(argv[i], "--device=", 9) || // Deprecated
            !strncmp(argv[i], "--overridedevice=", 17))
        {
            error("You can only use --overridedevice=xyz with --useconfig=xyz\n");
        }
        if (!strcmp(argv[i], "--version")) {
            c->version = true;
            return shared_ptr<Configuration>(c);
        }
        if (!strcmp(argv[i], "--license")) {
            c->license = true;
            return shared_ptr<Configuration>(c);
        }
        if (!strcmp(argv[i], "--analyze")) {
            c->analyze = true;
            if (isatty(1))
            {
                c->analyze_format = OutputFormat::TERMINAL;
            }
            else
            {
                c->analyze_format = OutputFormat::PLAIN;
            }
            c->analyze_driver = "";
            c->analyze_key = "";
            c->analyze_verbose = false;
            i++;
            continue;
        }
        if (!strncmp(argv[i], "--analyze=", 10)) {
            c->analyze = true;
            if (isatty(1))
            {
                c->analyze_format = OutputFormat::TERMINAL;
            }
            else
            {
                c->analyze_format = OutputFormat::PLAIN;
            }
            c->analyze_driver = "";
            c->analyze_key = "";
            c->analyze_verbose = false;
            string arg = string(argv[i]+10);
            vector<string> args = splitString(arg, ':');

            for (auto s : args)
            {
                bool inv = false;
                if (isHexStringStrict(s, &inv))
                {
                    if (inv)
                    {
                        error("Bad key \"%s\"", s.c_str());
                    }
                    c->analyze_key = s;
                }
                else if (s == "plain") c->analyze_format = OutputFormat::PLAIN;
                else if (s == "terminal") c->analyze_format = OutputFormat::TERMINAL;
                else if (s == "json") c->analyze_format = OutputFormat::JSON;
                else if (s == "html") c->analyze_format = OutputFormat::HTML;
                else if (s == "verbose") c->analyze_verbose = true;
                else
                {
                    MeterInfo mi;
                    mi.parse("x", s, "00000000", "");

                    if (mi.driver_name.str() == "")
                    {
                        error("Not a valid meter driver \"%s\"\n", s.c_str());
                    }
                    c->analyze_driver = s;
                }
            }
            i++;
            continue;
        }
        if (!strncmp(argv[i], "--logtimestamps=", 16))
        {
            string ts = string(argv[i]+16);
            debug("(cmdline) add log timestamps \"%s\"\n", ts.c_str());
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
                error("No such timestamp setting \"%s\" possible values are: never always important\n",
                      ts.c_str());
            }
            i++;
            continue;
        }
        if (!strcmp(argv[i], "--internaltesting")) {
            c->internaltesting = true;
            i++;
            continue;
        }
        if (!strncmp(argv[i], "--profile=", 10)) {
            c->analyze_profile = atoi(argv[i]+10);
            if (c->analyze_profile <= 0) error("Illegal profile value, must be greater than zero.\n");
            i++;
            continue;
        }
        if (!strncmp(argv[i], "--donotprobe=", 13))
        {
            string df = string(argv[i]+13);
            debug("(cmdline) do not probe \"%s\"\n", df.c_str());
            c->do_not_probe_ttys.insert(df);
            i++;
            continue;
        }
        if (!strncmp(argv[i], "--listento=", 11))
        {
            LinkModeSet lms = parseLinkModes(argv[i]+11);
            if (lms.empty())
            {
                error("Unknown default link mode \"%s\"!\n", argv[i]+11);
            }
            if (!c->default_device_linkmodes.empty()) {
                error("You have already specified a default link mode!\n");
            }
            c->default_device_linkmodes = lms;
            i++;
            continue;
        }

        LinkMode lm = isLinkModeOption(argv[i]);
        if (lm != LinkMode::UNKNOWN) {
            if (!c->default_device_linkmodes.empty())
            {
                error("You have already specified a default link mode!\n");
            }
            // Add to the empty set.
            c->default_device_linkmodes.addLinkMode(lm);
            i++;
            continue;
        }
        if (!strcmp(argv[i], "--logtelegrams")) {
            c->logtelegrams = true;
            i++;
            continue;
        }
        if (!strcmp(argv[i], "--logsummary")) {
            c->logsummary = true;
            i++;
            continue;
        }
        if (!strcmp(argv[i], "--ppjson"))
        {
            c->pretty_print_json = true;
            i++;
            continue;
        }
        if (!strncmp(argv[i], "--format=", 9))
        {
            if (!strcmp(argv[i]+9, "json"))
            {
                c->json = true;
                c->fields = false;
            }
            else
            if (!strcmp(argv[i]+9, "fields"))
            {
                c->json = false;
                c->fields = true;
                c->separator = ';';
            }
            else
            if (!strcmp(argv[i]+9, "hr"))
            {
                c->json = false;
                c->fields = false;
                c->separator = '\t';
            }
            else
            {
                error("Unknown output format: \"%s\"\n", argv[i]+9);
            }
            i++;
            continue;
        }
        if (!strncmp(argv[i], "--selectfields=", 15)) {
            if (strlen(argv[i]) > 15)
            {
                string s = string(argv[i]+15);
                handleSelectedFields(c, s);
            } else {
                error("You must supply fields to be selected.\n");
            }
            i++;
            continue;
        }
        if (!strncmp(argv[i], "--separator=", 12)) {
            if (!c->fields) {
                error("You must specify --format=fields before --separator=X\n");
            }
            if (strlen(argv[i]) != 13) {
                error("You must supply a single character as the field separator.\n");
            }
            c->separator = argv[i][12];
            i++;
            continue;
        }
        if (!strncmp(argv[i], "--meterfilesaction", 18)) {
            if (strlen(argv[i]) > 18 && argv[i][18] == '=') {
                if (!strncmp(argv[i]+19, "overwrite", 9)) {
                    c->meterfiles_action = MeterFileType::Overwrite;
                } else if (!strncmp(argv[i]+19, "append", 6)) {
                    c->meterfiles_action = MeterFileType::Append;
                } else {
                    error("No such meter file action %s\n", argv[i]+19);
                }
            } else {
                error("Incorrect option %s\n", argv[i]);
            }
            i++;
            continue;
        }
        if (!strncmp(argv[i], "--meterfilesnaming", 18)) {
            if (strlen(argv[i]) > 18 && argv[i][18] == '=') {
                if (!strncmp(argv[i]+19, "name-id", 7))
                {
                    c->meterfiles_naming = MeterFileNaming::NameId;
                }
                else if (!strncmp(argv[i]+19, "name", 4))
                {
                    c->meterfiles_naming = MeterFileNaming::Name;
                }
                else if (!strncmp(argv[i]+19, "id", 2))
                {
                    c->meterfiles_naming = MeterFileNaming::Id;
                } else
                {
                    error("No such meter file naming %s\n", argv[i]+19);
                }
            } else {
                error("Incorrect option %s\n", argv[i]);
            }
            i++;
            continue;
        }
        if (!strncmp(argv[i], "--meterfilestimestamp", 21)) {
            if (strlen(argv[i]) > 22 && argv[i][21] == '=') {
                if (!strncmp(argv[i]+22, "day", 3))
                {
                    c->meterfiles_timestamp = MeterFileTimestamp::Day;
                }
                else if (!strncmp(argv[i]+22, "hour", 4))
                {
                    c->meterfiles_timestamp = MeterFileTimestamp::Hour;
                }
                else if (!strncmp(argv[i]+22, "minute", 6))
                {
                    c->meterfiles_timestamp = MeterFileTimestamp::Minute;
                }
                else if (!strncmp(argv[i]+22, "micros", 5))
                {
                    c->meterfiles_timestamp = MeterFileTimestamp::Micros;
                }
                else if (!strncmp(argv[i]+22, "never", 5))
                {
                    c->meterfiles_timestamp = MeterFileTimestamp::Never;
                } else
                {
                    error("No such meter file timestamp \"%s\"\n", argv[i]+22);
                }
            } else {
                error("Incorrect option %s\n", argv[i]);
            }
            i++;
            continue;
        }
        if (!strcmp(argv[i], "--meterfiles") ||
            (!strncmp(argv[i], "--meterfiles", 12) &&
             strlen(argv[i]) > 12 &&
             argv[i][12] == '='))
        {
            c->meterfiles = true;
            size_t len = strlen(argv[i]);
            if (len > 13) {
                c->meterfiles_dir = string(argv[i]+13, len-13);
            } else {
                c->meterfiles_dir = "/tmp";
            }
            if (!checkIfDirExists(c->meterfiles_dir.c_str())) {
                error("Cannot write meter files into dir \"%s\"\n", c->meterfiles_dir.c_str());
            }
            i++;
            continue;
        }
        if (!strncmp(argv[i], "--logfile=", 10)) {
            c->use_logfile = true;
            if (strlen(argv[i]) > 10) {
                size_t len = strlen(argv[i])-10;
                if (len > 0) {
                    c->logfile = string(argv[i]+10, len);
                } else {
                    error("Not a valid log file name.\n");
                }
            }
            i++;
            continue;
        }
        if (!strncmp(argv[i], "--usestderr", 11)) {
            c->use_stderr_for_log = true;
            i++;
            continue;
        }
        if (!strncmp(argv[i], "--ignoreduplicates", 18)) {
            if (argv[i][18] == 0)
            {
                c->ignore_duplicate_telegrams = true;
            }
            else
            {
                if (!strcmp(argv[i]+18, "=true"))
                {
                    c->ignore_duplicate_telegrams = true;
                }
                else if (!strcmp(argv[i]+18, "=false"))
                {
                    c->ignore_duplicate_telegrams = false;
                }
                else
                {
                    error("You must specify true or false after --ignoreduplicates=\n");
                }
            }
            i++;
            continue;
        }
        if (!strncmp(argv[i], "--usestdoutforlogging", 13)) {
            c->use_stderr_for_log = false;
            i++;
            continue;
        }
        if (!strncmp(argv[i], "--shell=", 8)) {
            string cmd = string(argv[i]+8);
            if (cmd == "") {
                error("The telegram shell command cannot be empty.\n");
            }
            c->telegram_shells.push_back(cmd);
            i++;
            continue;
        }
        if (!strncmp(argv[i], "--metershell=", 13)) {
            string cmd = string(argv[i]+13);
            if (cmd == "") {
                error("The meter shell command cannot be empty.\n");
            }
            c->meter_shells.push_back(cmd);
            i++;
            continue;
        }
        if (!strncmp(argv[i], "--alarmshell=", 13)) {
            string cmd = string(argv[i]+13);
            if (cmd == "") {
                error("The alarm shell command cannot be empty.\n");
            }
            c->alarm_shells.push_back(cmd);
            i++;
            continue;
        }
        if (!strncmp(argv[i], "--json_", 7) ||
            !strncmp(argv[i], "--field_", 8))
        {
            // For example: --json_floor=42
            // or           --field_floor=42
            // they are equivalent.
            int off = 7;
            if (!strncmp(argv[i], "--field_", 8)) { off = 8; }

            string extra_constant_field = string(argv[i]+off);
            if (extra_constant_field == "") {
                error("The extra constant field command cannot be empty.\n");
            }
            // The extra "floor"="42" will be pushed to the json.
            debug("Added extra constant field %s\n", extra_constant_field.c_str());
            c->extra_constant_fields.push_back(extra_constant_field);
            i++;
            continue;
        }
        if (!strncmp(argv[i], "--calculate_", 12))
        {
            // For example: --calculate_adjusted_kwh='total_kwh + 12345 kwh'
            string extra_calculated_field = string(argv[i]+12);
            if (extra_calculated_field == "") {
                error("The calculated field command cannot be empty.\n");
            }
            debug("(cmdline) add calculated field %s\n", extra_calculated_field.c_str());
            c->extra_calculated_fields.push_back(extra_calculated_field);
            i++;
            continue;
        }
        if (!strncmp(argv[i], "--listenvs=", 11)) {
            c->list_shell_envs = true;
            c->list_meter = string(argv[i]+11);
            i++;
            continue;
        }
        if (!strncmp(argv[i], "--listfields=", 13)) {
            c->list_fields = true;
            c->list_meter = string(argv[i]+13);
            i++;
            continue;
        }
        if (!strncmp(argv[i], "--listmeters=", 13)) {
            c->list_meters = true;
            c->list_meters_search = string(argv[i]+13);
            i++;
            continue;
        }
        else if (!strncmp(argv[i], "--listmeters", 12)) {
            c->list_meters = true;
            c->list_meters_search = "";
            i++;
            continue;
        }
        else if (!strcmp(argv[i], "--listunits")) {
            c->list_units = true;
            i++;
            continue;
        }
        if (!strcmp(argv[i], "--oneshot")) {
            c->oneshot = true;
            i++;
            continue;
        }
        if (!strncmp(argv[i], "--exitafter=", 12) && strlen(argv[i]) > 12) {
            c->exitafter = parseTime(argv[i]+12);
            if (c->exitafter <= 0) {
                error("Not a valid time to exit after. \"%s\"\n", argv[i]+12);
            }
            i++;
            continue;
        }
        if (!strcmp(argv[i], "--nodeviceexit")) {
            c->nodeviceexit = true;
            i++;
            continue;
        }
        if (!strncmp(argv[i], "--pollinterval=", 15) && strlen(argv[i]) > 15) {
            c->pollinterval = parseTime(argv[i]+15);
            if (c->pollinterval <= 0) {
                error("Not a valid time to regularly poll meters. \"%s\"\n", argv[i]+15);
            }
            i++;
            continue;
        }
        if (!strncmp(argv[i], "--resetafter=", 13) && strlen(argv[i]) > 13) {
            c->resetafter = parseTime(argv[i]+13);
            if (c->resetafter <= 0) {
                error("Not a valid time to regularly reset after. \"%s\"\n", argv[i]+13);
            }
            i++;
            continue;
        }
        if (!strncmp(argv[i], "--alarmtimeout=", 15)) {
            c->alarm_timeout = parseTime(argv[i]+15);
            if (c->alarm_timeout <= 0) {
                error("Not a valid alarm timeout. \"%s\"\n", argv[i]+15);
            }
            i++;
            continue;
        }
        if (!strncmp(argv[i], "--alarmexpectedactivity=", 24)) {
            string ea = string(argv[i]+24);
            if (!isValidTimePeriod(ea))
            {
                error("Not a valid time period string. \"%s\"\n", ea.c_str());
            }
            c->alarm_expected_activity = ea;
            i++;
            continue;
        }
        if (!strncmp(argv[i], "--driversdir=", 13))
        {
            size_t len = strlen(argv[i]) - 13;
            c->drivers_dir = string(argv[i]+13, len);
            if (!checkIfDirExists(c->drivers_dir.c_str()))
            {
                error("You must supply a valid directory to --driversdir=<dir>\n");
            }
            i++;
            loadDriversFromDir(c->drivers_dir);
            continue;
        }
        if (!strncmp(argv[i], "--driver=", 9))
        {
            size_t len = strlen(argv[i]) - 9;
            string file_name = string(argv[i]+9, len);
            if (!checkFileExists(file_name.c_str()))
            {
                error("You must supply a valid file to --driver=<file>\n");
            }
            i++;
            loadDriver(file_name, NULL);
            continue;
        }

        error("Unknown option \"%s\"\n", argv[i]);
    }

    bool found_at_least_one_device_or_hex = false;
    while (argv[i])
    {
        if (!strncmp(argv[i], "--", 2))
        {
            // We have found a device and the next parameter is an option.
            error("All command line options must be placed before the devices, %s is placed wrong.\n", argv[i]);
        }
        bool ok = handleDeviceOrHex(c, argv[i]);
        if (ok)
        {
            found_at_least_one_device_or_hex = true;
        }
        else
        {
            if (!found_at_least_one_device_or_hex)
            {
                error("At least one valid device (or hex) must be supplied!\n");
            }
            // There are more arguments...
            break;
        }
        i++;
    }

    if (c->supplied_bus_devices.size() == 0 &&
        c->use_auto_device_detect == false &&
        !c->list_shell_envs &&
        !c->list_fields &&
        !c->list_meters &&
        !c->list_units)
    {
        error("You must supply at least one device to communicate using (w)mbus.\n");
    }

    while (argv[i] != 0 && SendBusContent::isLikely(argv[i]))
    {
        SendBusContent sbc;
        bool ok = sbc.parse(argv[i]);
        if (!ok)
        {
            error("Not a valid send bus content command.\n");
        }
        c->send_bus_content.push_back(sbc);
        i++;
    }

    if ((argc-i) % 4 != 0) {
        error("For each meter you must supply a: name,type,id and key.\n");
    }
    int num_meters = (argc-i)/4;

    for (int m=0; m<num_meters; ++m)
    {
        string bus;
        string name = argv[m*4+i+0];
        string driver = argv[m*4+i+1];
        string address_expressions = argv[m*4+i+2];
        string key = argv[m*4+i+3];

        MeterInfo mi;
        mi.parse(name, driver, address_expressions, key);
        mi.poll_interval = c->pollinterval;

        if (mi.driver_name.str() == "")
        {
            error("Not a valid meter driver \"%s\"\n", driver.c_str());
        }

        c->meters.push_back(mi);
    }

    return shared_ptr<Configuration>(c);
}

shared_ptr<Configuration> parseCommandLineWithUseConfig(Configuration *c, int argc, char **argv, bool pid_file_expected)
{
    int i = 1;

    while (argv[i] && argv[i][0] == '-')
    {
        if (!strncmp(argv[i], "--useconfig", 11))
        {
            if (strlen(argv[i]) == 11)
            {
                c->useconfig = true;
                c->config_root = "";
            }
            else if (strlen(argv[i]) > 12 && argv[i][11] == '=')
            {
                size_t len = strlen(argv[i]) - 12;
                c->useconfig = true;
                c->config_root = string(argv[i]+12, len);
                if (c->config_root == "/") {
                    c->config_root = "";
                }
            }
            else
            {
                error("You must supply a directory to --useconfig=dir\n");
            }
            i++;
            continue;
        }
        if (!strncmp(argv[i], "--verbose", 9))
        {
            c->overrides.loglevel_override = "verbose";
            debug("(useconfig) loglevel override \"%s\"\n", c->overrides.loglevel_override.c_str());
            i++;
            continue;
        }
        if (!strncmp(argv[i], "--normal", 8))
        {
            c->overrides.loglevel_override = "normal";
            debug("(useconfig) loglevel override \"%s\"\n", c->overrides.loglevel_override.c_str());
            i++;
            continue;
        }
        if (!strncmp(argv[i], "--silent", 8))
        {
            c->overrides.loglevel_override = "silent";
            debug("(useconfig) loglevel override \"%s\"\n", c->overrides.loglevel_override.c_str());
            i++;
            continue;
        }
        if (!strncmp(argv[i], "--debug", 7))
        {
            c->overrides.loglevel_override = "debug";
            debug("(useconfig) loglevel override \"%s\"\n", c->overrides.loglevel_override.c_str());
            i++;
            continue;
        }
        if (!strncmp(argv[i], "--trace", 7))
        {
            c->overrides.loglevel_override = "trace";
            debug("(useconfig) loglevel override \"%s\"\n", c->overrides.loglevel_override.c_str());
            i++;
            continue;
        }
        if (!strncmp(argv[i], "--device=", 9)) // Deprecated
        {
            c->overrides.device_override = string(argv[i]+9);
            debug("(useconfig) device override \"%s\"\n", c->overrides.device_override.c_str());
            i++;
            continue;
        }
        if (!strncmp(argv[i], "--overridedevice=", 17))
        {
            c->overrides.device_override = string(argv[i]+17);
            debug("(useconfig) device override \"%s\"\n", c->overrides.device_override.c_str());
            i++;
            continue;
        }
        if (!strncmp(argv[i], "--listento=", 11))
        {
            c->overrides.listento_override = string(argv[i]+11);
            debug("(useconfig) listento override \"%s\"\n", c->overrides.listento_override.c_str());
            i++;
            continue;
        }
        if (!strncmp(argv[i], "--exitafter=", 12) && strlen(argv[i]) > 12) {
            int s = parseTime(argv[i]+12);
            if (s <= 0) {
                error("Not a valid time to exit after. \"%s\"\n", argv[i]+12);
            }
            c->overrides.exitafter_override = argv[i]+12;
            i++;
            continue;
        }
        if (!strcmp(argv[i], "--oneshot")) {
            c->overrides.oneshot_override = "true";
            i++;
            continue;
        }
        if (!strncmp(argv[i], "--logfile=", 10)) {
            size_t len = strlen(argv[i])-10;
            if (len > 0) {
                c->overrides.logfile_override = string(argv[i]+10, len);
            } else {
                error("Not a valid log file name.\n");
            }
            i++;
            continue;
        }

        error("Usage error: --useconfig=... can only be used in combination with:\n"
              "--overridedevice= --listento= --exitafter= --oneshot= --logfile= --silent --normal --verbose --debug --trace\n");
        break;
    }

    if (pid_file_expected)
    {
        if (!argv[i])
        {
            error("Usage error: you must supply the pid file as the last argument to wmbusmetersd.\n");
        }
        if (argv[i][0] == '-')
        {
            error("Usage error: the pid file name must not start with minus (-). Please change \"%s\".\n", argv[i]);
        }
        c->pid_file = argv[i];
        i++;

        if (i < argc)
        {
            error("Usage error: you must supply the pid file as the last argument to wmbusmetersd.\n");
        }
    }
    else
    {
        if (i < argc)
        {
            error("Usage error: too many arguments \"%s\" with --useconfig=...\n", argv[i]);
        }
    }
    return shared_ptr<Configuration>(c);
}

static bool isDaemon(int argc, char **argv)
{
    const char *filename = strrchr(argv[0], '/');

    if (filename)
    {
        filename++;
    }
    else
    {
        filename = argv[0];
    }

    return !strcmp(filename, "wmbusmetersd");
}

static bool checkIfUseConfig(int argc, char **argv)
{
    while (*argv != NULL)
    {
        if (!strncmp(*argv, "--useconfig", 11)) return true;
        argv++;
    }

    return false;
}
