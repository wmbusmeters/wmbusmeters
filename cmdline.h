/*
 Copyright (C) 2017 Fredrik Öhrström

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
    int  exitafter {}; // Seconds to exit.
    char *usb_device {};
    LinkMode link_mode {};
    bool link_mode_set {};
    vector<MeterInfo> meters;
};

CommandLine *parseCommandLine(int argc, char **argv);

#endif
