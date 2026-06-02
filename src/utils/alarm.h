/*
 Copyright (C) 2017-2026 Fredrik Öhrström (gpl-3.0-or-later)

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

#ifndef UTILS_ALARM_H
#define UTILS_ALARM_H

#include <string>
#include <vector>

enum class Alarm
{
    DeviceFailure,
    RegularResetFailure,
    DeviceInactivity,
    SpecifiedDeviceNotFound
};

const char* toString(Alarm type);
void logAlarm(Alarm type, std::string info);
void setAlarmShells(std::vector<std::string> &alarm_shells);

#endif
