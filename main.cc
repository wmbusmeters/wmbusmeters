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
 
#include"util.h"
#include"serial.h"
#include"wmbus.h"
#include"meters.h"
#include"aes.h"

#include<string.h>

using namespace std;

void printMeter(Meter *meter) {
    printf("%s\t%s\t% 3.3f m3\t%s\t% 3.3f m3\t%s\n",
           meter->name().c_str(),
           meter->id().c_str(),
           meter->totalWaterConsumption(),
           meter->datetimeOfUpdate().c_str(),
           meter->targetWaterConsumption(), 
           meter->statusHumanReadable().c_str());


// targetWaterConsumption: The total consumption at the start of the previous 30 day period.    
// statusHumanReadable: DRY,REVERSED,LEAK,BURST if that status is detected right now, followed by
//                      (dry 15-21 days) which means that, even it DRY is not active right now,
//                      DRY has been active for 15-21 days during the last 30 days.    
}

int main(int argc, char **argv)
{
    if (argc < 2) {
        printf("wmbusmeters version: " WMBUSMETERS_VERSION "\n\n");
        printf("Usage: wmbusmeters [--verbose] [usbdevice] { [meter_name] [meter_id] [meter_key] }* \n");
        exit(0);
    }

    int i=1;
    if (!strcmp(argv[i], "--verbose")) {
        verboseEnabled(true);
        i++;
    }

    char *usbdevice = argv[i];
    i++;
    if (!usbdevice) error("You must supply the usb device to which the wmbus dongle is connected.\n");
    verbose("Using usbdevice: %s\n", usbdevice);

    if ((argc-i) % 3 != 0) {
        error("For each meter you must supply a: name,id and key.\n");
    }
    int num_meters = (argc-i)/3;
    verbose("Number of meters: %d\n", num_meters);

    auto manager = createSerialCommunicationManager();

    onExit(call(manager,stop));
    
    auto wmbus = openIM871A(usbdevice, manager);

    wmbus->setLinkMode(C1a);    
    if (wmbus->getLinkMode()!=C1a) error("Could not set link mode to C1a\n");

    // We want the data visible in the log file asap!    
    setbuf(stdout, NULL);
    
    if (num_meters > 0) {
        Meter *meters[num_meters];

        for (int m=0; m<num_meters; ++m) {
            char *name = argv[m*3+i+0];
            char *id = argv[m*3+i+1];
            char *key = argv[m*3+i+2];

            if (!isValidId(id)) error("Not a valid meter id \"%s\"\n", id);
            if (!isValidKey(key)) error("Not a valid meter key \"%s\"\n", key);
            
            verbose("Configuring meter: \"%s\" \"%s\" \"%s\"\n", name, id, key);
            meters[m] = createMultical21(wmbus, name, id, key);
            meters[m]->onUpdate(printMeter);
        }
    } else {
        printf("No meters configured. Printing id:s of all telegrams heard! Add --verbose to get more data.\n");
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
