/*
 Copyright (C) 2017-2019 Fredrik Öhrström

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

#include<string.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/errno.h>
#include <unistd.h>
#include <syslog.h>
#include <string.h>

using namespace std;

void oneshotCheck(Configuration *cmdline, SerialCommunicationManager *manager, Telegram *t, Meter *meter, vector<unique_ptr<Meter>> &meters);
bool startUsingCommandline(Configuration *cmdline);
void startUsingConfigFiles(string root, bool is_daemon);
void startDaemon(string pid_file); // Will use config files.

int main(int argc, char **argv)
{
    auto cmdline = parseCommandLine(argc, argv);

    if (cmdline->version) {
        printf("wmbusmeters: " VERSION "\n");
        printf(COMMIT "\n");
        exit(0);
    }
    if (cmdline->license) {
        const char * license = R"LICENSE(
Copyright (C) 2017-2019 Fredrik Öhrström

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
    if (cmdline->need_help) {
        printf("wmbusmeters version: " VERSION "\n");
        const char *short_manual =
#include"short_manual.h"
        puts(short_manual);
    }
    else
    if (cmdline->daemon) {
        startDaemon(cmdline->pid_file);
        exit(0);
    }
    else
    if (cmdline->useconfig) {
        startUsingConfigFiles(cmdline->config_root, false);
        exit(0);
    }
    else {
        // We want the data visible in the log file asap!
        setbuf(stdout, NULL);
        startUsingCommandline(cmdline.get());
    }
}

bool startUsingCommandline(Configuration *config)
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

    warningSilenced(config->silence);
    verboseEnabled(config->verbose);
    logTelegramsEnabled(config->logtelegrams);
    debugEnabled(config->debug);

    debug("(wmbusmeters) version: " VERSION "\n");

    if (config->exitafter != 0) {
        verbose("(config) wmbusmeters will exit after %d seconds\n", config->exitafter);
    }

    if (config->reopenafter != 0) {
        verbose("(config) wmbusmeters close/open the wmbus dongle fd after every %d seconds\n", config->reopenafter);
    }

    if (config->meterfiles) {
        verbose("(config) store meter files in: \"%s\"\n", config->meterfiles_dir.c_str());
    }
    verbose("(config) using device: %s\n", config->device.c_str());
    if (config->device_extra.length() > 0) {
        verbose("(config) with: %s\n", config->device_extra.c_str());
    }
    verbose("(config) number of meters: %d\n", config->meters.size());

    auto manager = createSerialCommunicationManager(config->exitafter, config->reopenafter);
    onExit(call(manager.get(),stop));

    Detected settings = detectWMBusDeviceSetting(config->device, config->device_extra, manager.get());

    unique_ptr<SerialDevice> serial_override;

    if (settings.override_tty)
    {
        serial_override = manager->createSerialDeviceFile(settings.devicefile);
        verbose("(serial) override with devicefile: %s\n", settings.devicefile.c_str());
    }

    unique_ptr<WMBus> wmbus;

    switch (settings.type)
    {
    case DEVICE_IM871A:
        verbose("(im871a) on %s\n", settings.devicefile.c_str());
        wmbus = openIM871A(settings.devicefile, manager.get(), std::move(serial_override));
        break;
    case DEVICE_AMB8465:
        verbose("(amb8465) on %s\n", settings.devicefile.c_str());
        wmbus = openAMB8465(settings.devicefile, manager.get(), std::move(serial_override));
        break;
    case DEVICE_SIMULATOR:
        verbose("(simulator) in %s\n", settings.devicefile.c_str());
        wmbus = openSimulator(settings.devicefile, manager.get(), std::move(serial_override));
        break;
    case DEVICE_RAWTTY:
        verbose("(rawtty) on %s\n", settings.devicefile.c_str());
        wmbus = openRawTTY(settings.devicefile, settings.baudrate, manager.get(), std::move(serial_override));
        break;
    case DEVICE_RFMRX2:
        verbose("(rfmrx2) on %s\n", settings.devicefile.c_str());
        if (config->reopenafter == 0)
        {
            manager->setReopenAfter(600); // Close and reopen the fd, because of some bug in the device.
        }
        wmbus = openRawTTY(settings.devicefile, 38400, manager.get(), std::move(serial_override));
        break;
    case DEVICE_RTLWMBUS:
    {
        string command;
        if (!settings.override_tty)
        {
            command = config->device_extra;
            string freq = "868.95M";
            string prefix = "";
            if (isFrequency(command)) {
                freq = command;
                command = "";
            }
            if (config->daemon) {
                prefix = "/usr/bin/";
            }
            if (command == "") {
                command = prefix+"rtl_sdr -f "+freq+" -s 1.6e6 - | "+prefix+"rtl_wmbus";
            }
            verbose("(rtlwmbus) using command: %s\n", command.c_str());
        }
        wmbus = openRTLWMBUS(command, manager.get(),
                             [command](){
                                 warning("(rtlwmbus) child process exited! "
                                         "Command was: \"%s\"\n", command.c_str());
                             },
                             std::move(serial_override));
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

    LinkModeCalculationResult lmcr = calculateLinkModes(config, wmbus.get());
    if (lmcr.type != LinkModeCalculationResultType::Success) {
        error("%s\n", lmcr.msg.c_str());
    }

    auto output = unique_ptr<Printer>(new Printer(config->json, config->fields,
                                                  config->separator, config->meterfiles, config->meterfiles_dir,
                                                  config->use_logfile, config->logfile,
                                                  config->shells,
                                                  config->meterfiles_action == MeterFileType::Overwrite,
                                                  config->meterfiles_naming));
    vector<unique_ptr<Meter>> meters;

    if (config->meters.size() > 0)
    {
        for (auto &m : config->meters)
        {
            const char *keymsg = (m.key[0] == 0) ? "not-encrypted" : "encrypted";
            switch (toMeterType(m.type))
            {
#define X(mname,link,info,type,cname) \
                case MeterType::type: \
                meters.push_back(create##cname(wmbus.get(), m)); \
                verbose("(wmbusmeters) configured \"%s\" \"" #mname "\" \"%s\" %s\n", \
                m.name.c_str(), m.id.c_str(), keymsg); \
                meters.back()->addConversions(config->conversions); \
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
                meters.back()->printMeter(&t,
                                          &ignore1,
                                          &ignore2, config->separator,
                                          &ignore3,
                                          &envs,
                                          &config->jsons);
                printf("Environment variables provided to shell for meter %s:\n", m.type.c_str());
                for (auto &e : envs) {
                    int p = e.find('=');
                    string key = e.substr(0,p);
                    printf("%s\n", key.c_str());
                }
                exit(0);
            }
            meters.back()->onUpdate([&](Telegram*t,Meter* meter) { output->print(t,meter,&config->jsons); });
            meters.back()->onUpdate([&](Telegram*t, Meter* meter) { oneshotCheck(config, manager.get(), t, meter, meters); });
        }
    }
    else
    {
        notice("No meters configured. Printing id:s of all telegrams heard!\n\n");

        wmbus->onTelegram([](Telegram *t){t->print();});
    }

    manager->startEventLoop();
    wmbus->setLinkModes(config->listen_to_link_modes);
    string using_link_modes = wmbus->getLinkModes().hr();

    verbose("(config) listen to link modes: %s\n", using_link_modes.c_str());

    if (settings.type == DEVICE_SIMULATOR) {
        wmbus->simulate();
    }

    if (config->daemon) {
        notice("(wmbusmeters) waiting for telegrams\n");
    }

    manager->waitForStop();

    if (config->daemon) {
        notice("(wmbusmeters) shutting down\n");
    }

    restoreSignalHandlers();
    return gotHupped();
}

void oneshotCheck(Configuration *config, SerialCommunicationManager *manager, Telegram *t, Meter *meter, vector<unique_ptr<Meter>> &meters)
{
    if (!config->oneshot) return;

    for (auto &m : meters) {
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

void startDaemon(string pid_file)
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
    startUsingConfigFiles("", true);
}

void startUsingConfigFiles(string root, bool is_daemon)
{
    bool restart = false;
    do
    {
        unique_ptr<Configuration> config = loadConfiguration(root);
        config->daemon = is_daemon;
        restart = startUsingCommandline(config.get());
        if (restart)
        {
            notice("(wmbusmeters) HUP received, restarting and reloading config files.\n");
        }
    }
    while (restart);
}
