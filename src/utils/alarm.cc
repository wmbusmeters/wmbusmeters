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

#include "log.h"
#include "shell.h"
#include "util.h"

#include "utils/alarm.h"

#include "string"
#include "vector"

std::vector<std::string> alarm_shells_;

const char* toString(Alarm type)
{
    switch (type)
    {
    case Alarm::DeviceFailure: return "DeviceFailure";
    case Alarm::RegularResetFailure: return "RegularResetFailure";
    case Alarm::DeviceInactivity: return "DeviceInactivity";
    case Alarm::SpecifiedDeviceNotFound: return "SpecifiedDeviceNotFound";
    }
    return "?";
}

void logAlarm(Alarm type, std::string info)
{
    std::vector<std::string> envs;
    std::string ts = toString(type);
    envs.push_back("ALARM_TYPE="+ts);

    std::string msg = tostrprintf("[ALARM %s] %s", ts.c_str(), info.c_str());
    envs.push_back("ALARM_MESSAGE="+msg);

    warning("%s\n", msg.c_str());

    for (auto &s : alarm_shells_)
    {
        std::vector<std::string> args;
        args.push_back("-c");
        args.push_back(s);
        invokeShell("/bin/sh", args, envs);
    }
}

void setAlarmShells(std::vector<std::string> &alarm_shells)
{
    alarm_shells_ = alarm_shells;
}