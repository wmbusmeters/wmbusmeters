/*
 Copyright (C) 2019-2021 Fredrik Öhrström (gpl-3.0-or-later)

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
#include<set>
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

enum class LogSummary
{
    All, Unknown
};

// These values can be overridden from the command line.
struct ConfigOverrides
{
    std::string loglevel_override;
    std::string device_override;
    std::string listento_override;
    std::string exitafter_override;
    std::string oneshot_override;
    std::string logfile_override;
};

struct Configuration
{
    string bin_dir {}; // The wmbusmeters binary executed is located here.
                       // Use this directory to look for other tools such as rtl_wmbus/rtl_sdr
                       // inside the same directory.
    bool daemon {};
    std::string pid_file;
    ConfigOverrides overrides;
    bool useconfig {};
    std::string config_root;
    bool need_help {};
    bool silent {};
    bool verbose {};
    bool version {};
    bool license {};
    bool analyze {};
    OutputFormat analyze_format {};
    string analyze_driver {};
    string analyze_key {};
    bool analyze_verbose {};
    bool debug {};
    bool trace {};
    AddLogTimestamps addtimestamps {};
    bool internaltesting {}; // Not currently used. Was used for speeding up testing. I.e. it shortened all timeouts.
                             // Might be needed in the future. Therefore it is still here.
    bool logsummary {};
    bool logtelegrams {};
    bool meterfiles {};
    std::string meterfiles_dir;
    MeterFileType meterfiles_action {};
    MeterFileNaming meterfiles_naming {};
    MeterFileTimestamp meterfiles_timestamp {}; // Default is never.
    bool use_logfile {};
    bool use_stderr_for_log = true; // Default is to use stderr for logging.
    bool ignore_duplicate_telegrams = true; // Default is to ignore duplicates.
    std::string logfile;
    bool json {};
    bool pretty_print_json {};
    int  pollinterval {}; // Time between polling of mbus meters.
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
    bool list_units {};
    std::string list_meters_search;
    // When asking for envs or fields, this is the meter type to list for.
    std::string list_meter;
    bool oneshot {};
    int  exitafter {}; // Seconds to exit.
    bool nodeviceexit {}; // If no wmbus receiver device is found, then exit immediately!
    int  resetafter {}; // Reset the wmbus devices regularly.
    std::vector<SpecifiedDevice> supplied_bus_devices; // /dev/ttyUSB0, simulation.txt, rtlwmbus, /dev/ttyUSB1:9600 /dev/ttyUSB2:mbus
    int num_wmbus_devices {};
    int num_mbus_devices {};
    bool use_auto_device_detect {}; // Set to true if auto was supplied as device.
    std::set<std::string> do_not_probe_ttys; // Do not probe these ttys! all = all of them.
    LinkModeSet auto_device_linkmodes; // The linkmodes specified by auto:c1,t1
    bool single_device_override {}; // Set to true if there is a stdin/file or simulation device.
    bool simulation_found {};
    LinkModeSet default_device_linkmodes; // Backwards compatible --listento=c1 or --c1 will set the default_linkmodes.
                                          // A device without a :t1 suffix, will use this linkmode.
                                          // Is empty when not set.
    LinkModeSet all_device_linkmodes_specified; // A union of all device specified linkmodes.
                                                // Eventually not all devices might be found, so a realtime check is done later.
    LinkModeSet all_meters_linkmodes_specified; // A union of all meters linkmodes.
    string telegram_reader;
    // A set of all link modes (union) that the user requests the wmbus dongle to listen to.
    bool no_init {};
    std::vector<std::string> selected_fields;
    std::vector<MeterInfo> meters;
    std::vector<std::string> extra_constant_fields; // Additional constant fields to always add to json.
    std::vector<std::string> extra_calculated_fields; // Additional calculated fields to always add to json.
    // These extra constant fields can also be part of selected with selectfields.
    std::vector<SendBusContent> send_bus_content; // Telegrams used to wake up a meter for reading or mbus read-out requests.
    std::set<BusDeviceType> probe_for; // Which devices should be probed for? DEVICE_AUTO means all.
    ~Configuration() = default;
};

shared_ptr<Configuration> loadConfiguration(string root, ConfigOverrides overrides);

void parseMeterConfig(Configuration *c, vector<char> &buf, string file);
void handleSelectedFields(Configuration *c, string s);
void handleAddedFields(Configuration *c, string s);
bool handleDeviceOrHex(Configuration *c, string devicefilehex);

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

LinkModeCalculationResult calculateLinkModes(Configuration *c, BusDevice *device, bool link_modes_matter = true);

#endif
