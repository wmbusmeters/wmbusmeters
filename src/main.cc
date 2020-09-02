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
#include <unistd.h>
#include <syslog.h>
#include <string.h>

using namespace std;

void oneshotCheck(Configuration *config, SerialCommunicationManager *manager, Telegram *t, Meter *meter, vector<unique_ptr<Meter>> *meters);
void setupLogFile(Configuration *config);
void setupMeters(Configuration *config, vector<unique_ptr<Meter>> *meters);
void attachMetersToPrinter(Configuration *config, vector<unique_ptr<Meter>> *meters, Printer *printer);
void detectAndConfigureWMBusDevices(Configuration *config, SerialCommunicationManager *manager, vector<unique_ptr<WMBus>> *devices);
unique_ptr<Printer> createPrinter(Configuration *config);
void logStartInformation(Configuration *config);
bool start(Configuration *config);
void startUsingConfigFiles(string root, bool is_daemon, string device_override, string listento_override);
void startDaemon(string pid_file, string device_override, string listento_override); // Will use config files.

// The serial communication manager takes care of
// monitoring the file descrtiptors for the ttys,
// background shells. It also invokes regular callbacks
// used for monitoring alarms and detecting new devices.
unique_ptr<SerialCommunicationManager> manager_;

// Registered meters to decode and relay.
// This does not change during runtime.
vector<unique_ptr<Meter>> meters_;

// Current active set of wmbus devices that can receive telegrams.
// This can change during runtime, plugging/unplugging wmbus dongles.
vector<unique_ptr<WMBus>> devices_;
pthread_mutex_t devices_lock_ = PTHREAD_MUTEX_INITIALIZER;

// Rendering the telegrams to json,fields or shell calls is
// done by the printer.
unique_ptr<Printer> printer_;

// Set as true when the warning for no detected wmbus devices has been printed.
bool printed_warning_ = false;

int main(int argc, char **argv)
{
    auto config = parseCommandLine(argc, argv);

    if (config->version) {
        printf("wmbusmeters: " VERSION "\n");
        printf(COMMIT "\n");
        exit(0);
    }
    if (config->license) {
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
    if (config->need_help) {
        printf("wmbusmeters version: " VERSION "\n");
        const char *short_manual =
#include"short_manual.h"
        puts(short_manual);
    }
    else
    if (config->daemon) {
        startDaemon(config->pid_file, config->device_override, config->listento_override);
        exit(0);
    }
    else
    if (config->useconfig) {
        startUsingConfigFiles(config->config_root, false, config->device_override, config->listento_override);
        exit(0);
    }
    else {
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
        wmbus = openRTLWMBUS(command, manager,
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
        wmbus = openRTL433(command, manager,
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
        warning("No wmbus device found! Exiting!\n");
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

void setupMeters(Configuration *config, vector<unique_ptr<Meter>> *meters)
{
    for (auto &m : config->meters)
    {
        const char *keymsg = (m.key[0] == 0) ? "not-encrypted" : "encrypted";
        switch (toMeterType(m.type))
        {
#define X(mname,link,info,type,cname) \
                case MeterType::type: \
                meters->push_back(create##cname(m)); \
                verbose("(wmbusmeters) configured \"%s\" \"" #mname "\" \"%s\" %s\n", \
                m.name.c_str(), m.id.c_str(), keymsg); \
                meters->back()->addConversions(config->conversions); \
                break;
LIST_OF_METERS
#undef X
        case MeterType::UNKNOWN:
            error("No such meter type \"%s\"\n", m.type.c_str());
            break;
        }

        if (config->list_shell_envs)
        {
            string ignore1, ignore2, ignore3;
            vector<string> envs;
            Telegram t;
            meters->back()->printMeter(&t,
                                      &ignore1,
                                      &ignore2, config->separator,
                                      &ignore3,
                                      &envs,
                                      &config->jsons,
                                      &config->selected_fields);
            printf("Environment variables provided to shell for meter %s:\n", m.type.c_str());
            for (auto &e : envs) {
                int p = e.find('=');
                    string key = e.substr(0,p);
                    printf("%s\n", key.c_str());
            }
            exit(0);
        }

        if (config->list_fields)
        {
            printf("Fields produced by meter %s:\n", m.type.c_str());
            printf("id\n");
            printf("name\n");
            printf("timestamp\n");
            for (auto &f : meters->back()->fields())
            {
                printf("%s\n", f.c_str());
            }
            exit(0);
        }
    }
}

void attachMetersToPrinter(Configuration *config, vector<unique_ptr<Meter>> *meters, Printer *printer)
{
    for (auto &meter : *meters)
    {
        meter->onUpdate([&](Telegram*t,Meter* meter)
                       {
                           printer->print(t,meter,&config->jsons,&config->selected_fields);
                       });
        meter->onUpdate([&](Telegram*t, Meter* meter)
                       {
                           oneshotCheck(config, manager_.get(), t, meter, meters);
                       });
    }
}

void detectAndConfigureWMBusDevices(Configuration *config, SerialCommunicationManager *manager, vector<unique_ptr<WMBus>> *devices)
{
    trace("(trace main) checking for dead wmbus devices...\n");

    LOCK("(main)", "detectAndConfigureWMBusDevices", devices_lock_);
    vector<WMBus*> not_working;
    for (auto &w : devices_)
    {
        if (!w->isWorking())
        {
            not_working.push_back(w.get());
        }
    }

    for (auto w : not_working)
    {
        auto i = devices_.begin();
        while (i != devices_.end())
        {
            if (w == (*i).get())
            {
                // The erased unique_ptr will delete the WMBus object.
                devices_.erase(i);
                break;
            }
            i++;
        }
    }

    if (devices_.size() == 0)
    {
        if (!printed_warning_)
        {
            info("(main) no wmbus devices detected.\n");
            printed_warning_ = true;
        }
    }
    else
    {
        printed_warning_ = false;
    }

    UNLOCK("(main)", "detectAndConfigureWMBusDevices", devices_lock_);

    trace("(trace main) checking for new wmbus devices...\n");

    vector<string> ds = manager->listSerialDevices();
    for (string& device : ds)
    {
        trace("(trace main) serial device %s\n", device.c_str());
        SerialDevice *sd = manager->lookup(device);
        if (sd == NULL)
        {
            debug("(main) device %s not currently used, detect contents...\n", device.c_str());
            // This serial device is not in use.
            Detected detected = detectImstAmberCul(device, "", manager);
            if (detected.type != DEVICE_UNKNOWN)
            {
                info("(main) detected %s on %s\n", toString(detected.type), device.c_str());
                LOCK("(main)", "detectAndConfigureWMBusDevices", devices_lock_);
                unique_ptr<WMBus> w = createWMBusDeviceFrom(&detected, config, manager);
                devices->push_back(std::move(w));
                WMBus *wmbus = devices->back().get();
                UNLOCK("(main)", "detectAndConfigureWMBusDevices", devices_lock_);
                wmbus->setLinkModes(config->listen_to_link_modes);
            }
        }
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

    for (auto &device : config->wmbus_devices)
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
    stderrEnabled(config->use_stderr);
    setAlarmShells(config->alarm_shells);

    logStartInformation(config);

    // Create the manager monitoring all filedescriptors and invoking callbacks.
    manager_ = createSerialCommunicationManager(config->exitafter, config->reopenafter);
    // If our software unexpectedly exits, then stop the manager, to try
    // to achive a nice shutdown.
    onExit(call(manager_.get(),stop));

    // Create the printer object that knows how to translate
    // telegrams into json, fields that are written into log files
    // or sent to shell invocations.
    printer_ = createPrinter(config);

    // Create the Meter objects from the configuration.
    setupMeters(config, &meters_);
    // Attach a received-telegram-callback from the meter and
    // attach it to the printer.
    attachMetersToPrinter(config, &meters_, printer_.get());

    manager_->startEventLoop();

    // Detect and initialize any devices.
    // Future changes are triggered through this callback.
    printed_warning_ = true;
    detectAndConfigureWMBusDevices(config, manager_.get(), &devices_);

    if (devices_.size() == 0)
    {
        info("(main) no wmbus devices detected.\n");
    }

    // Every 2 seconds, perform the exact same call again and again.
    manager_->startRegularCallback("device_detector",
                                  2,
                                  [&](){
                                      detectAndConfigureWMBusDevices(config, manager_.get(), &devices_);
                                  });

    //wmbus->setMeters(&meters);
    //wmbus->setTimeout(config->alarm_timeout, config->alarm_expected_activity);
    //wmbus->setLinkModes(config->listen_to_link_modes);
    //string using_link_modes = wmbus->getLinkModes().hr();

    //verbose("(config) listen to link modes: %s\n", using_link_modes.c_str());

    //if (detected.type == DEVICE_SIMULATOR) {
        //wmbus->simulate();
    //}
    if (config->daemon)
    {
        notice("(wmbusmeters) waiting for telegrams\n");
    }

    // This thread now sleeps waiting for the serial communication manager to stop.
    // The manager has already started one thread that performs select and then callbacks
    // to decoding the telegrams, finally invoking the printer.
    // The regular callback invoked to detect changes in the wmbus devices and
    // the alarm checks, is started in a separate thread.
    manager_->waitForStop();

    if (config->daemon)
    {
        notice("(wmbusmeters) shutting down\n");
    }

    // Destroy any remaining allocated objects.
    devices_.clear();
    meters_.clear();
    printer_.reset();
    manager_.reset();

    restoreSignalHandlers();
    return gotHupped();
}

void oneshotCheck(Configuration *config, SerialCommunicationManager *manager, Telegram *t, Meter *meter, vector<unique_ptr<Meter>> *meters)
{
    if (!config->oneshot) return;

    for (auto &m : *meters)
    {
        if (m->numUpdates() == 0) return;
    }
    // All meters have received at least one update! Stop!
    verbose("(main) all meters have received at least one update, stopping.\n");
    manager->stop();
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
