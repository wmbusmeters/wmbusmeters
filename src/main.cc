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

#include"bus.h"
#include"cmdline.h"
#include"config.h"
#include"drivers.h"
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
void print_driver(Configuration *config, string meter_type);
void list_shell_envs(Configuration *config, string meter_type);
void list_meters(Configuration *config, bool load, bool daemon);
void list_units();
void log_start_information(Configuration *config);
void oneshot_check(Configuration *config, Telegram *t, Meter *meter);
void regular_checkup(Configuration *config);
bool start(Configuration *config);
void start_using_config_files(string root, bool is_daemon, ConfigOverrides overrides);

void start_daemon(string pid_file, string root, ConfigOverrides overrides);

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
    tzset(); // Load the current timezone.

    setVersion(VERSION);

    enableEarlyLoggingFromCommandLine(argc, argv);
    prepareBuiltinDrivers();

    auto config = parseCommandLine(argc, argv);

    if (config->version)
    {
        printf("wmbusmeters: %s\n", getVersion());
        printf(COMMIT "\n");
        exit(0);
    }

    if (config->license)
    {
        const char * authors =
#include"authors.h"
        const char * license =
            R"LICENSE(
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

You can download the source here: https://github.com/wmbusmeters/wmbusmeters
But you can also request the source from the person/company that
provided you with this binary. Read the full license for all details.

)LICENSE";
        printf("%s%s", authors, license);
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

    if (config->print_driver)
    {
        print_driver(config.get(), config->list_meter);
        exit(0);
    }

    if (config->list_meters)
    {
        list_meters(config.get(), true, false);
        exit(0);
    }

    if (config->list_units)
    {
        list_units();
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
        start_daemon(config->pid_file, config->config_root, config->overrides);
        exit(0);
    }

    if (config->useconfig)
    {
        start_using_config_files(config->config_root, false, config->overrides);
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
    return shared_ptr<Printer>(new Printer(config->json,
                                           config->pretty_print_json,
                                           config->fields,
                                           config->separator, config->meterfiles, config->meterfiles_dir,
                                           config->use_logfile, config->logfile,
                                           config->new_meter_shells,
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
    shared_ptr<Meter> meter;
    DriverInfo di;

    mi.driver_name = meter_driver;
    if (!lookupDriverInfo(meter_driver, &di))
    {
        error("No such driver %s\n", meter_driver.c_str());
    }
    meter = di.construct(mi);

    printf("METER_DEVICE\n"
           "METER_ID\n"
           "METER_JSON\n"
           "METER_DRIVER\n"
           "METER_MEDIA\n"
           "METER_TYPE\n"
           "METER_NAME\n"
           "METER_RSSI_DBM\n"
           "METER_TIMESTAMP\n"
           "METER_TIMESTAMP_LT\n"
           "METER_TIMESTAMP_UT\n"
           "METER_TIMESTAMP_UTC\n");

    for (auto &fi : meter->fieldInfos())
    {
        if (fi.vname() == "") continue;
        string name = fi.vname();
        std::transform(name.begin(), name.end(), name.begin(), ::toupper);
        if (fi.xuantity() != Quantity::Text)
        {
            printf("METER_%s_%s\n",name.c_str(), unitToStringUpperCase(fi.displayUnit()).c_str());
        }
        else
        {
            printf("METER_%s\n",name.c_str());
        }
    }

    for (string &s : meter->extraConstantFields())
    {
        string name = s;
        std::transform(name.begin(), name.end(), name.begin(), ::toupper);
        printf("METER_%s\n", name.c_str());
    }

}

void list_fields(Configuration *config, string meter_driver)
{
    MeterInfo mi;
    shared_ptr<Meter> meter;
    DriverInfo di;

    mi.driver_name = meter_driver;
    if (!lookupDriverInfo(meter_driver, &di))
    {
        error("No such driver %s\n", meter_driver.c_str());
    }
    meter = di.construct(mi);

    int width = 13; // Width of timestamp_utc
    for (FieldInfo &fi : meter->fieldInfos())
    {
        if (fi.printProperties().hasHIDE()) continue;

        string name = fi.vname();
        if (fi.xuantity() != Quantity::Text)
        {
            Unit u = defaultUnitForQuantity(fi.xuantity());
            Unit du = fi.displayUnit();
            if (du != Unit::Unknown) u = du;
            name += "_"+unitToStringLowerCase(u);
        }

        if ((int)name.size() > width) width = name.size();
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
    printf("%s  Timestamp when wmbusmeters received the telegram. Local time for hr/fields UTC for json.\n", timestamp.c_str());
    string timestamp_ut = padLeft("timestamp_ut", width);
    printf("%s  Unix timestamp when wmbusmeters received the telegram.\n", timestamp_ut.c_str());
    string timestamp_lt = padLeft("timestamp_lt", width);
    printf("%s  Local time when wmbusmeters received the telegram.\n", timestamp_lt.c_str());
    string timestamp_utc = padLeft("timestamp_utc", width);
    printf("%s  UTC time when wmbusmeters received the telegram.\n", timestamp_utc.c_str());
    string device = padLeft("device", width);
    printf("%s  The wmbus device that received the telegram.\n", device.c_str());
    string rssi = padLeft("rssi_dbm", width);
    printf("%s  The rssi for the received telegram as reported by the device.\n", rssi.c_str());
    for (auto &fi : meter->fieldInfos())
    {
        if (fi.printProperties().hasHIDE()) continue;

        if (fi.vname() == "") continue;
        string name = fi.vname();
        if (fi.xuantity() != Quantity::Text)
        {
            Unit u = defaultUnitForQuantity(fi.xuantity());
            Unit du = fi.displayUnit();
            if (du != Unit::Unknown) u = du;
            name += "_"+unitToStringLowerCase(u);
        }

        string fn = padLeft(name, width);
        printf("%s  %s\n", fn.c_str(), fi.help().c_str());
    }
}

void print_driver(Configuration *config, string meter_driver)
{
    loadAllBuiltinDrivers();

    MeterInfo mi;
    shared_ptr<Meter> meter;
    DriverInfo di;

    mi.driver_name = meter_driver;
    if (!lookupDriverInfo(meter_driver, &di))
    {
        error("info='No such driver %s'\n", meter_driver.c_str());
    }
    meter = di.construct(mi);

    const string f = di.getDynamicFileName();
    if (f != "")
    {
        printf("%s\n", di.getDynamicSource().c_str());
    }
    else
    {
        printf("info='Driver is binary only.'\n");
    }
}

void list_meters(Configuration *config, bool load, bool daemon)
{
    if (load) loadAllBuiltinDrivers();

    for (DriverInfo *di : allDrivers())
    {
        string mname = di->name().str();
        const char *infotxt = toString(di->type());
        const char *where = "";
        const string f = di->getDynamicFileName();
        if (f != "")
        {
            where = f.c_str();
        }
        else
        {
            where = "c++";
        }

        if (config->list_meters_search == "" ||                      \
            stringFoundCaseIgnored(infotxt, config->list_meters_search) || \
            stringFoundCaseIgnored(mname.c_str(), config->list_meters_search))
        {
            if (load)
            {
                printf("%-14s %s %s\n", mname.c_str(), infotxt, where);
            }
            else
            {
                if (daemon)
                {
                    verbose("(driver) %-14s %s %s\n", mname.c_str(), infotxt, where);
                }
                else
                {
                    debug("(driver) %-14s %s %s\n", mname.c_str(), infotxt, where);
                }
            }
        }
    }
}

struct TmpUnit
{
    string suff, expl, name, quantity, si;
};

void list_units()
{
    vector<TmpUnit> units;
    set<string> quantities;

// X(KVARH,kvarh,"kVARh",Reactive_Energy,"kilo volt amperes reactive hour")
    int width = 1;
    int total = 50;
    int tmp = 0;
#define X(upn,lcn,name,quantity,expl)   \
    tmp = strlen(#lcn); if (tmp > width) { width = tmp; } \
    { \
        SIUnit si(Unit::upn);                                           \
        string sis = si.str();                                          \
        units.push_back( { #lcn, expl, name, #quantity, sis } );        \
    }                                                                   \
    quantities.insert(#quantity);
LIST_OF_UNITS
#undef X

    sort(units.begin(), units.end(), [](const TmpUnit & a, const TmpUnit & b) -> bool { return a.suff > b.suff; });
    bool first = true;
    for (const string &q : quantities)
    {
        if (!first)
        {
            printf("\n");
        }
        first = false;
        printf("%s ", q.c_str());
        size_t l = total-strlen(q.c_str());
        for (size_t i=0; i<l; ++i) printf("─");
        printf("\n\n");
        for (TmpUnit &u : units)
        {
            if (u.quantity == q)
            {
                string s = tostrprintf("%-*s %s (%s)", width, u.suff.c_str(), u.expl.c_str(), u.name.c_str());
                int left = strlen_utf8(s.c_str());
                string si = tostrprintf("%s", u.si.c_str());
                int right = strlen_utf8(si.c_str());
                int space = 50-left-right;
                printf("%s", s.c_str());
                while (space > 0 && space < 100) { printf(" "); space--; }
                printf("%s\n", si.c_str());
            }
        }
    }
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
    if (isDebugEnabled())
    {
        for (MeterInfo &m : config->meters)
        {
            string aes = AddressExpression::concat(m.address_expressions);
            debug("(config) template %s %s %s\n", m.name.c_str(), aes.c_str(), m.str().c_str());
        }
    }
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

    meter_manager_->pollMeters(bus_manager_);

    if (serial_manager_ && config)
    {
        bus_manager_->detectAndConfigureWmbusDevices(config, DetectionType::ALL_BUT_SFS);
    }

    bus_manager_->regularCheckup();
    bus_manager_->sendQueue();
}

void setup_log_file(Configuration *config)
{
    if (config->use_logfile)
    {
        verbose("(wmbusmeters) using log file %s\n", config->logfile.c_str());
        if (config->logfile == "syslog")
        {
            enableSyslog();
            return;
        }
        bool ok = enableLogfile(config->logfile, config->daemon);
        if (!ok) {
            if (config->daemon) {
                warning("Could not open log file %s will use syslog instead.\n", config->logfile.c_str());
            } else {
                error("Could not open log file %s\n", config->logfile.c_str());
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
    for (MeterInfo &m : config->meters)
    {
        m.extra_calculated_fields.insert(m.extra_calculated_fields.end(),
                                         config->extra_calculated_fields.begin(),
                                         config->extra_calculated_fields.end());

        if (m.usesPolling() || driverNeedsPolling(m.driver_name))
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
    debugEnabled(config->debug);
    traceEnabled(config->trace);
    logTelegramsEnabled(config->logtelegrams);

    if (config->addtimestamps == AddLogTimestamps::NotSet)
    {
        // Default to important log timestamps when starting a daemon.
        if (config->daemon) config->addtimestamps = AddLogTimestamps::Important;
    }
    setLogTimestamps(config->addtimestamps);
    internalTestingEnabled(config->internaltesting);

    stderrEnabled(config->use_stderr_for_log);
    setAlarmShells(config->alarm_shells);
    setIgnoreDuplicateTelegrams(config->ignore_duplicate_telegrams);
    setDetailedFirst(config->detailed_first);
    if (config->new_meter_shells.size() > 0)
    {
        // We have metershells, force detailed first telegram.
        setDetailedFirst(true);
    }
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
    meter_manager_->analyzeEnabled(config->analyze,
                                   config->analyze_format,
                                   config->analyze_driver,
                                   config->analyze_key,
                                   config->analyze_verbose,
                                   config->analyze_profile);

    // The bus manager detects new/lost wmbus devices and
    // configures the devices according to the specification.
    bus_manager_   = createBusManager(serial_manager_, meter_manager_);

    // When a meter is updated, print it, shell it, log it, etc.
    // The first update will trigger the add callback (metershell)
    meter_manager_->whenMeterUpdated(
        [&](Telegram *t,Meter *meter)
        {
            printer_->print(t, meter, &config->extra_constant_fields, &config->selected_fields);
            oneshot_check(config, t, meter);
        }
    );

    setup_meters(config, meter_manager_.get());

    list_meters(config, false, config->daemon);

    bus_manager_->detectAndConfigureWmbusDevices(config, DetectionType::STDIN_FILE_SIMULATION);

    serial_manager_->startEventLoop();

    bus_manager_->detectAndConfigureWmbusDevices(config, DetectionType::ALL_BUT_SFS);

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

    bool stop_after_send = false;
    if ((config->logsummary || !meter_manager_->hasMeters()) && serial_manager_->isRunning())
    {
        if  (config->send_bus_content.size() != 0)
        {
            stop_after_send = true;
        }
        else if (!config->analyze)
        {
            if (!config->logsummary) notice("No meters configured. Printing id:s of all telegrams heard!\n");

            meter_manager_->onTelegram([](AboutTelegram &about, vector<uchar> frame) {
                    Telegram t;
                    t.about = about;
                    MeterKeys mk;
                    t.parse(frame, &mk, false); // Try a best effort parse, do not print any warnings.
                    t.print();
                    string info = string("(")+toString(about.type)+")";
                    t.explainParse(info.c_str(), 0);
                    logTelegram(t.original, t.frame, 0, 0);
                    return true;
                });
        }
    }

    bus_manager_->runAnySimulations();

    // Queue any command line send bus contents.
    for (SendBusContent &sbc : config->send_bus_content)
    {
        bus_manager_->queueSendBusContent(sbc);
    }

    // Flush the initial queue provided on the command line.
    bus_manager_->sendQueue();
    if (stop_after_send)
    {
        serial_manager_->stop();
    }
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

void start_daemon(string pid_file, string root, ConfigOverrides overrides)
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
    start_using_config_files(root, true, overrides);
}

void start_using_config_files(string root, bool is_daemon, ConfigOverrides overrides)
{
    bool restart = false;
    do
    {
        shared_ptr<Configuration> config = loadConfiguration(root, overrides);
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
