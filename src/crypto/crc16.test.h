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

#ifndef CRYPTO_CRC16_TEST_H
#define CRYPTO_CRC16_TEST_H

#include"test.h"
#include"crypto/crc16.h"

static auto _crc16_suite = describe("crc", []()
{
    it("crc16_EN13757 of known 4-byte sequence", []()
    {
        unsigned char data[4] = { 0x01, 0xfd, 0x1f, 0x01 };
        uint16_t crc = crc16_EN13757(data, 4);
        assert(crc == 0xcc22);
    });

    it("crc16_EN13757 changes with last byte", []()
    {
        unsigned char data[4] = { 0x01, 0xfd, 0x1f, 0x00 };
        uint16_t crc = crc16_EN13757(data, 4);
        assert(crc == 0xf147);
    });

    it("crc16_EN13757 of 10-byte block", []()
    {
        uchar block[10] = { 0xEE, 0x44, 0x9A, 0xCE, 0x01, 0x00, 0x00, 0x80, 0x23, 0x07 };
        uint16_t crc = crc16_EN13757(block, 10);
        assert(crc == 0xaabc);
    });

    it("crc16_EN13757 of ASCII \"123456789\"", []()
    {
        uchar block[9] = { '1', '2', '3', '4', '5', '6', '7', '8', '9' };
        uint16_t crc = crc16_EN13757(block, 9);
        assert(crc == 0xc2b7);
    });
});

#endif // CRYPTO_CRC16_TEST_H
