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

#ifndef DRIVERS_H_
#define DRIVERS_H_

#include<cstdint>
#include<string>

struct BuiltinDriver
{
    const char *name;
    const char *aliases;
    const char *content;
    bool loaded;
};

struct MVT
{
    uint16_t mfct;
    uint8_t version;
    uint8_t type;
};

struct MapToDriver {
    MVT mvt;
    const char *name;
};

void prepareBuiltinDrivers();
void loadDriversFromDir(std::string dir);
bool loadBuiltinDriver(std::string driver_name);
void loadAllBuiltinDrivers();
const char *findBuiltinDriver(uint16_t mfct, uint8_t ver, uint8_t type);
void removeBuiltinDriver(std::string driver_name);

#endif
