/*
 Copyright (C) 2017-2020 Fredrik Öhrström (gpl-3.0-or-later)

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

#include<string>
#include<vector>

void invokeShell(std::string program, std::vector<std::string> args, std::vector<std::string> envs);
int  invokeShellCaptureOutput(std::string program, std::vector<std::string> args, std::vector<std::string> envs, std::string *out, bool do_not_warn_if_fail);
bool invokeBackgroundShell(std::string program, std::vector<std::string> args, std::vector<std::string> envs, int *out, int *pid);
bool stillRunning(int pid);
void stopBackgroundShell(int pid);
void detectProcesses(std::string cmd, std::vector<int> *pids);
