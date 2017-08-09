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
    
int main(int argc, char **argv)
{
    if (argc > 1) {
        if (!strcmp(argv[1], "--verbose")) {
            verboseEnabled(true);
        } else {
            printf("wmbusmeters version: " WMBUSMETERS_VERSION "\n\n");
            printf("Usage: wmbusmeters [--verbose]\n");
            exit(0);
        }
    }
    auto manager = createSerialCommunicationManager();

    onExit(call(manager,stop));
    
    auto wmbus = openIM871A("/dev/ttyUSB0", manager);

    wmbus->setLinkMode(C1a);    
    if (wmbus->getLinkMode()!=C1a) error("Could not set link mode to C1a\n");

    auto meter  = createMultical21(wmbus, "MyHouseWater", "12345678", "00112233445566778899AABBCCDDEEFF");

    meter->onUpdate([meter](){
            printf("%s\t%s\t% 3.3f m3\t%s\n",
                   meter->name().c_str(),
                   meter->id().c_str(),
                   meter->totalWaterConsumption(),
                   meter->datetimeOfUpdate().c_str());
        });
            
    manager->waitForStop();    
}
