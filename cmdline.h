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

#ifndef CMDLINE_H
#define CMDLINE_H

#include"meters.h"
#include<memory>
#include<string.h>
#include<vector>

using namespace std;

struct MeterInfo {
    char *name;
    char *type;
    char *id;
    char *key;

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
    string meterfiles_dir;
    bool json {};
    bool fields {};
    char separator { ';' };
    vector<string> shells;
    bool list_shell_envs {};
    bool oneshot {};
    int  exitafter {}; // Seconds to exit.
    char *usb_device {};
    LinkMode link_mode {};
    bool link_mode_set {};
    bool no_init {};
    vector<MeterInfo> meters;

    ~CommandLine() = default;
};

unique_ptr<CommandLine> parseCommandLine(int argc, char **argv);

#endif
