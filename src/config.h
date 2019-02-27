/*
 Copyright (C) 2019 Fredrik Öhrström

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

#ifndef CONFIG_H
#define CONFIG_H

#include"util.h"
#include"wmbus.h"
#include<vector>

using namespace std;

struct MeterInfo {
    string name;
    string type;
    string id;
    string key;

    MeterInfo(string& n, string& t, string& i, string& k) {
        name = n;
        type = t;
        id = i;
        key = k;
    }
};

enum class MeterFileType
{
    Overwrite, Append
};

struct Configuration {
    bool daemon {};
    std::string pid_file;
    bool useconfig {};
    std::string config_root;
    bool reload {};
    bool need_help {};
    bool silence {};
    bool verbose {};
    bool debug {};
    bool logtelegrams {};
    bool meterfiles {};
    std::string meterfiles_dir;
    MeterFileType meterfiles_action {};
    bool use_logfile {};
    std::string logfile;
    bool json {};
    bool fields {};
    char separator { ';' };
    std::vector<std::string> shells;
    bool list_shell_envs {};
    bool oneshot {};
    int  exitafter {}; // Seconds to exit.
    string device; // auto, /dev/ttyUSB0, simulation.txt, rtlwmbus
    string device_extra; // The frequency or the command line that will start rtlwmbus
    string telegram_reader;
    LinkMode link_mode {};
    bool link_mode_set {};
    bool no_init {};
    vector<MeterInfo> meters;

    ~Configuration() = default;
};

unique_ptr<Configuration> loadConfiguration(string root);


bool parseUseConfig(string arg, Configuration *config);


#endif
