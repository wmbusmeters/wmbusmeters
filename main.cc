/*
 Copyright (C) 2017-2018 Fredrik Öhrström

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
#include"meters.h"
#include"printer.h"
#include"serial.h"
#include"util.h"
#include"wmbus.h"

#include<string.h>

using namespace std;

void oneshotCheck(CommandLine *cmdline, SerialCommunicationManager *manager, Meter *meter);

int main(int argc, char **argv)
{
    CommandLine *cmdline = parseCommandLine(argc, argv);

    if (cmdline->need_help) {
        printf("wmbusmeters version: " WMBUSMETERS_VERSION "\n");
        printf("Usage: wmbusmeters [options] (auto | /dev/ttyUSBx) { [meter_name] [meter_type] [meter_id] [meter_key] }* \n\n");
        printf("Add more meter quadruplets to listen to more meters.\n");
        printf("Add --verbose for more detailed information on communication.\n");
        printf("    --robot or --robot=json for json output.\n");
        printf("    --robot=fields for semicolon separated fields.\n");
        printf("    --separator=X change field separator to X.\n");
        printf("    --meterfiles=dir to create status files below dir,\n"
               "        named dir/meter_name, containing the latest reading.\n");
        printf("    --meterfiles defaults dir to /tmp.\n");
        printf("    --shell=cmd invokes cmd with env variables containing the latest reading.\n");
        printf("    --shellenvs list the env variables available for the meter.\n");
        printf("    --oneshot wait for an update from each meter, then quit.\n\n");
        printf("    --exitafter=20h program exits after running for twenty hoursh\n"
               "        or 10m for ten minutes or 5s for five seconds.\n\n");
        printf("Specifying auto as the device will automatically look for usb\n");
        printf("wmbus dongles on /dev/im871a and /dev/amb8465\n\n");
        printf("The meter types: multical21,flowiq3100,supercom587,iperl (water meters) are supported.\n"
               "The meter types: multical302 (heat) and omnipower (electricity)\n"
               "are work in progress.\n\n");
        exit(0);
    }

    // We want the data visible in the log file asap!
    setbuf(stdout, NULL);

    warningSilenced(cmdline->silence);
    verboseEnabled(cmdline->verbose);
    logTelegramsEnabled(cmdline->logtelegrams);
    debugEnabled(cmdline->debug);

    if (cmdline->exitafter != 0) {
        verbose("(cmdline) wmbusmeters will exit after %d seconds\n", cmdline->exitafter);
    }

    if (cmdline->meterfiles) {
        verbose("(cmdline) store meter files in: \"%s\"\n", cmdline->meterfiles_dir);
    }
    verbose("(cmdline) using usb device: %s\n", cmdline->usb_device);
    verbose("(cmdline) number of meters: %d\n", cmdline->meters.size());

    auto manager = createSerialCommunicationManager(cmdline->exitafter);

    onExit(call(manager,stop));

    WMBus *wmbus = NULL;

    auto type_and_device = detectMBusDevice(cmdline->usb_device, manager);

    switch (type_and_device.first) {
    case DEVICE_IM871A:
        verbose("(im871a) detected on %s\n", type_and_device.second.c_str());
        wmbus = openIM871A(type_and_device.second, manager);
        break;
    case DEVICE_AMB8465:
        verbose("(amb8465) detected on %s\n", type_and_device.second.c_str());
        wmbus = openAMB8465(type_and_device.second, manager);
        break;
    case DEVICE_SIMULATOR:
        verbose("(simulator) found %s\n", type_and_device.second.c_str());
        wmbus = openSimulator(type_and_device.second, manager);
        break;
    case DEVICE_UNKNOWN:
        error("No wmbus device found!\n");
        exit(1);
        break;
    }

    if (!cmdline->link_mode_set) {
        // The link mode is not explicitly set. Examine the meters to see which
        // link mode to use.
        for (auto &m : cmdline->meters) {
            if (!cmdline->link_mode_set) {
                cmdline->link_mode = toMeterLinkMode(m.type);
                cmdline->link_mode_set = true;
            } else {
                if (cmdline->link_mode != toMeterLinkMode(m.type)) {
                    error("A different link mode has been set already.\n");
                }
            }
        }
    }
    if (!cmdline->link_mode_set) {
        error("If you specify no meters, you have to specify the link mode: --c1 or --t1\n");
    }
    wmbus->setLinkMode(cmdline->link_mode);
    string using_link_mode = linkModeName(wmbus->getLinkMode());

    verbose("(cmdline) using link mode: %s\n", using_link_mode.c_str());

    Printer *output = new Printer(cmdline->json, cmdline->fields,
                                  cmdline->separator, cmdline->meterfiles, cmdline->meterfiles_dir,
                                  cmdline->shells);

    if (cmdline->meters.size() > 0) {
        for (auto &m : cmdline->meters) {
            const char *keymsg = (m.key[0] == 0) ? "not-encrypted" : "encrypted";
            switch (toMeterType(m.type)) {
            case MULTICAL21_METER:
                m.meter = createMultical21(wmbus, m.name, m.id, m.key, MULTICAL21_METER);
                verbose("(multical21) configured \"%s\" \"multical21\" \"%s\" %s\n", m.name, m.id, keymsg);
                break;
            case FLOWIQ3100_METER:
                m.meter = createMultical21(wmbus, m.name, m.id, m.key, FLOWIQ3100_METER);
                verbose("(flowiq3100) configured \"%s\" \"flowiq3100\" \"%s\" %s\n", m.name, m.id, keymsg);
                break;
            case MULTICAL302_METER:
                m.meter = createMultical302(wmbus, m.name, m.id, m.key);
                verbose("(multical302) configured \"%s\" \"multical302\" \"%s\" %s\n", m.name, m.id, keymsg);
                break;
            case OMNIPOWER_METER:
                m.meter = createOmnipower(wmbus, m.name, m.id, m.key);
                verbose("(omnipower) configured \"%s\" \"omnipower\" \"%s\" %s\n", m.name, m.id, keymsg);
                break;
            case SUPERCOM587_METER:
                m.meter = createSupercom587(wmbus, m.name, m.id, m.key);
                verbose("(supercom587) configured \"%s\" \"supercom587\" \"%s\" %s\n", m.name, m.id, keymsg);
                break;
            case IPERL_METER:
                m.meter = createIperl(wmbus, m.name, m.id, m.key);
                verbose("(iperl) configured \"%s\" \"iperl\" \"%s\" %s\n", m.name, m.id, keymsg);
                break;
            case UNKNOWN_METER:
                error("No such meter type \"%s\"\n", m.type);
                break;
            }
            if (cmdline->list_shell_envs) {
                string ignore1, ignore2, ignore3;
                vector<string> envs;
                m.meter->printMeter(&ignore1,
                                    &ignore2, cmdline->separator,
                                    &ignore3,
                                    &envs);
                printf("Environment variables provided to shell for meter %s:\n", m.type);
                for (auto &e : envs) {
                    int p = e.find('=');
                    string key = e.substr(0,p);
                    printf("%s\n", key.c_str());
                }
                exit(0);
            }
            m.meter->onUpdate(calll(output,print,Meter*));
            m.meter->onUpdate([cmdline,manager](Meter*meter) { oneshotCheck(cmdline,manager,meter); });
        }
    } else {
        printf("No meters configured. Printing id:s of all telegrams heard!\n\n");

        wmbus->onTelegram([](Telegram *t){t->print();});
    }

    if (type_and_device.first == DEVICE_SIMULATOR) {
        wmbus->simulate();
    }

    manager->waitForStop();
}

void oneshotCheck(CommandLine *cmdline, SerialCommunicationManager *manager, Meter *meter)
{
    if (!cmdline->oneshot) return;

    for (auto &m : cmdline->meters) {
	if (m.meter->numUpdates() == 0) return;
    }
    // All meters have received at least one update! Stop!
    manager->stop();
}
