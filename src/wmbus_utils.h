/*
 Copyright (C) 2020 Fredrik Öhrström

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

#ifndef WMBUS_UTILS_H
#define WMBUS_UTILS_H

#include "util.h"
#include "threads.h"
#include "wmbus.h"

bool decrypt_ELL_AES_CTR(Telegram *t, vector<uchar> &frame, vector<uchar>::iterator &pos, vector<uchar> &aeskey);
bool decrypt_TPL_AES_CBC_IV(Telegram *t, vector<uchar> &frame, vector<uchar>::iterator &pos, vector<uchar> &aeskey);
bool decrypt_TPL_AES_CBC_NO_IV(Telegram *t, vector<uchar> &frame, vector<uchar>::iterator &pos, vector<uchar> &aeskey);
string frameTypeKamstrupC1(int ft);

#endif
