/*
 Copyright (C) 2024 Fredrik Öhrström (gpl-3.0-or-later)

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

#include<vector>
#include<string>
#include<map>

#include"util.h"
#include"drivers.h"
#include"meters.h"

using namespace std;

void loadDriversFromDir(std::string dir)
{
    if (!checkIfDirExists(dir.c_str()))
    {
        debug("(drivers) dir did not exist: %s\n", dir.c_str());
        return;
    }

    verbose("(drivers) scanning dir %s\n", dir.c_str());

    vector<string> drivers;
    listFiles(dir, &drivers);

    for (string &file : drivers)
    {
        string file_name = dir+"/"+file;
        string s = loadDriver(file_name, NULL);
    }
}

struct BuiltinDriver
{
    const char *name;
    const char *content;
    bool loaded;
};

struct MVT
{
    uint16_t mfct;
    uchar version;
    uchar type;
};

struct MapToDriver {
    MVT mvt;
    const char *name;
};

#include"generated_database.cc"

map<uint32_t,const char*> builtins_mvt_lookup_;
map<string,BuiltinDriver*> builtins_name_lookup_;

bool loadBuiltinDriver(string driver_name)
{
    // Check that there is such a builtin driver.
    if (builtins_name_lookup_.count(driver_name) == 0) return false;

    if (lookupDriver(driver_name))
    {
        // A driver has already been loaded! Skip loading the builtin driver.
        return true;
    }

    BuiltinDriver *driver = builtins_name_lookup_[driver_name];
    if (driver->loaded) return true;

    string name = loadDriver("", driver->content);
    driver->loaded = true;

    return true;
}

void loadAllBuiltinDrivers()
{
    for (auto &p : builtins_name_lookup_)
    {
        if (!p.second->loaded)
        {
            loadBuiltinDriver(p.first);
            p.second->loaded = true;
        }
    }
}

const char *findBuiltinDriver(uint16_t mfct, uchar ver, uchar type)
{
    uint32_t key = mfct << 16  | ver << 8 | type;
    if (builtins_mvt_lookup_.count(key) == 0)
    {
        // Workaround for weird aPT and iTW mfcts.
        key = (mfct & 0x7fff) << 16  | ver << 8 | type;
        if (builtins_mvt_lookup_.count(key) == 0) return NULL;
    }
    return builtins_mvt_lookup_[key];
}

void prepareBuiltinDrivers()
{
    size_t num_mvts = sizeof(builtins_mvts_) / sizeof(MapToDriver);
    size_t num_drivers = sizeof(builtins_) / sizeof(BuiltinDriver);

    for (size_t i = 0; i < num_drivers; ++i)
    {
        builtins_name_lookup_[builtins_[i].name] = &builtins_[i];
        debug("(drivers) added builtin driver %s\n", builtins_[i].name);
    }
    for (size_t i = 0; i < num_mvts; ++i)
    {
        uint32_t key = builtins_mvts_[i].mvt.mfct << 16 |
            builtins_mvts_[i].mvt.version << 8 |
            builtins_mvts_[i].mvt.type;
        builtins_mvt_lookup_[key] = builtins_mvts_[i].name;
    }
}
