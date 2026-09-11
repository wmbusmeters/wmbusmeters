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

#ifndef UTILS_FS_H
#define UTILS_FS_H

#include<string>
#include<vector>

bool checkCharacterDeviceExists(const char *tty, bool fail_if_not);
bool checkFileExists(const char *file);
bool checkIfSimulationFile(const char *file);
bool checkIfDirExists(const char *dir);
bool listFiles(const std::string& dir, std::vector<std::string> *files);
int loadFile(const std::string& file, std::vector<std::string> *lines);
bool loadFile(const std::string& file, std::vector<char> *buf);
bool appendFile(const std::string& file, const std::string &line);

#endif
