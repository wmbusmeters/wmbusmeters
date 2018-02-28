// Copyright (c) 2017 Fredrik Öhrström
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

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
        printf("Usage: wmbusmeters [options] (auto | /dev/ttyUSBx) { [meter_name] [meter_id] [meter_key] }* \n\n");
        printf("Add more meter triplets to listen to more meters.\n");
        printf("Add --verbose for more detailed information on communication.\n");
        printf("    --robot for json output.\n");
	printf("    --meterfiles to create status files below tmp,\n"
	       "       named /tmp/meter_name, containing the latest reading.\n");
	printf("    --oneshot wait for an update from each meter, then quit.\n\n");
        printf("Specifying auto as the device will automatically look for usb\n");
        printf("wmbus dongles on /dev/im871a and /dev/amb8465\n\n");

        exit(0);
    }
    // We want the data visible in the log file asap!
    setbuf(stdout, NULL);

    warningSilenced(cmdline->silence);
    verboseEnabled(cmdline->verbose);
    debugEnabled(cmdline->debug);

    auto manager = createSerialCommunicationManager();

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
    case DEVICE_UNKNOWN:
        error("No wmbus device found!\n");
        exit(1);
        break;
    }

    wmbus->setLinkMode(LinkModeC1);
    if (wmbus->getLinkMode()!=LinkModeC1) error("Could not set link mode to receive C1 telegrams.\n");

    Printer *output = new Printer(cmdline->robot, cmdline->meterfiles);

    if (cmdline->meters.size() > 0) {
        for (auto &m : cmdline->meters) {
            m.meter = createMultical21(wmbus, m.name, m.id, m.key);
            verbose("(multical21) configured \"%s\" \"%s\" \"%s\"\n", m.name, m.id, m.key);
            m.meter->onUpdate(calll(output,print,Meter*));
	    m.meter->onUpdate([cmdline,manager](Meter*meter) { oneshotCheck(cmdline,manager,meter); });
        }
    } else {
        printf("No meters configured. Printing id:s of all telegrams heard!\n\n");

        wmbus->onTelegram([](Telegram *t){t->print();});
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
