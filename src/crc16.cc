/*
 Copyright (C) 2022 Fredrik Öhrström (gpl-3.0-or-later)

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

#include "crc16.h"

#include <cassert>

uint16_t crc16(uint16_t poly, uint16_t init, uchar *data, size_t len)
{
    uint16_t crc = init;

    assert(len == 0 || data != NULL);

    for (size_t i = 0; i < len; i++)
    {
        uchar b = data[i];
        for (int j = 0; j < 8; j++)
        {
            if (((crc & 0x8000) >> 8) ^ (b & 0x80))
            {
                crc = (crc << 1) ^ poly;
            }
            else
            {
                crc = (crc << 1);
            }
            b <<= 1;
        }
    }

    return crc;
}

#define CRC16_EN_13757_INIT_VALUE 0x0000
#define CRC16_EN_13757_POLYNOM    0x3D65

uint16_t crc16_EN13757(uchar *data, size_t len)
{
    return ~crc16(CRC16_EN_13757_POLYNOM, CRC16_EN_13757_INIT_VALUE, data, len);
}

/*
 CCITT stands for "Comité Consultatif International Télégraphique et
 Téléphonique", also known as "International Telecommunication Union
 Telecommunication Standardization Sector", or short "ITU-T".
 
 This CRC16 variation is used by im871a for its serial communication.
*/ 

#define CRC16_CCITT_INIT_VALUE 0xFFFF
#define CRC16_CCITT_POLYNOM    0x1021
#define CRC16_CCITT_GOOD_VALUE 0x0F47

uint16_t crc16_CCITT(uchar *data, uint16_t length)
{
    return crc16(CRC16_CCITT_POLYNOM, CRC16_CCITT_INIT_VALUE, data, length);
}

bool crc16_CCITT_check(uchar *data, uint16_t length)
{
    uint16_t crc = ~crc16_CCITT(data, length);
    return crc == CRC16_CCITT_GOOD_VALUE;
}

