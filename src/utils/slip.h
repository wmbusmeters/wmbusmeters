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

enum class SlipFrameResult
{
    OK,      // Complete frame decoded. frame_length > 0; to holds decoded bytes.
    PARTIAL, // No trailing END seen yet; collect more data and retry. frame_length == 0.
    CORRUPT, // Invalid escape sequence. Output cleared. frame_length == bytes consumed
             // up to and including the bad byte, so the caller can resynchronise.
};

// Check if buffer is all SLIP END = 0xc0.
// Returns true for an empty vector (vacuous truth); callers that need to
// distinguish an empty buffer should check msg.size() first.
bool slipAllEND(std::vector<uchar>& msg);

// Return the encoded (wire) byte count of the next frame, excluding the trailing
// SLIP_END. Escape pairs (e.g. 0xDB 0xDC) count as two wire bytes even though
// they decode to one payload byte. Returns -1 if no frame terminator is found.
ssize_t slipFrameSize(std::vector<uchar>& msg);

// Add a SLIP_END and escape any 0xc0 with 0xdbdc and 0xdb with 0xdbdd.
void addSlipFraming(std::vector<uchar>& from, std::vector<uchar> &to);

// Decode one SLIP frame from `from` into `to`.
//   OK      – frame decoded; frame_length = wire bytes consumed (including terminators).
//   PARTIAL – no trailing END yet; frame_length = 0; caller should collect more data.
//   CORRUPT – invalid escape sequence; output cleared; frame_length = wire bytes consumed
//             up to the bad byte so the caller can skip past the corrupt frame.
SlipFrameResult removeSlipFraming(std::vector<uchar>& from, size_t *frame_length, std::vector<uchar> &to);

#endif
