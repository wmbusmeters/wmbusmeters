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

#include "always.h"
#include "slip.h"

#define SLIP_END             0xc0    /* indicates end of packet */
#define SLIP_ESC             0xdb    /* indicates byte stuffing */
#define SLIP_ESC_END         0xdc    /* ESC ESC_END means END data byte */
#define SLIP_ESC_ESC         0xdd    /* ESC ESC_ESC means ESC data byte */

bool slipAllEND(std::vector<uchar>& msg)
{
    for (size_t i = 0; i < msg.size(); ++i)
    {
        uchar c = msg[i];
        if (c != SLIP_END) return false;
    }
    return true;
}

ssize_t slipFrameSize(std::vector<uchar>& msg)
{
    size_t i;

    // Skip any leading 0xc0, they do not count.
    for (i = 0; i < msg.size(); ++i)
    {
        uchar c = msg[i];
        if (c != SLIP_END) break;
    }

    size_t from = i;
    for (; i < msg.size(); ++i)
    {
        uchar c = msg[i];
        if (c == SLIP_END) return (ssize_t)(i-from);
    }
    return -1;
}

void addSlipFraming(std::vector<uchar>& from, std::vector<uchar> &to)
{
    to.push_back(SLIP_END);
    for (uchar c : from)
    {
        if (c == SLIP_END)
        {
            to.push_back(SLIP_ESC);
            to.push_back(SLIP_ESC_END);
        }
        else if (c == SLIP_ESC)
        {
            to.push_back(SLIP_ESC);
            to.push_back(SLIP_ESC_ESC);
        }
        else
        {
            to.push_back(c);
        }
    }
    to.push_back(SLIP_END);
}

void removeSlipFraming(std::vector<uchar>& from, size_t *frame_length, std::vector<uchar> &to)
{
    *frame_length = 0;
    to.clear();
    to.reserve(from.size());
    bool esc = false;
    size_t i;
    bool found_end = false;

    // Skip any leading C0:s aka SLIP_ENDs.
    for (i = 0; i < from.size(); ++i)
    {
        uchar c = from[i];
        if (c != SLIP_END) break;
    }

    for (; i < from.size(); ++i)
    {
        uchar c = from[i];
        if (c == SLIP_END)
        {
            found_end = true;
            i++;
            break;
        }
        else if (c == SLIP_ESC)
        {
            esc = true;
        }
        else if (esc)
        {
            esc = false;
            if (c == SLIP_ESC_END) to.push_back(SLIP_END);
            else if (c == SLIP_ESC_ESC) to.push_back(SLIP_ESC);
            else to.push_back(c); // This is an error......
        }
        else
        {
            to.push_back(c);
        }
    }

    if (found_end)
    {
        *frame_length = i;
    }
    else
    {
        *frame_length = 0;
        to.clear();
    }
}
