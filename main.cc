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
#include"serial.h"
#include"util.h"
#include"wmbus.h"

#include<string.h>

using namespace std;

void printMeter(Meter *meter) {
    printf("%s\t%s\t% 3.3f m3\t%s\t% 3.3f m3\t%s\n",
           meter->name().c_str(),
           meter->id().c_str(),
           meter->totalWaterConsumption(),
           meter->datetimeOfUpdateHumanReadable().c_str(),
           meter->targetWaterConsumption(), 
           meter->statusHumanReadable().c_str());

// targetWaterConsumption: The total consumption at the start of the previous 30 day period.    
// statusHumanReadable: DRY,REVERSED,LEAK,BURST if that status is detected right now, followed by
//                      (dry 15-21 days) which means that, even it DRY is not active right now,
//                      DRY has been active for 15-21 days during the last 30 days.    
}

#define Q(x,y) "\""#x"\":"#y","
#define QS(x,y) "\""#x"\":\""#y"\","
#define QSE(x,y) "\""#x"\":\""#y"\""

void printMeterJSON(Meter *meter) {
    printf("{"
           QS(name,%s)
           QS(id,%s)
           Q(total_m3,%.3f)
           Q(target_m3,%.3f)
           QS(current_status,%s)
           QS(time_dry,%s)
           QS(time_reversed,%s)
           QS(time_leaking,%s)
           QS(time_bursting,%s)
           QSE(timestamp,%s)
           "}\n",
           meter->name().c_str(),
           meter->id().c_str(),
           meter->totalWaterConsumption(),
           meter->targetWaterConsumption(), 
           meter->status().c_str(), // DRY REVERSED LEAK BURST
           meter->timeDry().c_str(),
           meter->timeReversed().c_str(),
           meter->timeLeaking().c_str(),
           meter->timeBursting().c_str(),
           meter->datetimeOfUpdateRobot().c_str());
}

int main(int argc, char **argv)
{
    CommandLine *c = parseCommandLine(argc, argv);
    
    if (c->need_help) {
        printf("wmbusmeters version: " WMBUSMETERS_VERSION "\n");
        printf("Usage: wmbusmeters [--verbose] [--robot] [usbdevice] { [meter_name] [meter_id] [meter_key] }* \n");
        printf("\nAdd more meter triplets to listen to more meters.\n");
        printf("Add --verbose for detailed debug information.\n");
        printf("     --robot for json output.\n");
        exit(0);
    }

    verboseEnabled(c->verbose);
    
    auto manager = createSerialCommunicationManager();

    onExit(call(manager,stop));
    
    auto wmbus = openIM871A(c->usb_device, manager);

    wmbus->setLinkMode(C1a);    
    if (wmbus->getLinkMode()!=C1a) error("Could not set link mode to C1a\n");

    // We want the data visible in the log file asap!    
    setbuf(stdout, NULL);
    
    if (c->meters.size() > 0) {
        for (auto &m : c->meters) {
            verbose("Configuring meter: \"%s\" \"%s\" \"%s\"\n", m.name, m.id, m.key);
            Meter *meter=createMultical21(wmbus, m.name, m.id, m.key);
            if (c->robot) meter->onUpdate(printMeterJSON);
            else meter->onUpdate(printMeter);
        }
    } else {
        printf("No meters configured. Printing id:s of all telegrams heard! \n");
        printf("To configure a meter, add a triplet to the command line: name id key\n");
        printf("Where name is your string identifying the meter.\n");
        printf("      id is the 8 digits printed on the meter.\n");
        printf("      key is 32 hex digits with the aes key.\n\n");

        wmbus->onTelegram([](Telegram *t){
                printf("Received telegram from id: %02x%02x%02x%02x \n",
                       t->a_field_address[0], t->a_field_address[1], t->a_field_address[2], t->a_field_address[3]);
            });
    }

    manager->waitForStop();    
}
