/*
 Copyright (C) 2017-2021 Fredrik Öhrström

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

#include"bus.h"
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
shared_ptr<Printer> create_printer(Configuration *config);
SpecifiedDevice *find_specified_device_from_detected(Configuration *c, Detected *d);
void list_fields(Configuration *config, string meter_type);
void list_shell_envs(Configuration *config, string meter_type);
void list_meters(Configuration *config);
void log_start_information(Configuration *config);
void oneshot_check(Configuration *config, Telegram *t, Meter *meter);
void regular_checkup(Configuration *config);
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

// Manage registered meters.
shared_ptr<MeterManager> meter_manager_;

// Manage bus devices that receive telegrams or send commands to meters.
shared_ptr<BusManager> bus_manager_;

// The printer renders the telegrams to: json, fields or shell calls.
shared_ptr<Printer> printer_;

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
Copyright (C) 2017-2021 Fredrik Öhrström

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

void list_shell_envs(Configuration *config, string meter_driver)
{
    string ignore1, ignore2, ignore3;
    vector<string> envs;
    Telegram t;
    t.about.device = "?";
    MeterInfo mi;
    mi.driver = toMeterDriver(meter_driver);
    shared_ptr<Meter> meter = createMeter(&mi);
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

void list_fields(Configuration *config, string meter_driver)
{
    MeterInfo mi;
    mi.driver = toMeterDriver(meter_driver);
    shared_ptr<Meter> meter = createMeter(&mi);

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

    for (SpecifiedDevice &specified_device : config->supplied_bus_devices)
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
            notice_timestamp("(memory) rss %zu peak %s\n", curr_rss, prss.c_str());
        }
    }

    meter_manager_->pollMeters();

    if (serial_manager_ && config)
    {
        bus_manager_->detectAndConfigureWmbusDevices(config, DetectionType::ALL);
    }

    bus_manager_->regularCheckup();
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
        m.conversions = config->conversions;

        if (needsPolling(m.driver))
        {
            // A polling meter must be defined from the start.
            auto meter = createMeter(&m);
            manager->addMeter(meter);
        }
        else
        {
            // Non polling meters are added lazily, when
            // the first telegram arrives. Just add a template
            // here.
            manager->addMeterTemplate(m);
        }
    }
}

bool start(Configuration *config)
{
    // Configure where the logging information should end up.
    setup_log_file(config);

    if (config->meters.size() == 0)
    {
        if (config->num_wmbus_devices > 0 && config->all_device_linkmodes_specified.empty())
        {
            error("Wmbus devices found but no meters supplied. You must supply which link modes to listen to. Eg. auto:c1\n");
        }
    }

    // Configure settings.
    silentLogging(config->silent);
    verboseEnabled(config->verbose);
    logTelegramsEnabled(config->logtelegrams);
    debugEnabled(config->debug);
    setLogTimestamps(config->addtimestamps);
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

    // Create the printer object that knows how to translate
    // telegrams into json, fields that are written into log files
    // or sent to shell invocations.
    printer_ = create_printer(config);

    // The meter manager knows about specified device templates
    // and creates meters on demand when the telegram arrives
    // or on startup for 2-way communication meters like mbus or T2.
    meter_manager_ = createMeterManager(config->daemon);

    // The bus manager detects new/lost wmbus devices and
    // configures the devices according to the specification.
    bus_manager_   = createBusManager(serial_manager_, meter_manager_);

    // When a meter is updated, print it, shell it, log it, etc.
    meter_manager_->whenMeterUpdated(
        [&](Telegram *t,Meter *meter)
        {
            printer_->print(t, meter, &config->jsons, &config->selected_fields);
            oneshot_check(config, t, meter);
        }
    );

    setup_meters(config, meter_manager_.get());

    bus_manager_->detectAndConfigureWmbusDevices(config, DetectionType::STDIN_FILE_SIMULATION);

    serial_manager_->startEventLoop();

    bus_manager_->detectAndConfigureWmbusDevices(config, DetectionType::ALL);

    if (bus_manager_->numBusDevices() == 0)
    {
        if (config->nodeviceexit)
        {
            notice("No wmbus device detected. Exiting!\n");
            serial_manager_->stop();
        }
        else
        {
            notice("No wmbus device detected, waiting for a device to be plugged in.\n");
            checkIfMultipleWmbusMetersRunning();
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
                if (t.about.type == FrameType::WMBUS)
                {
                    t.parse(frame, &mk, false); // Try a best effort parse, do not print any warnings.
                    t.print();
                    t.explainParse("(wmbus)",0);
                    logTelegram(t.original, t.frame, 0, 0);
                }
                else
                {
                    t.parseMBusHeader(frame); // Try a best effort parse, do not print any warnings.
                    t.print();
                    t.explainParse("(mbus)",0);
                    logTelegram(t.original, t.frame, 0, 0);
                }
                return true;
            });
    }

    /*
    for (auto &w : bus_manager_->bus_devices_)
    {
        if (w->type() == WMBusDeviceType::DEVICE_MBUS)
        {
            fprintf(stderr, "SENDING Query...\n");
            vector<uchar> buf(5);
            buf[0] = 0x10; // Start
            buf[1] = 0x40; // SND_NKE
            buf[2] = 0x00; // address 0
            uchar cs = 0;
            for (int i=1; i<3; ++i) cs += buf[i];
            buf[3] = cs; // checksum
            buf[4] = 0x16; // Stop

            w->serial()->send(buf);

            sleep(2);

            buf[0] = 0x10; // Start
            buf[1] = 0x5b; // REQ_UD2
            buf[2] = 0x00; // address 0
            cs = 0;
            for (int i=1; i<3; ++i) cs += buf[i];
            buf[3] = cs; // checksum
            buf[4] = 0x16; // Stop

            w->serial()->send(buf);

        }
    }
    */
    bus_manager_->runAnySimulations();

    // This main thread now sleeps and waits for the serial communication manager to stop.
    // The manager has already started one thread that performs select and then callbacks
    // to decoding the telegrams, finally invoking the printer.
    // The regular callback invoked to detect changes in the wmbus devices and perform the alarm checks,
    // is started in a separate thread.
    //
    // Totalling 3 threads: main (sleeping here), serial manager (telegram handling), regular checks (check lost devices and alarms)
    serial_manager_->waitForStop();

    if (config->daemon)
    {
        notice("(wmbusmeters) shutting down\n");
    }

    bus_manager_->removeAllBusDevices();
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
