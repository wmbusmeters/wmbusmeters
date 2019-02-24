/*
 Copyright (C) 2017-2019 Fredrik Öhrström

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

#include"printer.h"
#include"shell.h"

using namespace std;

Printer::Printer(bool json, bool fields, char separator, bool meterfiles, string &meterfiles_dir,
                 vector<string> shell_cmdlines)
{
    json_ = json;
    fields_ = fields;
    separator_ = separator;
    meterfiles_ = meterfiles;
    meterfiles_dir_ = meterfiles_dir;
    shell_cmdlines_ = shell_cmdlines;
}

void Printer::print(Meter *meter)
{
    string human_readable, fields, json;
    vector<string> envs;

    meter->printMeter(&human_readable, &fields, separator_, &json, &envs);

    if (shell_cmdlines_.size() > 0) {
        printShells(meter, envs);
    } else {
        printFiles(meter, human_readable, fields, json);
    }
}

void Printer::printShells(Meter *meter, vector<string> &envs)
{
    for (auto &s : shell_cmdlines_) {
        vector<string> args;
        args.push_back("-c");
        args.push_back(s);
        invokeShell("/bin/sh", args, envs);
    }
}

void Printer::printFiles(Meter *meter, string &human_readable, string &fields, string &json)
{
    FILE *output = stdout;

    if (meterfiles_) {
        char filename[128];
        memset(filename, 0, sizeof(filename));
        snprintf(filename, 127, "%s/%s", meterfiles_dir_.c_str(), meter->name().c_str());
        output = fopen(filename, "w");
        if (!output) {
            warning("Could not open file \"%s\" for writing!\n", filename);
        }
    }

    if (json_) {
        fprintf(output, "%s\n", json.c_str());
    }
    else if (fields_) {
        fprintf(output, "%s\n", fields.c_str());
    }
    else {
        fprintf(output, "%s\n", human_readable.c_str());
    }

    if (output != stdout) {
        fclose(output);
    }
}
