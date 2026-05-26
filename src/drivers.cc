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

#include<cctype>
#include<cstring>
#include<vector>
#include<string>
#include<map>

#include"always.h"
#include"drivers.h"
#include"log.h"
#include"meters.h"
#include"util.h"

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

#include"generated_database.cc"

map<uint32_t,const char*> builtins_mvt_lookup_;
map<string,BuiltinDriver*> builtins_name_lookup_;

bool loadBuiltinDriver(string driver_name)
{
    // Resolve aliases via the DriverInfo stub registered by prepareBuiltinDrivers.
    DriverInfo *existing = lookupDriver(driver_name);
    if (existing == NULL) return false;

    if (!existing->isBuiltin())
    {
        // A driver has already been loaded! Skip loading the builtin driver.
        return true;
    }

    // The stub's primary name is what we use to find the xmq content.
    auto it = builtins_name_lookup_.find(existing->name().str());
    if (it == builtins_name_lookup_.end()) return false;

    BuiltinDriver *driver = it->second;
    if (driver->loaded) return true;

    loadDriver("", driver->content);
    driver->loaded = true;

    return true;
}

const char *findBuiltinDriver(uint16_t mfct, uchar ver, uchar type)
{
    auto find = [&](uint16_t m, uchar v, uchar t) -> const char*
    {
        uint32_t key = m << 16 | v << 8 | t;
        if (builtins_mvt_lookup_.count(key)) return builtins_mvt_lookup_[key];
        return NULL;
    };

    uint16_t normalized_mfct = mfct & 0x7fff;
    uint16_t mfcts[] = { mfct, normalized_mfct };
    uchar versions[] = { ver, 0xff };
    uchar types[] = { type, 0xff };

    for (uint16_t m : mfcts)
    {
        for (uchar v : versions)
        {
            for (uchar t : types)
            {
                const char *name = find(m, v, t);
                if (name) return name;
            }
        }
    }
    return NULL;
}

void removeBuiltinDriver(string name)
{
    builtins_name_lookup_.erase(name);
    debug("(drivers) removed builtin driver %s\n", name.c_str());

    size_t num_mvts = sizeof(builtins_mvts_) / sizeof(MapToDriver);

    for (size_t i = 0; i < num_mvts; ++i)
    {
        uint32_t key = builtins_mvts_[i].mvt.mfct << 16 |
            builtins_mvts_[i].mvt.version << 8 |
            builtins_mvts_[i].mvt.type;
        if (name == builtins_mvts_[i].name)
        {
            builtins_mvt_lookup_.erase(key);
        }
    }
}

// Register a builtin xmq driver: index it by name and add a lazy DriverInfo
// stub so it shows up in allDrivers() (used by --list-meters, polling, etc.)
// without paying for an xmq parse until the driver is actually needed.
// Aliases are stored on the stub and resolved via lookupDriver, so
// builtins_name_lookup_ only needs the primary name.
static void addBuiltinDriver(BuiltinDriver *bd)
{
    builtins_name_lookup_[bd->name] = bd;
    debug("(drivers) added builtin driver %s\n", bd->name);

    // Do not shadow a driver that is already registered (e.g. a compiled-in c++
    // driver or an xmq driver loaded from /etc/wmbusmeters.drivers.d).
    if (lookupDriver(bd->name) != NULL) return;

    DriverInfo di;
    di.setName(bd->name);
    di.setAliases(bd->aliases);
    di.setBuiltin(true);
    di.setDynamic("builtin", NULL); // So --listmeters shows "(builtin)" rather than "(c++)".
    addRegisteredDriver(di);
}

string findBuiltinMeterType(const string &name)
{
    auto it = builtins_name_lookup_.find(name);
    if (it == builtins_name_lookup_.end()) return "";

    // Cheap scan over the embedded xmq content to find meter_type=...
    // (avoids a full xmq parse just to discover the meter type).
    const char *p = strstr(it->second->content, "meter_type=");
    if (!p) return "";

    p += strlen("meter_type=");
    const char *end = p;
    while (*end && !isspace((unsigned char)*end) && *end != '}') end++;
    return string(p, end - p);
}

void prepareBuiltinDrivers()
{
    size_t num_mvts = sizeof(builtins_mvts_) / sizeof(MapToDriver);
    size_t num_drivers = sizeof(builtins_) / sizeof(BuiltinDriver);

    for (size_t i = 0; i < num_drivers; ++i)
    {
        addBuiltinDriver(&builtins_[i]);
    }

    for (size_t i = 0; i < num_mvts; ++i)
    {
        uint32_t key = builtins_mvts_[i].mvt.mfct << 16 |
            builtins_mvts_[i].mvt.version << 8 |
            builtins_mvts_[i].mvt.type;
        builtins_mvt_lookup_[key] = builtins_mvts_[i].name;
    }
}
