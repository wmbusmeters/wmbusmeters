/*
 Copyright (C) 2017-2020 Fredrik Öhrström

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
#include"config.h"
#include"meters.h"
#include"printer.h"
#include"rtlsdr.h"
#include"serial.h"
#include"shell.h"
#include"threads.h"
#include"util.h"
#include"version.h"
#include"wmbus.h"

#include <algorithm>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <semaphore.h>
#include <set>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <syslog.h>
#include <unistd.h>

using namespace std;

int main(int argc, char **argv);
void check_if_multiple_wmbus_meters_running();
void check_for_dead_wmbus_devices(Configuration *config);
shared_ptr<Meter> create_meter(Configuration *config, MeterType type, MeterInfo *mi, const char *keymsg);
shared_ptr<Printer> create_printer(Configuration *config);
shared_ptr<WMBus> create_wmbus_object(Detected *detected, Configuration *config, shared_ptr<SerialCommunicationManager> manager);
enum class DetectionType { STDIN_FILE_SIMULATION, ALL };
void detect_and_configure_wmbus_devices(Configuration *config, DetectionType dt);
SpecifiedDevice *find_specified_device_from_detected(Configuration *c, Detected *d);
bool find_specified_device_and_update_detected(Configuration *c, Detected *d);
void find_specified_device_and_mark_as_handled(Configuration *c, Detected *d);
void list_fields(Configuration *config, string meter_type);
void list_shell_envs(Configuration *config, string meter_type);
void list_meters(Configuration *config);
void log_start_information(Configuration *config);
void oneshot_check(Configuration *config, Telegram *t, Meter *meter);
void open_wmbus_device_and_set_linkmodes(Configuration *config, string how, Detected *detected);
void perform_auto_scan_of_serial_devices(Configuration *config);
void perform_auto_scan_of_swradio_devices(Configuration *config);
void regular_checkup(Configuration *config);
void remove_lost_serial_devices_from_ignore_list(vector<string> &devices);
void remove_lost_swradio_devices_from_ignore_list(vector<string> &devices);
bool start(Configuration *config);
void start_using_config_files(string root, bool is_daemon, string device_override, string listento_override);
void start_daemon(string pid_file, string device_override, string listento_override); // Will use config files.
void setup_log_file(Configuration *config);
void setup_meters(Configuration *config, MeterManager *manager);
void write_pid(string pid_file, int pid);

// The serial communication manager takes care of
// monitoring the file descrtiptors for the ttys,
// background shells, files and stdin. It also invokes
// regular callbacks used for monitoring alarms and
// detecting new devices.
shared_ptr<SerialCommunicationManager> serial_manager_;

// Manage registered meters to decode and relay.
shared_ptr<MeterManager> meter_manager_;

// Current active set of wmbus devices that can receive telegrams.
// This can change during runtime, plugging/unplugging wmbus dongles.
vector<shared_ptr<WMBus>> wmbus_devices_;
RecursiveMutex wmbus_devices_mutex_("wmbus_devices_mutex");
#define LOCK_WMBUS_DEVICES(where) WITH(wmbus_devices_mutex_, where)

// Remember devices that were not detected as wmbus devices.
// To avoid probing them again and again.
set<string> not_serial_wmbus_devices_;

// The software radio devices are always swradio devices
// but they might not be available for wmbusmeters.
set<string> not_swradio_wmbus_devices_;

// When manually supplying stdin or a file, then, after
// it has been read, do not open it again!
set<string> do_not_open_file_again_;

// Store simulation files here.
set<string> simulation_files_;

// Rendering the telegrams to json,fields or shell calls is
// done by the printer.
shared_ptr<Printer> printer_;

// Set as true when the warning for no detected wmbus devices has been printed.
bool printed_warning_ = false;

// Then check if the rtl_sdr and/or rtl_wmbus is in the path.
bool rtlsdr_found_ = false;
bool rtlwmbus_found_ = false;

int main(int argc, char **argv)
{
    auto config = parseCommandLine(argc, argv);

    if (config->version)
    {
        printf("wmbusmeters: " VERSION "\n");
        printf(COMMIT "\n");
        exit(0);
    }

    if (config->license)
    {
        const char * license = R"LICENSE(
Copyright (C) 2017-2020 Fredrik Öhrström

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

You can download the source here: https://github.com/weetmuts/wmbusmeters
But you can also request the source from the person/company that
provided you with this binary. Read the full license for all details.

)LICENSE";
        puts(license);
        exit(0);
    }

    if (config->list_shell_envs)
    {
        list_shell_envs(config.get(), config->list_meter);
        exit(0);
    }

    if (config->list_fields)
    {
        list_fields(config.get(), config->list_meter);
        exit(0);
    }

    if (config->list_meters)
    {
        list_meters(config.get());
        exit(0);
    }

    if (config->need_help)
    {
        printf("wmbusmeters version: " VERSION "\n");
        const char *short_manual =
#include"short_manual.h"
        puts(short_manual);
        exit(0);
    }

    if (config->daemon)
    {
        start_daemon(config->pid_file, config->device_override, config->listento_override);
        exit(0);
    }

    if (config->useconfig)
    {
        start_using_config_files(config->config_root, false, config->device_override, config->listento_override);
        exit(0);
    }
    else
    {
        // We want the data visible in the log file asap!
        setbuf(stdout, NULL);
        start(config.get());
        exit(0);
    }
    error("(main) internal error\n");
}

void check_if_multiple_wmbus_meters_running()
{
    pid_t my_pid = getpid();
    vector<int> daemons;
    detectProcesses("wmbusmetersd", &daemons);
    for (int i : daemons)
    {
        if (i != my_pid)
        {
            info("Notice! Wmbusmeters daemon (pid %d) is running and it might hog any wmbus devices.\n", i);
        }
    }

    vector<int> processes;
    detectProcesses("wmbusmeters", &processes);

    for (int i : processes)
    {
        if (i != my_pid)
        {
            info("Notice! Other wmbusmeters (pid %d) is running and it might hog any wmbus devices.\n", i);
        }
    }
}

void check_for_dead_wmbus_devices(Configuration *config)
{
    LOCK_WMBUS_DEVICES(check_for_wmbus_devices);

    trace("[MAIN] checking for dead wmbus devices...\n");

    vector<WMBus*> not_working;
    for (auto &w : wmbus_devices_)
    {
        if (!w->isWorking())
        {
            not_working.push_back(w.get());

            string id = w->getDeviceId();
            if (id != "") id = "["+id+"]";

            notice("Lost %s closing %s%s\n",
                   w->device().c_str(),
                   toLowerCaseString(w->type()),
                   id.c_str());

            w->close();
        }
    }

    for (auto w : not_working)
    {
        auto i = wmbus_devices_.begin();
        while (i != wmbus_devices_.end())
        {
            if (w == (*i).get())
            {
                // The erased shared_ptr will delete the WMBus object.
                wmbus_devices_.erase(i);
                break;
            }
            i++;
        }
    }

    if (wmbus_devices_.size() == 0)
    {
        if (config->single_device_override)
        {
            if (!config->simulation_found)
            {
                // Expect stdin/file to work.
                // Simulation is special since it will self stop the serial manager.
                serial_manager_->expectDevicesToWork();
            }
        }
        else
        {
            if (config->nodeviceexit)
            {
                if (!printed_warning_)
                {
                    notice("No wmbus device detected. Exiting!\n");
                    serial_manager_->stop();
                    printed_warning_ = true;
                }
            }
            else
            {
                if (!printed_warning_)
                {
                    info("No wmbus device detected, waiting for a device to be plugged in.\n");
                    check_if_multiple_wmbus_meters_running();
                    printed_warning_ = true;
                }
            }
        }
    }
    else
    {
        printed_warning_ = false;
    }
}

shared_ptr<Meter> create_meter(Configuration *config, MeterType type, MeterInfo *mi, const char *keymsg)
{
    shared_ptr<Meter> newm;

    switch (type)
    {
#define X(mname,link,info,type,cname) \
        case MeterType::type:                              \
        {                                                  \
            newm = create##cname(*mi);                      \
            newm->addConversions(config->conversions);     \
            verbose("(main) configured \"%s\" \"" #mname "\" \"%s\" %s\n", \
                    mi->name.c_str(), mi->id.c_str(), keymsg);              \
            return newm;                                                \
        }                                                               \
        break;
LIST_OF_METERS
#undef X
    case MeterType::UNKNOWN:
        error("No such meter type \"%s\"\n", mi->type.c_str());
        break;
    }
    return newm;
}

shared_ptr<WMBus> create_wmbus_object(Detected *detected, Configuration *config,
                                      shared_ptr<SerialCommunicationManager> manager)
{
    shared_ptr<WMBus> wmbus;

    shared_ptr<SerialDevice> serial_override;

    if (detected->found_tty_override)
    {
        serial_override = manager->createSerialDeviceFile(detected->specified_device.file,
                                                          string("override ")+detected->specified_device.file.c_str());
        verbose("(serial) override with devicefile: %s\n", detected->specified_device.file.c_str());
    }

    switch (detected->found_type)
    {
    case DEVICE_AUTO:
        assert(0);
        error("Internal error DEVICE_AUTO should not be used here!\n");
        break;
    case DEVICE_IM871A:
        verbose("(im871a) on %s\n", detected->found_file.c_str());
        wmbus = openIM871A(detected->found_file, manager, serial_override);
        break;
    case DEVICE_AMB8465:
        verbose("(amb8465) on %s\n", detected->found_file.c_str());
        wmbus = openAMB8465(detected->found_file, manager, serial_override);
        break;
    case DEVICE_SIMULATION:
        verbose("(simulation) in %s\n", detected->found_file.c_str());
        wmbus = openSimulator(detected->found_file, manager, serial_override);
        break;
    case DEVICE_RAWTTY:
        verbose("(rawtty) on %s\n", detected->found_file.c_str());
        wmbus = openRawTTY(detected->found_file, detected->found_bps, manager, serial_override);
        break;
    case DEVICE_RTLWMBUS:
    {
        string command;
        string identifier = detected->found_device_id;
        int id = 0;

        if (!detected->found_tty_override)
        {
            id = indexFromRtlSdrSerial(identifier);

            command = "";
            if (detected->found_command != "")
            {
                command = detected->found_command;
                identifier = "cmd_"+to_string(detected->specified_device.index);
            }
            string freq = "868.7M";
			string simultaneous_s1_c1_t1_flag=" -s";
            if (detected->specified_device.fq != "")
            {
                freq = detected->specified_device.fq;
				//todo: test if .fq <> 868,7 and set flag, otherwise unset
				simultaneous_s1_c1_t1_flag= ""; //no simultaneus capture when frequency is supplied  
				
            }
            string prefix = "";
            if (config->daemon)
            {
                prefix = "/usr/bin/";
                if (command == "")
                {
                    // Default command is used, check that the binaries are in place.
                    if (!checkFileExists("/usr/bin/rtl_sdr"))
                    {
                        error("(rtlwmbus) error: when starting as daemon, wmbusmeters expects /usr/bin/rtl_sdr to exist!\n");
                    }
                    if (!checkFileExists("/usr/bin/rtl_wmbus"))
                    {
                        error("(rtlwmbus) error: when starting as daemon, wmbusmeters expects /usr/bin/rtl_wmbus to exist!\n");
                    }
                }
            }
            if (command == "") {
                command = prefix+"rtl_sdr -d "+to_string(id)+" -f "+freq+" -s 1.6e6 - 2>/dev/null | "+prefix+"rtl_wmbus"+simultaneous_s1_c1_t1_flag;
            }
            verbose("(rtlwmbus) using command: %s\n", command.c_str());
        }
        wmbus = openRTLWMBUS(identifier, command, manager,
                             [command](){
                                 warning("(rtlwmbus) child process exited! "
                                         "Command was: \"%s\"\n", command.c_str());
                             },
                             serial_override);
        break;
    }
    case DEVICE_RTL433:
    {
        string command;
        string identifier = detected->found_file;
        int id = 0;
        if (!detected->found_tty_override)
        {
            id = indexFromRtlSdrName(identifier);
            command = "";
            if (detected->specified_device.command != "")
            {
                command = detected->specified_device.command;
                identifier = "cmd_"+to_string(detected->specified_device.index);
            }
            string freq = "868.95M";
            if (detected->specified_device.fq != "")
            {
                freq = detected->specified_device.fq;
            }
            string prefix = "";
            if (config->daemon) {
                prefix = "/usr/bin/";
                if (command == "")
                {
                    // Default command is used, check that the binaries are in place.
                    if (!checkFileExists("/usr/bin/rtl_433"))
                    {
                        error("(rtl433) error: when starting as daemon, wmbusmeters expects /usr/bin/rtl_433 to exist!\n");
                    }
                }
            }
            if (command == "") {
                command = prefix+"rtl_433 -d "+to_string(id)+" -F csv -f "+freq;
            }
            verbose("(rtl433) using command: %s\n", command.c_str());
        }
        wmbus = openRTL433(identifier, command, manager,
                             [command](){
                                 warning("(rtl433) child process exited! "
                                         "Command was: \"%s\"\n", command.c_str());
                             },
                             serial_override);
        break;
    }
    case DEVICE_CUL:
    {
        verbose("(cul) on %s\n", detected->found_file.c_str());
        wmbus = openCUL(detected->found_file, manager, serial_override);
        break;
    }
    case DEVICE_RC1180:
    {
        verbose("(rc1180) on %s\n", detected->found_file.c_str());
        wmbus = openRC1180(detected->found_file, manager, serial_override);
        break;
    }
    case DEVICE_UNKNOWN:
        warning("(main) internal error! cannot create an unknown device! exiting!\n");
        if (config->daemon) {
            // If starting as a daemon, wait a bit so that systemd have time to catch up.
            sleep(1);
        }
        exit(1);
        break;
    }

    if (detected->found_device_id != "" &&  !detected->found_tty_override)
    {
        string did = wmbus->getDeviceId();
        if (did != detected->found_device_id && detected->found_type != DEVICE_RTLWMBUS)
        {
            warning("Not the expected dongle (dongle said %s, you said %s!\n", did.c_str(), detected->found_device_id.c_str());
            return NULL;
        }
    }
    wmbus->setDetected(*detected);
    return wmbus;
}

shared_ptr<Printer> create_printer(Configuration *config)
{
    return shared_ptr<Printer>(new Printer(config->json, config->fields,
                                           config->separator, config->meterfiles, config->meterfiles_dir,
                                           config->use_logfile, config->logfile,
                                           config->telegram_shells,
                                           config->meterfiles_action == MeterFileType::Overwrite,
                                           config->meterfiles_naming,
                                           config->meterfiles_timestamp));
}

void detect_and_configure_wmbus_devices(Configuration *config, DetectionType dt)
{
    check_for_dead_wmbus_devices(config);

    bool must_auto_find_ttys = false;
    bool must_auto_find_rtlsdrs = false;

    // The device=auto has been specified....
    if (config->use_auto_device_detect && dt == DetectionType::ALL)
    {
        must_auto_find_ttys = true;
        must_auto_find_rtlsdrs = true;
    }

    for (SpecifiedDevice &specified_device : config->supplied_wmbus_devices)
    {
        specified_device.handled = false;
        if (dt != DetectionType::ALL)
        {
            if (specified_device.is_tty || (!specified_device.is_stdin && !specified_device.is_file && !specified_device.is_simulation))
            {
                // The event loop has not yet started and this is not stdin nor a file, nor a simulation file.
                // Therefore, do not try to detect it yet!
                continue;
            }
        }
        if (specified_device.file == "" && specified_device.command == "")
        {
            // File/tty/command not specified, use auto scan later to find actual device file/tty.
            must_auto_find_ttys |= usesTTY(specified_device.type);
            must_auto_find_rtlsdrs |= usesRTLSDR(specified_device.type);
            continue;
        }
        if (specified_device.command != "")
        {
            string identifier = "cmd_"+to_string(specified_device.index);
            shared_ptr<SerialDevice> sd = serial_manager_->lookup(identifier);
            if (sd != NULL)
            {
                trace("(main) command %s already configured\n", identifier.c_str());
                specified_device.handled = true;
                continue;
            }
            Detected detected = detectWMBusDeviceWithCommand(specified_device, config->default_device_linkmodes, serial_manager_);
            specified_device.handled = true;
            open_wmbus_device_and_set_linkmodes(config, "config", &detected);
        }
        if (specified_device.file != "")
        {
            shared_ptr<SerialDevice> sd = serial_manager_->lookup(specified_device.file);
            if (sd != NULL)
            {
                trace("(main) %s already configured\n", sd->device().c_str());
                specified_device.handled = true;
                continue;
            }
            if (simulation_files_.count(specified_device.file) > 0)
            {
                debug("(main) %s already configured as simulation\n", specified_device.file.c_str());
                specified_device.handled = true;
                continue;
            }
            if (do_not_open_file_again_.count(specified_device.file) > 0)
            {
                // This was stdin/file, it should only be opened once.
                trace("[MAIN] ignoring handled file %s\n", specified_device.file.c_str());
                specified_device.handled = true;
                continue;
            }

            if (not_serial_wmbus_devices_.count(specified_device.file) > 0)
            {
                // Enumerate all serial devices that might connect to a wmbus device.
                vector<string> ttys = serial_manager_->listSerialTTYs();
                // Did a non-wmbus-device get unplugged? Then remove it from the known-not-wmbus-device set.
                remove_lost_serial_devices_from_ignore_list(ttys);
                if (not_serial_wmbus_devices_.count(specified_device.file) > 0)
                {
                    trace("[MAIN] ignoring failed file %s\n", specified_device.file.c_str());
                    specified_device.handled = true;
                    continue;
                }
            }

            if (!checkCharacterDeviceExists(specified_device.file.c_str(), false) &&
                !checkFileExists(specified_device.file.c_str()) &&
                specified_device.file != "stdin")
            {
                trace("Cannot open %s, no such device.\n", specified_device.file.c_str(),
                        specified_device.str().c_str());
                continue;
            }

            Detected detected = detectWMBusDeviceWithFile(specified_device, config->default_device_linkmodes, serial_manager_);

            if (detected.found_type == DEVICE_UNKNOWN)
            {
                if (checkCharacterDeviceExists(specified_device.file.c_str(), false))
                {
                    // Yes, this device actually exists, there is a need to ignore it.
                    not_serial_wmbus_devices_.insert(specified_device.file);
                }
            }

            if (detected.specified_device.is_stdin || detected.specified_device.is_file || detected.specified_device.is_simulation)
            {
                // Only read stdin and files once!
                do_not_open_file_again_.insert(specified_device.file);
            }
            open_wmbus_device_and_set_linkmodes(config, "config", &detected);
        }

        specified_device.handled = true;
    }

    if (must_auto_find_ttys)
    {
        perform_auto_scan_of_serial_devices(config);
    }

    if (must_auto_find_rtlsdrs)
    {
        perform_auto_scan_of_swradio_devices(config);
    }

    for (shared_ptr<WMBus> &wmbus : wmbus_devices_)
    {
        assert(wmbus->getDetected() != NULL);
        find_specified_device_and_mark_as_handled(config, wmbus->getDetected());
    }

    for (SpecifiedDevice &specified_device : config->supplied_wmbus_devices)
    {
        if (dt == DetectionType::ALL && !specified_device.handled)
        {
            time_t last_alarm = specified_device.last_alarm;
            time_t now = time(NULL);

            // If the device is missing, warn once per minute.
            if (now - last_alarm > 60)
            {
                specified_device.last_alarm = now;
                string device = specified_device.str();
                string info = tostrprintf("the device %s is not working", device.c_str());
                logAlarm(Alarm::SpecifiedDeviceNotFound, info);
            }
        }
    }
}

SpecifiedDevice *find_specified_device_from_detected(Configuration *c, Detected *d)
{
    // Iterate over the supplied devices and look for an exact type+id match.
    // This will find specified devices like: im871a[12345678]
    for (SpecifiedDevice & sd : c->supplied_wmbus_devices)
    {
        if (sd.file == "" && sd.id != "" && sd.id == d->found_device_id && sd.type == d->found_type)
        {
            return &sd;
        }
    }

    // Iterate over the supplied devices and look for a type match.
    // This will find specified devices like: im871a, rtlwmbus
    for (SpecifiedDevice & sd : c->supplied_wmbus_devices)
    {
        if (sd.file == "" && sd.id == "" && sd.type == d->found_type)
        {
            return &sd;
        }
    }

    return NULL;
}

bool find_specified_device_and_update_detected(Configuration *c, Detected *d)
{
    SpecifiedDevice *sd = find_specified_device_from_detected(c, d);

    if (sd)
    {
        d->specified_device = *sd;
        debug("(main) found specified device (%s) that matches detected device (%s)\n",
              sd->str().c_str(),
              d->str().c_str());
        return true;
    }

    return false;
}

void find_specified_device_and_mark_as_handled(Configuration *c, Detected *d)
{
    SpecifiedDevice *sd = find_specified_device_from_detected(c, d);

    if (sd)
    {
        sd->handled = true;
    }
}

void list_shell_envs(Configuration *config, string meter_type)
{
    string ignore1, ignore2, ignore3;
    vector<string> envs;
    Telegram t;
    MeterInfo mi;
    shared_ptr<Meter> meter = create_meter(config, toMeterType(meter_type), &mi, "");
    meter->printMeter(&t,
                      &ignore1,
                      &ignore2, config->separator,
                      &ignore3,
                      &envs,
                      &config->jsons,
                      &config->selected_fields);

    for (auto &e : envs)
    {
        int p = e.find('=');
        string key = e.substr(0,p);
        printf("%s\n", key.c_str());
    }
}

void list_fields(Configuration *config, string meter_type)
{
    MeterInfo mi;
    shared_ptr<Meter> meter = create_meter(config, toMeterType(meter_type), &mi, "");

    int width = 0;
    for (auto &p : meter->prints())
    {
        if ((int)p.field_name.size() > width) width = p.field_name.size();
    }

    string id = padLeft("id", width);
    printf("%s  The meter id number.\n", id.c_str());
    string name = padLeft("name", width);
    printf("%s  Your name for the meter.\n", name.c_str());
    string media = padLeft("media", width);
    printf("%s  What does the meter measure?\n", media.c_str());
    string meterr = padLeft("meter", width);
    printf("%s  Meter driver.\n", meterr.c_str());
    string timestamp = padLeft("timestamp", width);
    printf("%s  Timestamp when wmbusmeters received the telegram.\n", timestamp.c_str());
    string device = padLeft("device", width);
    printf("%s  The wmbus device that received the telegram.\n", device.c_str());
    string rssi = padLeft("rssi_dbm", width);
    printf("%s  The rssi for the received telegram as reported by the device.\n", rssi.c_str());
    for (auto &p : meter->prints())
    {
        if (p.vname == "") continue;
        string fn = padLeft(p.field_name, width);
        printf("%s  %s\n", fn.c_str(), p.help.c_str());
    }
}

void list_meters(Configuration *config)
{
#define X(mname,link,info,type,cname) \
    if (config->list_meters_search == "" || \
        stringFoundCaseIgnored(#info, config->list_meters_search) || \
        stringFoundCaseIgnored(#mname, config->list_meters_search)) \
            printf("%-14s %s\n", #mname, #info);
LIST_OF_METERS
#undef X
}

void log_start_information(Configuration *config)
{
    verbose("(wmbusmeters) version: " VERSION "\n");

    if (config->exitafter != 0) {
        verbose("(config) wmbusmeters will exit after %d seconds\n", config->exitafter);
    }

    if (config->meterfiles) {
        verbose("(config) store meter files in: \"%s\"\n", config->meterfiles_dir.c_str());
    }

    for (SpecifiedDevice &specified_device : config->supplied_wmbus_devices)
    {
        verbose("(config) using device: %s \n", specified_device.str().c_str());
    }
    verbose("(config) number of meters: %d\n", config->meters.size());
}

void oneshot_check(Configuration *config, Telegram *t, Meter *meter)
{
    if (!config->oneshot) return;

    if (meter_manager_->hasAllMetersReceivedATelegram())
    {
        // All meters have received at least one update! Stop!
        verbose("(main) all meters have received at least one update, stopping.\n");
        serial_manager_->stop();
    }
}

void open_wmbus_device_and_set_linkmodes(Configuration *config, string how, Detected *detected)
{
    if (detected->found_type == WMBusDeviceType::DEVICE_UNKNOWN)
    {
        debug("(verbose) ignoring device %s\n", detected->specified_device.str().c_str());
        return;
    }

    LOCK_WMBUS_DEVICES(open_wmbus_device);

    debug("(main) opening %s\n", detected->specified_device.str().c_str());

/*    if (detected->found_type == DEVICE_UNKNOWN)
    {
        // This is a manual config, lets detect and check the device properly.
        AccessCheck ac = reDetectDevice(detected, serial_manager_);
        if (ac != AccessCheck::AccessOK)
        {
            error("Could not open device %s\n", detected->specified_device.str().c_str());
        }
    }
*/
    LinkModeSet lms = detected->specified_device.linkmodes;
    if (lms.empty())
    {
        if (config->use_auto_device_detect)
        {
            lms = config->auto_device_linkmodes;
        }
        if (lms.empty())
        {
            lms = config->default_device_linkmodes;
        }
    }
    string using_link_modes = lms.hr();

    string id = detected->found_device_id.c_str();
    if (id != "") id = "["+id+"]";
    string fq = detected->specified_device.fq;
    if (fq != "") fq = " using fq "+fq;
    string file = detected->found_file.c_str();
    if (file != "") file = " on "+file;
    string cmd = detected->found_command.c_str();
    if (cmd != "")
    {
        cmd = " using CMD("+cmd+")";
    }

    string started = tostrprintf("Started %s %s%s%s listening on %s%s%s\n",
                                 how.c_str(),
                                 toLowerCaseString(detected->found_type),
                                 id.c_str(),
                                 file.c_str(),
                                 using_link_modes.c_str(),
                                 fq.c_str(),
                                 cmd.c_str());

    // A newly plugged in device has been manually configured or automatically detected! Start using it!
    if (config->use_auto_device_detect || detected->found_type != DEVICE_SIMULATION)
    {
        notice("%s", started.c_str());
    }
    else
    {
        // Hide the started when running simulations.
        verbose("%s", started.c_str());
    }

    shared_ptr<WMBus> wmbus = create_wmbus_object(detected, config, serial_manager_);
    if (wmbus == NULL) return;
    wmbus_devices_.push_back(wmbus);

    // By default, reset your dongle once every 23 hours,
    // so that the reset is not at the exact same time every day.
    int regular_reset = 23*3600;
    if (config->resetafter != 0) regular_reset = config->resetafter;
    wmbus->setResetInterval(regular_reset);
    verbose("(main) regular reset of %s %s%s will happen every %d seconds\n",
            toString(detected->found_type), file.c_str(), cmd.c_str(), regular_reset);

    if (wmbus->canSetLinkModes(lms))
    {
        wmbus->setLinkModes(lms);
    }
    else
    {
        warning("Warning! Desired link modes %s cannot be set for device %s\n",
                lms.hr().c_str(), wmbus->hr().c_str());
    }
    /*
    LinkModeCalculationResult lmcr = calculateLinkModes(config, wmbus.get(), link_modes_matter);
    if (lmcr.type != LinkModeCalculationResultType::Success)
    {
        error("%s\n", lmcr.msg.c_str());
        }*/

    bool simulated = false;
    if (detected->found_type == DEVICE_SIMULATION)
    {
        simulated = true;
        debug("(main) added %s to files\n", detected->found_file.c_str());
        simulation_files_.insert(detected->specified_device.file);
    }
    wmbus->onTelegram([&, simulated](AboutTelegram &about,vector<uchar> data){return meter_manager_->handleTelegram(about, data, simulated);});
    wmbus->setTimeout(config->alarm_timeout, config->alarm_expected_activity);
}

void perform_auto_scan_of_serial_devices(Configuration *config)
{
    // Enumerate all serial devices that might connect to a wmbus device.
    vector<string> ttys = serial_manager_->listSerialTTYs();

    // Did a non-wmbus-device get unplugged? Then remove it from the known-not-wmbus-device set.
    remove_lost_serial_devices_from_ignore_list(ttys);

    for (string& tty : ttys)
    {
        trace("[MAIN] serial device %s\n", tty.c_str());
        if (not_serial_wmbus_devices_.count(tty) > 0)
        {
            trace("[MAIN] skipping already probed not wmbus serial device %s\n", tty.c_str());
            continue;
        }
        if (config->do_not_probe_ttys.count("all") > 0 ||
            config->do_not_probe_ttys.count(tty) > 0)
        {
            trace("[MAIN] not probing forbidden tty %s\n", tty.c_str());
            continue;
        }
        shared_ptr<SerialDevice> sd = serial_manager_->lookup(tty);
        if (!sd)
        {
            // This serial device is not in use, but is there a device on it?
            debug("(main) device %s not currently used, detect contents...\n", tty.c_str());

            // What should the desired linkmodes be? We have no specified device since this an auto detect.
            // But we might have an auto linkmodes?
            LinkModeSet desired_linkmodes = config->auto_device_linkmodes;
            if (desired_linkmodes.empty())
            {
                // Nope, lets fall back on the default_linkmodes.
                desired_linkmodes = config->default_device_linkmodes;
            }
            Detected detected = detectWMBusDeviceOnTTY(tty, desired_linkmodes, serial_manager_);
            if (detected.found_type != DEVICE_UNKNOWN)
            {
                // See if we had a specified device without a file,
                // that matches this detected device.
                bool found = find_specified_device_and_update_detected(config, &detected);
                if (config->use_auto_device_detect || found)
                {
                    // Open the device, only if auto is enabled, or if the device was specified.
                    open_wmbus_device_and_set_linkmodes(config, found?"config":"auto", &detected);
                }
            }
            else
            {
                // This serial device was something that we could not recognize.
                // A modem, an android phone, a teletype Model 33, etc....
                // Mark this serial device as unknown, to avoid repeated detection attempts.
                not_serial_wmbus_devices_.insert(tty);
                verbose("(main) ignoring %s, it does not respond as any of the supported wmbus devices.\n", tty.c_str());
            }
        }
    }
}

void perform_auto_scan_of_swradio_devices(Configuration *config)
{
    // Enumerate all swradio devices, that can be used.
    vector<string> serialnrs = listRtlSdrDevices();

    if (serialnrs.size() > 0)
    {
        if (!rtlsdr_found_ || !rtlwmbus_found_)
        {
            rtlsdr_found_ = check_if_rtlsdr_exists_in_path();
            rtlwmbus_found_ = check_if_rtlwmbus_exists_in_path();
        }
        if (!rtlsdr_found_)
        {
            warning("Warning! Auto scan has found an rtl_sdr dongle, but you have no rtl_sdr in the path!\n");
        }
        if (!rtlwmbus_found_)
        {
            warning("Warning! Auto scan has found an rtl_sdr dongle, but you have no rtl_wmbus in the path!\n");
        }
    }

    // We are missing rtl_sdr and/or rtl_wmbus, stop here.
    if (!rtlsdr_found_ || !rtlwmbus_found_) return;

    // Did an unavailable swradio-device get unplugged? Then remove it from the known-not-swradio-device set.
    remove_lost_swradio_devices_from_ignore_list(serialnrs);

    for (string& serialnr : serialnrs)
    {
        trace("[MAIN] rtlsdr device %s\n", serialnr.c_str());
        if (not_swradio_wmbus_devices_.count(serialnr) > 0)
        {
            trace("[MAIN] skipping already probed rtlsdr %s\n", serialnr.c_str());
            continue;
        }
        shared_ptr<SerialDevice> sd = serial_manager_->lookup(serialnr);
        if (!sd)
        {
            debug("(main) rtlsdr device %s not currently used.\n", serialnr.c_str());
            Detected detected;
            detected.specified_device.type = WMBusDeviceType::DEVICE_RTLWMBUS;
            AccessCheck ac = detectRTLSDR(serialnr, &detected);
            if (ac != AccessCheck::AccessOK)
            {
                // We cannot access this swradio device.
                not_swradio_wmbus_devices_.insert(serialnr);
                verbose("(main) ignoring rtlsdr %s since it is unavailable.\n", serialnr.c_str());
            }
            else
            {
                // Use the serialnr as the id.
                detected.found_device_id = serialnr;
                bool found = find_specified_device_and_update_detected(config, &detected);
                if (config->use_auto_device_detect || found)
                {
                    // Open the device, only if auto is enabled, or if the device was specified.
                    open_wmbus_device_and_set_linkmodes(config, found?"config":"auto", &detected);
                }
            }
        }
    }
}

time_t last_info_print_ = 0;

void regular_checkup(Configuration *config)
{
    if (config->daemon)
    {
        time_t now = time(NULL);
        if (now - last_info_print_ > 3600*24)
        {
            last_info_print_= now;
            size_t peak_rss = getPeakRSS();
            size_t curr_rss = getCurrentRSS();
            string prss = humanReadableTwoDecimals(peak_rss);

            // Log memory usage once per day.
            notice("(memory) rss %zu peak %s\n", curr_rss, prss.c_str());
        }
    }

    if (serial_manager_ && config)
    {
        detect_and_configure_wmbus_devices(config, DetectionType::ALL);
    }

    {
        LOCK_WMBUS_DEVICES(regular_checkup);

        for (auto &w : wmbus_devices_)
        {
            if (w->isWorking())
            {
                w->checkStatus();
            }
        }
    }
}

void remove_lost_serial_devices_from_ignore_list(vector<string> &devices)
{
    vector<string> to_be_removed;

    // Iterate over the devices known to NOT be wmbus devices.
    for (const string& nots : not_serial_wmbus_devices_)
    {
        auto i = std::find(devices.begin(), devices.end(), nots);
        if (i == devices.end())
        {
            // The device has been removed, therefore
            // we have to forget that the device was not a wmbus device.
            // Since next time someone plugs in a device, it might be a different
            // one getting the same /dev/ttyUSBxx
            to_be_removed.push_back(nots);
        }
    }

    for (string& r : to_be_removed)
    {
        not_serial_wmbus_devices_.erase(r);
    }
}

void remove_lost_swradio_devices_from_ignore_list(vector<string> &devices)
{
    vector<string> to_be_removed;

    // Iterate over the devices known to NOT be wmbus devices.
    for (const string& nots : not_swradio_wmbus_devices_)
    {
        auto i = std::find(devices.begin(), devices.end(), nots);
        if (i == devices.end())
        {
            // The device has been removed, therefore
            // we have to forget that the device was not a wmbus device.
            // Since next time someone plugs in a device, it might be a different
            // one getting the same /dev/ttyUSBxx
            to_be_removed.push_back(nots);
        }
    }

    for (string& r : to_be_removed)
    {
        not_swradio_wmbus_devices_.erase(r);
    }
}

void setup_log_file(Configuration *config)
{
    if (config->use_logfile)
    {
        verbose("(wmbusmeters) using log file %s\n", config->logfile.c_str());
        bool ok = enableLogfile(config->logfile, config->daemon);
        if (!ok) {
            if (config->daemon) {
                warning("Could not open log file, will use syslog instead.\n");
            } else {
                error("Could not open log file.\n");
            }
        }
    }
    else
    {
        disableLogfile();
    }
}

void setup_meters(Configuration *config, MeterManager *manager)
{
    for (auto &m : config->meters)
    {
        const char *keymsg = (m.key[0] == 0) ? "not-encrypted" : "encrypted";
        auto meter = create_meter(config, toMeterType(m.type), &m, keymsg);
        manager->addMeter(meter);
    }
}

bool start(Configuration *config)
{
    // Configure where the logging information should end up.
    setup_log_file(config);

    if (config->meters.size() == 0 && config->all_device_linkmodes_specified.empty())
    {
        error("No meters supplied. You must supply which link modes to listen to. 11 Eg. auto:c1\n");
    }

    // Configure settings.
    silentLogging(config->silent);
    verboseEnabled(config->verbose);
    logTelegramsEnabled(config->logtelegrams);
    debugEnabled(config->debug);
    internalTestingEnabled(config->internaltesting);
    traceEnabled(config->trace);
    stderrEnabled(config->use_stderr_for_log);
    setAlarmShells(config->alarm_shells);
    setIgnoreDuplicateTelegrams(config->ignore_duplicate_telegrams);

    log_start_information(config);

    // Create the manager monitoring all filedescriptors and invoking callbacks.
    serial_manager_ = createSerialCommunicationManager(config->exitafter, true);
    // If our software unexpectedly exits, then stop the manager, to try
    // to achive a nice shutdown.
    onExit(call(serial_manager_.get(),stop));
    //serial_manager_->eachEventLooping([]() { check_statuses(); });

    // Create the printer object that knows how to translate
    // telegrams into json, fields that are written into log files
    // or sent to shell invocations.
    printer_ = create_printer(config);

    meter_manager_ = createMeterManager();

    // Create the Meter objects from the configuration.
    setup_meters(config, meter_manager_.get());

    // Attach a received-telegram-callback from the meter and
    // attach it to the printer.
    meter_manager_->forEachMeter(
        [&](Meter *meter)
        {
            meter->onUpdate([&](Telegram *t,Meter *meter)
                            {
                                printer_->print(t, meter, &config->jsons, &config->selected_fields);
                                oneshot_check(config, t, meter);
                            });
        }
        );

    // Detect and initialize any devices.
    // Future changes are triggered through this callback.
    printed_warning_ = true;

    detect_and_configure_wmbus_devices(config, DetectionType::STDIN_FILE_SIMULATION);

    serial_manager_->startEventLoop();
    detect_and_configure_wmbus_devices(config, DetectionType::ALL);

    if (wmbus_devices_.size() == 0)
    {
        if (config->nodeviceexit)
        {
            notice("No wmbus device detected. Exiting!\n");
            serial_manager_->stop();
        }
        else
        {
            notice("No wmbus device detected, waiting for a device to be plugged in.\n");
            check_if_multiple_wmbus_meters_running();
        }
    }

    // Every 2 seconds detect any plugged in or removed wmbus devices.
    serial_manager_->startRegularCallback("HOT_PLUG_DETECTOR",
                                  2,
                                  [&](){
                                      regular_checkup(config);
                                  });

    if (config->daemon)
    {
        notice("(wmbusmeters) waiting for telegrams\n");
    }

    if (!meter_manager_->hasMeters() && serial_manager_->isRunning())
    {
        notice("No meters configured. Printing id:s of all telegrams heard!\n");

        meter_manager_->onTelegram([](AboutTelegram &about, vector<uchar> frame) {
                Telegram t;
                t.about = about;
                MeterKeys mk;
                t.parse(frame, &mk, false); // Try a best effort parse, do not print any warnings.
                t.print();
                t.explainParse("(wmbus)",0);
                logTelegram(t.frame, 0, 0);
                return true;
            });
    }

    for (auto &w : wmbus_devices_)
    {
        // Real devices do nothing, but the simulator device will simulate.
        w->simulate();
    }

    // This thread now sleeps waiting for the serial communication manager to stop.
    // The manager has already started one thread that performs select and then callbacks
    // to decoding the telegrams, finally invoking the printer.
    // The regular callback invoked to detect changes in the wmbus devices and
    // the alarm checks, is started in a separate thread.
    serial_manager_->waitForStop();

    if (config->daemon)
    {
        notice("(wmbusmeters) shutting down\n");
    }

    // Destroy any remaining allocated objects.
    wmbus_devices_.clear();
    meter_manager_->removeAllMeters();
    printer_.reset();
    serial_manager_.reset();

    restoreSignalHandlers();
    return gotHupped();
}

void start_daemon(string pid_file, string device_override, string listento_override)
{
    setlogmask(LOG_UPTO (LOG_INFO));
    openlog("wmbusmetersd", LOG_CONS | LOG_PID | LOG_NDELAY, LOG_LOCAL1);

    enableSyslog();

    // Pre check that the pid file can be writte to.
    // Exit before fork, if it fails.
    write_pid(pid_file, 0);

    pid_t pid = fork();
    if (pid < 0)
    {
        error("Could not fork.\n");
    }
    if (pid > 0)
    {
        // Success! The parent stores the pid and exits.
        write_pid(pid_file, pid);
        return;
    }

    // Change the file mode mask
    umask(0);

    // Create a new SID for the daemon
    pid_t sid = setsid();
    if (sid < 0) {
        // log
        exit(-1);
    }

    if ((chdir("/")) < 0) {
        error("Could not change to root as current working directory.");
    }

    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);

    if (open("/dev/null", O_RDONLY) == -1) {
        error("Failed to reopen stdin while daemonising (errno=%d)",errno);
    }
    if (open("/dev/null", O_WRONLY) == -1) {
        error("Failed to reopen stdout while daemonising (errno=%d)",errno);
    }
    if (open("/dev/null", O_RDWR) == -1) {
        error("Failed to reopen stderr while daemonising (errno=%d)",errno);
    }
    start_using_config_files("", true, device_override, listento_override);
}

void start_using_config_files(string root, bool is_daemon, string device_override, string listento_override)
{
    bool restart = false;
    do
    {
        shared_ptr<Configuration> config = loadConfiguration(root, device_override, listento_override);
        config->daemon = is_daemon;
        restart = start(config.get());
        if (restart)
        {
            notice("(wmbusmeters) HUP received, restarting and reloading config files.\n");
        }
    }
    while (restart);
}

void write_pid(string pid_file, int pid)
{
    FILE *pidf = fopen(pid_file.c_str(), "w");
    if (!pidf) {
        error("Could not open pid file \"%s\" for writing!\n", pid_file.c_str());
    }
    if (pid > 0) {
        int n = fprintf(pidf, "%d\n", pid);
        if (!n) {
            error("Could not write pid (%d) to file \"%s\"!\n", pid, pid_file.c_str());
        }
        notice("(wmbusmeters) started %s\n", pid_file.c_str());
    }
    fclose(pidf);
    return;
}
