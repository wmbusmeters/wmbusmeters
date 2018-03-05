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
#include"util.h"

using namespace std;

CommandLine *parseCommandLine(int argc, char **argv) {

    CommandLine * c = new CommandLine;

    int i=1;
    if (argc < 2) {
        c->need_help = true;
        return c;
    }
    while (argv[i] && argv[i][0] == '-') {
        if (!strcmp(argv[i], "-h") || !strcmp(argv[i], "-help") || !strcmp(argv[i], "--help")) {
            c->need_help = true;
            return c;
        }
        if (!strcmp(argv[i], "--silence")) {
            c->silence = true;
            i++;
            continue;
        }
        if (!strcmp(argv[i], "--verbose")) {
            c->verbose = true;
            i++;
            continue;
        }
        if (!strcmp(argv[i], "--debug")) {
            c->debug = true;
            i++;
            continue;
        }
        if (!strcmp(argv[i], "--logtelegrams")) {
            c->logtelegrams = true;
            i++;
            continue;
        }
        if (!strncmp(argv[i], "--robot", 7)) {
            if (strlen(argv[i]) == 7 ||
                (strlen(argv[i]) == 12 &&
                 !strncmp(argv[i]+7, "=json", 5)))
            {
                c->json = true;
                c->fields = false;
            }
            else if (strlen(argv[i]) == 14 &&
                     !strncmp(argv[i]+7, "=fields", 7))
            {
                c->json = false;
                c->fields = true;
                c->separator = ';';
            } else {
                error("Unknown output format: \"%s\"\n", argv[i]+7);
            }
            i++;
            continue;
        }
        if (!strncmp(argv[i], "--separator=", 12)) {
            if (!c->fields) {
                error("You must specify --robot=fields before --separator=X\n");
            }
            if (strlen(argv[i]) != 13) {
                error("You must supply a single character as the field separator.\n");
            }
            c->separator = argv[i][12];
            i++;
            continue;
        }
        if (!strcmp(argv[i], "--meterfiles")) {
            c->meterfiles = true;
            i++;
            continue;
        }
        if (!strcmp(argv[i], "--oneshot")) {
            c->oneshot = true;
            i++;
            continue;
        }
        if (!strcmp(argv[i], "--")) {
            i++;
            break;
        }
        error("Unknown option \"%s\"\n", argv[i]);
    }

    c->usb_device = argv[i];
    i++;
    if (!c->usb_device) error("You must supply the usb device to which the wmbus dongle is connected.\n");
    verbose("Using usb device: %s\n", c->usb_device);

    if ((argc-i) % 4 != 0) {
        error("For each meter you must supply a: name,type,id and key.\n");
    }
    int num_meters = (argc-i)/4;
    verbose("Number of meters: %d\n", num_meters);

    for (int m=0; m<num_meters; ++m) {
        char *name = argv[m*4+i+0];
        char *type = argv[m*4+i+1];
        char *id = argv[m*4+i+2];
        char *key = argv[m*4+i+3];

        if (!isValidType(type)) error("Not a valid meter type \"%s\"\n", type);
        if (!isValidId(id)) error("Not a valid meter id \"%s\"\n", id);
        if (!isValidKey(key)) error("Not a valid meter key \"%s\"\n", key);
        c->meters.push_back(MeterInfo(name,type,id,key));
    }

    return c;
}
