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

#ifndef CONFIG_H
#define CONFIG_H

#include"units.h"
#include"util.h"
#include"wmbus.h"
#include"meters.h"
#include<vector>

using namespace std;

enum class MeterFileType
{
    Overwrite, Append
};

enum class MeterFileNaming
{
    Name, Id, NameId
};

enum class MeterFileTimestamp
{
    Never, Day, Hour, Minute, Micros
};

struct Configuration
{
    bool daemon {};
    std::string pid_file;
    std::string device_override;
    std::string listento_override;
    bool useconfig {};
    std::string config_root;
    bool reload {};
    bool need_help {};
    bool silence {};
    bool verbose {};
    bool version {};
    bool license {};
    bool debug {};
    bool trace {};
    bool internaltesting {}; // Only for testing! When true, shorten all timeouts.
    bool logtelegrams {};
    bool meterfiles {};
    std::string meterfiles_dir;
    MeterFileType meterfiles_action {};
    MeterFileNaming meterfiles_naming {};
    MeterFileTimestamp meterfiles_timestamp {}; // Default is never.
    bool use_logfile {};
    bool use_stderr_for_log = true; // Default is to use stderr for logging.
    bool ignore_duplicate_telegrams = false; // Default is to report all telegrams.
    std::string logfile;
    bool json {};
    bool fields {};
    char separator { ';' };
    std::vector<std::string> telegram_shells;
    std::vector<std::string> alarm_shells;
    int alarm_timeout {}; // Maximum number of seconds between dongle receiving two telegrams.
    std::string alarm_expected_activity; // Only warn when within these time periods.
    bool exit_instead_of_alarm_ {};
    bool list_shell_envs {};
    bool list_fields {};
    bool list_meters {};
    std::string list_meters_search;
    // When asking for envs or fields, this is the meter type to list for.
    std::string list_meter;
    bool oneshot {};
    int  exitafter {}; // Seconds to exit.
    int  resetafter {}; // Reset the wmbus devices regularly.
    std::vector<SpecifiedDevice> supplied_wmbus_devices; // /dev/ttyUSB0, simulation.txt, rtlwmbus, /dev/ttyUSB1:9600
    bool use_auto_detect {}; // Set to true if auto was supplied as device.
    bool single_device_override {}; // Set to true if there is a stdin/file or simulation device.
    bool simulation_found {};
    LinkModeSet linkmodes; // If --c1 or auto:c1 then store c1 here.
    bool linkmodes_configured {}; // Either auto:c1 or --c1 is specified.
    string telegram_reader;
    // A set of all link modes (union) that the user requests the wmbus dongle to listen to.
    bool no_init {};
    std::vector<Unit> conversions;
    std::vector<std::string> selected_fields;
    std::vector<MeterInfo> meters;
    std::vector<std::string> jsons; // Additional jsons to always add.

    ~Configuration() = default;
};

shared_ptr<Configuration> loadConfiguration(string root, string device_override, string listento_override);

void handleConversions(Configuration *c, string s);
void handleSelectedFields(Configuration *c, string s);
bool handleDevice(Configuration *c, string devicefile);

enum class LinkModeCalculationResultType
{
    Success,
    NoMetersMustSupplyModes,
    AutomaticDeductionFailed,
    DongleCannotListenTo,
    MightMissTelegrams
};

struct LinkModeCalculationResult
{
    LinkModeCalculationResultType type;
    std::string msg;
};

LinkModeCalculationResult calculateLinkModes(Configuration *c, WMBus *wmbus, bool link_modes_matter = true);

#endif
