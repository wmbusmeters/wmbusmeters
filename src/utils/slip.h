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

#ifndef UTILS_SLIP_H
#define UTILS_SLIP_H

#include"always.h"

#include<unistd.h>
#include<vector>

// Check if buffer is all SLIP END = 0xc0.
bool slipAllEND(std::vector<uchar>& msg);
// Scan the buffer after the byte SLIP END = 0xc0
// return its index if exists, otherwise -1.
ssize_t slipFrameSize(std::vector<uchar>& msg);
// Add a SLIP_END and escape any 0xc0 with 0xdbdc and and 0xdb with 0xdbdd.
void addSlipFraming(std::vector<uchar>& from, std::vector<uchar> &to);
// Frame length is set to zero if no frame was found.
void removeSlipFraming(std::vector<uchar>& from, size_t *frame_length, std::vector<uchar> &to);

#endif
