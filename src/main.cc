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
#include"serial.h"
#include"util.h"
#include"version.h"
#include"wmbus.h"

#include <algorithm>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>
#include <syslog.h>
#include <string.h>
#include <set>

using namespace std;

void oneshotCheck(Configuration *config, Telegram *t, Meter *meter);
void setupLogFile(Configuration *config);
void setup_meters(Configuration *config, MeterManager *manager);
void detectAndConfigureWMBusDevices(Configuration *config);
unique_ptr<Printer> createPrinter(Configuration *config);
void logStartInformation(Configuration *config);
bool start(Configuration *config);
void startUsingConfigFiles(string root, bool is_daemon, string device_override, string listento_override);
void startDaemon(string pid_file, string device_override, string listento_override); // Will use config files.
void list_shell_envs(Configuration *config, string meter_type);
void list_fields(Configuration *config, string meter_type);
void list_meters(Configuration *config);
unique_ptr<Meter> create_meter(Configuration *config, MeterType type, MeterInfo *mi, const char *keymsg);

// The serial communication manager takes care of
// monitoring the file descrtiptors for the ttys,
// background shells, files and stdin. It also invokes
// regular callbacks used for monitoring alarms and
// detecting new devices.
unique_ptr<SerialCommunicationManager> serial_manager_;

// Manage registered meters to decode and relay.
unique_ptr<MeterManager> meter_manager_;

// Current active set of wmbus devices that can receive telegrams.
// This can change during runtime, plugging/unplugging wmbus dongles.
vector<unique_ptr<WMBus>> wmbus_devices_;
pthread_mutex_t devices_lock_ = PTHREAD_MUTEX_INITIALIZER;

// Remember devices that were not detected as wmbus devices.
// To avoid probing them again and again.
set<string> not_serial_wmbus_devices_;

// The software radio devices are always swradio devices
// but they might not be available for wmbusmeters.
set<string> not_swradio_wmbus_devices_;

// When manually supplying stdin or a file, then, after
// it has been read, do not open it again!
set<string> do_not_open_file_again_;

// Rendering the telegrams to json,fields or shell calls is
// done by the printer.
unique_ptr<Printer> printer_;

// Set as true when the warning for no detected wmbus devices has been printed.
bool printed_warning_ = false;

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
    }
    else
    if (config->daemon)
    {
        startDaemon(config->pid_file, config->device_override, config->listento_override);
        exit(0);
    }
    else
    if (config->useconfig)
    {
        startUsingConfigFiles(config->config_root, false, config->device_override, config->listento_override);
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

unique_ptr<WMBus> createWMBusDeviceFrom(Detected *detected, Configuration *config, SerialCommunicationManager *manager)
{
    unique_ptr<WMBus> wmbus;

    unique_ptr<SerialDevice> serial_override;
    bool link_modes_matter = true;

    if (detected->override_tty)
    {
        serial_override = manager->createSerialDeviceFile(detected->device.file);
        verbose("(serial) override with devicefile: %s\n", detected->device.file.c_str());
        link_modes_matter = false;
    }

    switch (detected->type)
    {
    case DEVICE_IM871A:
        verbose("(im871a) on %s\n", detected->device.file.c_str());
        wmbus = openIM871A(detected->device.file, manager, std::move(serial_override));
        break;
    case DEVICE_AMB8465:
        verbose("(amb8465) on %s\n", detected->device.file.c_str());
        wmbus = openAMB8465(detected->device.file, manager, std::move(serial_override));
        break;
    case DEVICE_SIMULATOR:
        verbose("(simulator) in %s\n", detected->device.file.c_str());
        wmbus = openSimulator(detected->device.file, manager, std::move(serial_override));
        link_modes_matter = false;
        break;
    case DEVICE_RAWTTY:
        verbose("(rawtty) on %s\n", detected->device.file.c_str());
        wmbus = openRawTTY(detected->device.file, detected->baudrate, manager, std::move(serial_override));
        link_modes_matter = false;
        break;
    case DEVICE_RFMRX2:
        verbose("(rfmrx2) on %s\n", detected->device.file.c_str());
        if (config->reopenafter == 0)
        {
            manager->setReopenAfter(600); // Close and reopen the fd, because of some bug in the device.
        }
        wmbus = openRawTTY(detected->device.file, 38400, manager, std::move(serial_override));
        break;
    case DEVICE_RTLWMBUS:
    {
        string command;
        if (!detected->override_tty)
        {
            command = detected->device.suffix;
            string freq = "868.95M";
            string prefix = "";
            if (isFrequency(command)) {
                freq = command;
                command = "";
            }
            if (config->daemon) {
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
                command = prefix+"rtl_sdr -f "+freq+" -s 1.6e6 - 2>/dev/null | "+prefix+"rtl_wmbus";
            }
            verbose("(rtlwmbus) using command: %s\n", command.c_str());
        }
        wmbus = openRTLWMBUS(detected->device.file, command, manager,
                             [command](){
                                 warning("(rtlwmbus) child process exited! "
                                         "Command was: \"%s\"\n", command.c_str());
                             },
                             std::move(serial_override));
        break;
    }
    case DEVICE_RTL433:
    {
        string command;
        if (!detected->override_tty)
        {
            command = detected->device.suffix;
            string freq = "868.95M";
            string prefix = "";
            if (isFrequency(command)) {
                freq = command;
                command = "";
            }
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
                command = prefix+"rtl_433 -F csv -f "+freq;
            }
            verbose("(rtl433) using command: %s\n", command.c_str());
        }
        wmbus = openRTL433(detected->device.file, command, manager,
                             [command](){
                                 warning("(rtl433) child process exited! "
                                         "Command was: \"%s\"\n", command.c_str());
                             },
                             std::move(serial_override));
        break;
    }
    case DEVICE_CUL:
    {
        verbose("(cul) on %s\n", detected->device.file.c_str());
        wmbus = openCUL(detected->device.file, manager, std::move(serial_override));
        break;
    }
    case DEVICE_D1TC:
    {
        verbose("(d1tc) on %s\n", detected->device.file.c_str());
        wmbus = openD1TC(detected->device.file, manager, std::move(serial_override));
        break;
    }
    case DEVICE_WMB13U:
    {
        verbose("(wmb13u) on %s\n", detected->device.file.c_str());
        wmbus = openWMB13U(detected->device.file, manager, std::move(serial_override));
        break;
    }
    case DEVICE_UNKNOWN:
        verbose("(main) internal error! cannot create an unknown device! exiting!\n");
        if (config->daemon) {
            // If starting as a daemon, wait a bit so that systemd have time to catch up.
            sleep(1);
        }
        exit(1);
        break;
    }

    LinkModeCalculationResult lmcr = calculateLinkModes(config, wmbus.get(), link_modes_matter);
    if (lmcr.type != LinkModeCalculationResultType::Success)
    {
        error("%s\n", lmcr.msg.c_str());
    }
    return wmbus;
}

void list_shell_envs(Configuration *config, string meter_type)
{
    string ignore1, ignore2, ignore3;
    vector<string> envs;
    Telegram t;
    MeterInfo mi;
    unique_ptr<Meter> meter = create_meter(config, toMeterType(meter_type), &mi, "");
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
    unique_ptr<Meter> meter = create_meter(config, toMeterType(meter_type), &mi, "");

    int width = 0;
    for (auto &p : meter->prints())
    {
        if ((int)p.field_name.size() > width) width = p.field_name.size();
    }

    string id = padLeft("id", width);
    printf("%s  The meter id number.\n", id.c_str());
    string name = padLeft("name", width);
    printf("%s  Your name for the meter.\n", name.c_str());
    string type = padLeft("type", width);
    printf("%s  Meter type/driver.\n", type.c_str());
    string timestamp = padLeft("timestamp", width);
    printf("%s  Timestamp when wmbusmeters received the telegram.\n", timestamp.c_str());
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


void setupLogFile(Configuration *config)
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

unique_ptr<Meter> create_meter(Configuration *config, MeterType type, MeterInfo *mi, const char *keymsg)
{
    unique_ptr<Meter> newm;

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

void setup_meters(Configuration *config, MeterManager *manager)
{
    for (auto &m : config->meters)
    {
        const char *keymsg = (m.key[0] == 0) ? "not-encrypted" : "encrypted";
        auto meter = create_meter(config, toMeterType(m.type), &m, keymsg);
        manager->addMeter(std::move(meter));
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

void check_for_dead_wmbus_devices(Configuration *config)
{
    trace("[MAIN] checking for dead wmbus devices...\n");

    LOCK("(main)", "check_for_dead_wmbus_devices", devices_lock_);
    vector<WMBus*> not_working;
    for (auto &w : wmbus_devices_)
    {
        if (!w->isWorking())
        {
            not_working.push_back(w.get());
            if (!config->use_auto_detect)
            {
                notice("Lost %s closing %s\n", w->device().c_str(), toString(w->type()));
            }
        }
    }

    for (auto w : not_working)
    {
        auto i = wmbus_devices_.begin();
        while (i != wmbus_devices_.end())
        {
            if (w == (*i).get())
            {
                // The erased unique_ptr will delete the WMBus object.
                wmbus_devices_.erase(i);
                break;
            }
            i++;
        }
    }

    if (wmbus_devices_.size() == 0)
    {
        if (!printed_warning_)
        {
            info("(main) no wmbus device detected, waiting for a device to be plugged in.\n");
            printed_warning_ = true;
        }
    }
    else
    {
        printed_warning_ = false;
    }

    UNLOCK("(main)", "check_for_dead_wmbus_devices", devices_lock_);
}

void open_wmbus_device(Configuration *config, string how, string device, Detected *detected)
{
    // A newly plugged in device has been manually configured or automatically detected! Start using it!
    if (config->use_auto_detect)
    {
        notice("Detected %s %s on %s\n", how.c_str(), toString(detected->type), device.c_str());
    }
    else
    {
        verbose("(main) %s %s on %s\n", how.c_str(), toString(detected->type), device.c_str());
    }
    LOCK("(main)", "perform_auto_scan_of_devices", devices_lock_);
    unique_ptr<WMBus> w = createWMBusDeviceFrom(detected, config, serial_manager_.get());
    wmbus_devices_.push_back(std::move(w));
    WMBus *wmbus = wmbus_devices_.back().get();
    wmbus->setLinkModes(config->listen_to_link_modes);
    //string using_link_modes = wmbus->getLinkModes().hr();
    //verbose("(config) listen to link modes: %s\n", using_link_modes.c_str());
    bool simulated = detected->type == WMBusDeviceType::DEVICE_SIMULATOR;
    wmbus->onTelegram([&, simulated](vector<uchar> data){return meter_manager_->handleTelegram(data, simulated);});
    wmbus->setTimeout(config->alarm_timeout, config->alarm_expected_activity);
    UNLOCK("(main)", "perform_auto_scan_of_devices", devices_lock_);
}

void perform_auto_scan_of_serial_devices(Configuration *config)
{
    // Enumerate all serial devices that might connect to a wmbus device.
    vector<string> devices = serial_manager_->listSerialDevices();

    // Did a non-wmbus-device get unplugged? Then remove it from the known-not-wmbus-device set.
    remove_lost_serial_devices_from_ignore_list(devices);

    for (string& device : devices)
    {
        trace("[MAIN] serial device %s\n", device.c_str());
        if (not_serial_wmbus_devices_.count(device) > 0)
        {
            trace("[MAIN] skipping already probed not wmbus serial device %s\n", device.c_str());
            continue;
        }
        SerialDevice *sd = serial_manager_->lookup(device);
        if (sd == NULL)
        {
            debug("(main) device %s not currently used, detect contents...\n", device.c_str());
            // This serial device is not in use.
            Detected detected = detectImstAmberCul(device, "", "", serial_manager_.get(), true, false, false);
            if (detected.type == DEVICE_UNKNOWN)
            {
                // This serial device was something that we could not recognize.
                // A modem, an android phone, a teletype Model 33, etc....
                // Mark this serial device as unknown, to avoid repeated detection attempts.
                not_serial_wmbus_devices_.insert(device);
                info("(main) ignoring %s, it does not respond as any of the supported wmbus devices.\n", device.c_str());
            }
            else
            {
                open_wmbus_device(config, "auto scan detected", device, &detected);
            }
        }
    }
}

void perform_auto_scan_of_swradio_devices(Configuration *config)
{
    // Enumerate all swradio devices, that can be used.
    vector<string> devices = serial_manager_->listSWRadioDevices();

    // Did an unavailable swradio-device get unplugged? Then remove it from the known-not-swradio-device set.
    remove_lost_swradio_devices_from_ignore_list(devices);

    for (string& device : devices)
    {
        trace("[MAIN] swradio device %s\n", device.c_str());
        if (not_swradio_wmbus_devices_.count(device) > 0)
        {
            trace("[MAIN] skipping already probed swradio device %s\n", device.c_str());
            continue;
        }
        SerialDevice *sd = serial_manager_->lookup(device);
        if (sd == NULL)
        {
            debug("(main) swradio device %s not currently used, detect contents...\n", device.c_str());
            // This serial device is not in use.
            Detected detected;
            AccessCheck ac = detectRTLSDR(device, &detected, serial_manager_.get());
            if (ac != AccessCheck::AccessOK)
            {
                // We cannot access this swradio device.
                not_swradio_wmbus_devices_.insert(device);
                info("(main) ignoring swradio %s since it is unavailable.\n", device.c_str());
            }
            else
            {
                detected.device.file = device;
                open_wmbus_device(config, "auto scan detected", device, &detected);
            }
        }
    }
}

void perform_auto_scan_of_devices(Configuration *config)
{
    perform_auto_scan_of_serial_devices(config);
    perform_auto_scan_of_swradio_devices(config);
}

void detectAndConfigureWMBusDevices(Configuration *config)
{
    check_for_dead_wmbus_devices(config);

    for (auto &device : config->supplied_wmbus_devices)
    {
        SerialDevice *sd = serial_manager_->lookup(device.file);
        if (sd != NULL)
        {
            trace("(main) %s already configured\n", sd->device().c_str());
            continue;
        }

        if (do_not_open_file_again_.count(device.file) == 0)
        {
            Detected detected = detectWMBusDeviceSetting(device.file,
                                                         device.suffix,
                                                         device.linkmodes,
                                                         serial_manager_.get());

            if (detected.type != DEVICE_UNKNOWN)
            {
                if (detected.is_stdin || detected.is_file)
                {
                    // Only read stdin and files once!
                    do_not_open_file_again_.insert(device.file);
                }
                open_wmbus_device(config, "manual configuration", device.str(), &detected);
            }
        }
        else
        {
            trace("(MAIN) ignoring handled file %s\n", device.file.c_str());
        }
    }

    if (config->use_auto_detect)
    {
        perform_auto_scan_of_devices(config);
    }
}

unique_ptr<Printer> createPrinter(Configuration *config)
{
    return unique_ptr<Printer>(new Printer(config->json, config->fields,
                                           config->separator, config->meterfiles, config->meterfiles_dir,
                                           config->use_logfile, config->logfile,
                                           config->telegram_shells,
                                           config->meterfiles_action == MeterFileType::Overwrite,
                                           config->meterfiles_naming,
                                           config->meterfiles_timestamp));
}

void logStartInformation(Configuration *config)
{
    verbose("(wmbusmeters) version: " VERSION "\n");

    if (config->exitafter != 0) {
        verbose("(config) wmbusmeters will exit after %d seconds\n", config->exitafter);
    }

    if (config->reopenafter != 0) {
        verbose("(config) wmbusmeters close/open the wmbus dongle fd after every %d seconds\n", config->reopenafter);
    }

    if (config->meterfiles) {
        verbose("(config) store meter files in: \"%s\"\n", config->meterfiles_dir.c_str());
    }

    for (auto &device : config->supplied_wmbus_devices)
    {
        verbose("(config) using device: %s %s\n", device.file.c_str(), device.suffix.c_str());
    }
    verbose("(config) number of meters: %d\n", config->meters.size());
}

bool start(Configuration *config)
{
    // Configure where the logging information should end up.
    setupLogFile(config);

    // Configure settings.
    warningSilenced(config->silence);
    verboseEnabled(config->verbose);
    logTelegramsEnabled(config->logtelegrams);
    debugEnabled(config->debug);
    internalTestingEnabled(config->internaltesting);
    traceEnabled(config->trace);
    stderrEnabled(config->use_stderr_for_log);
    setAlarmShells(config->alarm_shells);

    logStartInformation(config);

    // Create the manager monitoring all filedescriptors and invoking callbacks.
    serial_manager_ = createSerialCommunicationManager(config->exitafter, config->reopenafter);
    // If our software unexpectedly exits, then stop the manager, to try
    // to achive a nice shutdown.
    onExit(call(serial_manager_.get(),stop));

    // Create the printer object that knows how to translate
    // telegrams into json, fields that are written into log files
    // or sent to shell invocations.
    printer_ = createPrinter(config);

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
                                oneshotCheck(config, t, meter);
                            });
        }
        );

    serial_manager_->startEventLoop();

    // Detect and initialize any devices.
    // Future changes are triggered through this callback.
    printed_warning_ = true;
    detectAndConfigureWMBusDevices(config);

    if (!config->use_auto_detect)
    {
        serial_manager_->expectDevicesToWork();
        if (wmbus_devices_.size() == 0)
        {
            notice("(main) no wmbus device configured! Exiting.\n");
            serial_manager_->stop();
        }
    }
    else
    {
        if (wmbus_devices_.size() == 0)
        {
            notice("(main) no wmbus device detected, waiting for a device to be plugged in.\n");
        }
    }

    // Every 2 seconds detect any plugged in or removed wmbus devices.
    serial_manager_->startRegularCallback("HOT_PLUG_DETECTOR",
                                  2,
                                  [&](){
                                      if (serial_manager_ && config)
                                          detectAndConfigureWMBusDevices(config);
                                  });

    if (config->daemon)
    {
        notice("(wmbusmeters) waiting for telegrams\n");
    }

    if (!meter_manager_->hasMeters())
    {
        notice("No meters configured. Printing id:s of all telegrams heard!\n\n");

        meter_manager_->onTelegram([](vector<uchar> frame) {
                Telegram t;
                MeterKeys mk;
                t.parserNoWarnings(); // Try a best effort parse, do not print any warnings.
                t.parse(frame, &mk);
                t.print();
                t.explainParse("(wmbus)",0);
                logTelegram("(wmbus)", t.frame, 0, 0);
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

void oneshotCheck(Configuration *config, Telegram *t, Meter *meter)
{
    if (!config->oneshot) return;

    if (meter_manager_->hasAllMetersReceivedATelegram())
    {
        // All meters have received at least one update! Stop!
        verbose("(main) all meters have received at least one update, stopping.\n");
        serial_manager_->stop();
    }
}

void writePid(string pid_file, int pid)
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

void startDaemon(string pid_file, string device_override, string listento_override)
{
    setlogmask(LOG_UPTO (LOG_INFO));
    openlog("wmbusmetersd", LOG_CONS | LOG_PID | LOG_NDELAY, LOG_LOCAL1);

    enableSyslog();

    // Pre check that the pid file can be writte to.
    // Exit before fork, if it fails.
    writePid(pid_file, 0);

    pid_t pid = fork();
    if (pid < 0)
    {
        error("Could not fork.\n");
    }
    if (pid > 0)
    {
        // Success! The parent stores the pid and exits.
        writePid(pid_file, pid);
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
    startUsingConfigFiles("", true, device_override, listento_override);
}

void startUsingConfigFiles(string root, bool is_daemon, string device_override, string listento_override)
{
    bool restart = false;
    do
    {
        unique_ptr<Configuration> config = loadConfiguration(root, device_override, listento_override);
        config->daemon = is_daemon;
        restart = start(config.get());
        if (restart)
        {
            notice("(wmbusmeters) HUP received, restarting and reloading config files.\n");
        }
    }
    while (restart);
}
