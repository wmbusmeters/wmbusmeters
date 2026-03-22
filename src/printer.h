/*
 Copyright (C) 2017-2021 Fredrik Öhrström (gpl-3.0-or-later)

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

#include"cmdline.h"
#include"meters.h"
#include"wmbus.h"

struct Printer {
    Printer(bool json,
            bool pretty_print_json,
            bool fields,
            char separator,
            bool meterfiles, std::string &meterfiles_dir,
            bool use_logfile, std::string &logfile,
            std::vector<std::string> new_meter_shell_cmdlines,
            std::vector<std::string> shell_cmdlines,
            bool overwrite,
            MeterFileNaming naming,
            MeterFileTimestamp timestamp);

    void print(Telegram *t, Meter *meter, std::vector<std::string> *more_json, std::vector<std::string> *selected_fields);

    private:

    bool json_, pretty_print_json_, fields_;
    bool use_meterfiles_;
    std::string meterfiles_dir_;
    bool use_logfile_;
    std::string logfile_;
    char separator_;
    std::vector<std::string> new_meter_shell_cmdlines_;
    std::vector<std::string> shell_cmdlines_;
    bool overwrite_;
    MeterFileNaming naming_;
    MeterFileTimestamp timestamp_;

    void printNewMeterShells(Meter *meter, std::vector<std::string> &envs);
    void printShells(Meter *meter, std::vector<std::string> &envs);
    void printFiles(Meter *meter, Telegram *t, std::string &human_readable, std::string &fields, std::string &json);

};
