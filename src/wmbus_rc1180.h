/*
 Copyright (C) 2026 Fredrik Ohrstrom (gpl-3.0-or-later)

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

#ifndef WMBUS_RC1180_H
#define WMBUS_RC1180_H

#include"always.h"

#define WMBUS_RC1180_DEFAULT_BAUD_RATE 19200

enum class RcUartBaudRate : uchar
{
    b2400 = 1,
    b4800 = 2,
    b9600 = 3,
    b14400 = 4,
    b19200 = 5,
    b28800 = 6,
    b38400 = 7,
    b57600 = 8,
    b76800 = 9,
    b115200 = 10,
    b230400 = 11,
    invalid = 255,
};

RcUartBaudRate rcUartBaudRateFromBauds(int baud_rate);

#endif