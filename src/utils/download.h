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

#ifndef UTILS_DOWNLOAD_H
#define UTILS_DOWNLOAD_H

void setNoNetwork(bool v);
void setBasicAuth(const char *cred);

void setDownloadDir(const char *dir);
const char *downloadDir();

// Returns 200 or 404 or 304 etc...
int download(const char *suffix, const char *file, const char *local_file);

#endif
