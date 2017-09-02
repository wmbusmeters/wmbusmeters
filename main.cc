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
        printf("Usage: wmbusmeters [--verbose] [--robot] [usbdevice] { [meter_name] [meter_id] [meter_key] }* \n");
        printf("\nAdd more meter quadruplets to listen to more meters.\n");
        printf("Add --verbose for detailed debug information.\n");
        printf("    --robot for json output.\n");
	printf("    --meterfiles to create status files below tmp,\n"
	       "       named /tmp/meter_name, containing the latest reading.\n");
	printf("    --oneshot wait for an update from each meter, then quit.\n");
        exit(0);
    }

    verboseEnabled(cmdline->verbose);
    
    auto manager = createSerialCommunicationManager();

    onExit(call(manager,stop));
    
    auto wmbus = openIM871A(cmdline->usb_device, manager);

    wmbus->setLinkMode(C1a);    
    if (wmbus->getLinkMode()!=C1a) error("Could not set link mode to C1a\n");

    // We want the data visible in the log file asap!    
    setbuf(stdout, NULL);

    Printer *output = new Printer(cmdline->robot, cmdline->meterfiles);
    
    if (cmdline->meters.size() > 0) {
        for (auto &m : cmdline->meters) {
            verbose("Configuring meter: \"%s\" \"%s\" \"%s\"\n", m.name, m.id, m.key);
            m.meter = createMultical21(wmbus, m.name, m.id, m.key);
            m.meter->onUpdate(calll(output,print,Meter*));
	    m.meter->onUpdate([cmdline,manager](Meter*meter) { oneshotCheck(cmdline,manager,meter); });
        }
    } else {
        printf("No meters configured. Printing id:s of all telegrams heard! \n");
        printf("To configure a meter, add a triplet to the command line: name id key\n");
        printf("Where name is your string identifying the meter.\n");
        printf("      id is the 8 digits printed on the meter.\n");
        printf("      key is 32 hex digits with the aes key.\n\n");

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
