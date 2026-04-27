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

#include "always.h"
#include "crc16.h"

#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>

#define CRC16_EN_13757 0x3D65

constexpr std::array<uint16_t, 256> generate_crc16_en_13757_table()
{
    std::array<uint16_t, 256> table{};
    for (unsigned i = 0; i < 256; ++i) {
        uint16_t crc = static_cast<uint16_t>(i << 8);
        for (unsigned bit = 0; bit < 8; ++bit) {
            if (crc & 0x8000)
                crc = (crc << 1) ^ CRC16_EN_13757;
            else
                crc = (crc << 1);
        }
        table[i] = crc;
    }
    return table;
}

inline constexpr std::array<uint16_t, 256> crc16_en_13757_table = generate_crc16_en_13757_table();

uint16_t crc16_EN13757(const uchar *__restrict data, const size_t len)
{
    assert(len == 0 || data != nullptr);

    uint16_t crc = 0x0000;
    for (size_t i = 0; i < len; ++i) {
        crc = (crc << 8) ^ crc16_en_13757_table[((crc >> 8) ^ data[i]) & 0xFF];
    }
    return ~crc;
}

#define CRC16_INIT_VALUE 0xFFFF
#define CRC16_GOOD_VALUE 0x0F47
#define CRC16_GOOD_VALUE_EXPECTED (~CRC16_GOOD_VALUE)
#define CRC16_POLYNOM    0x8408

constexpr std::array<uint16_t, 256> generate_crc16_ccitt_table()
{
    std::array<uint16_t, 256> table{};
    for (unsigned i = 0; i < 256; ++i) {
        uint16_t crc = static_cast<uint16_t>(i);
        for (unsigned bit = 0; bit < 8; ++bit) {
            if (crc & 1)
                crc = (crc >> 1) ^ CRC16_POLYNOM;
            else
                crc >>= 1;
        }
        table[i] = crc;
    }
    return table;
}

inline constexpr std::array<uint16_t, 256> crc16_ccitt_table = generate_crc16_ccitt_table();

uint16_t crc16_CCITT(const uchar *__restrict data, const size_t length)
{
    uint16_t crc = CRC16_INIT_VALUE;
    for (size_t i = 0; i < length; ++i) {
        crc = (crc >> 8) ^ crc16_ccitt_table[(crc & 0xFF) ^ data[i]];
    }
    return crc;
}

bool crc16_CCITT_check(const uchar *__restrict data, const size_t length)
{
    return crc16_CCITT(data, length) == CRC16_GOOD_VALUE_EXPECTED;
}