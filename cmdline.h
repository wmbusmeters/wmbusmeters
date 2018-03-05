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

#ifndef CMDLINE_H
#define CMDLINE_H

#include"meters.h"
#include<string.h>
#include<vector>

using namespace std;

struct MeterInfo {
    char *name;
    char *type;
    char *id;
    char *key;
    Meter *meter;

    MeterInfo(char *n, char *t, char *i, char *k) {
        name = n;
        type = t;
        id = i;
        key = k;
    }
};

struct CommandLine {
    bool need_help {};
    bool silence {};
    bool verbose {};
    bool debug {};
    bool logtelegrams {};
    bool meterfiles {};
    const char *meterfiles_dir {};
    bool json {};
    bool fields {};
    char separator { ';' };
    bool oneshot {};
    char *usb_device {};
    vector<MeterInfo> meters;
};

CommandLine *parseCommandLine(int argc, char **argv);

#endif
