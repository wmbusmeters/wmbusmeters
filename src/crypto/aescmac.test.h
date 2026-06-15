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

#ifndef CRYPTO_AESCMAC_TEST_H
#define CRYPTO_AESCMAC_TEST_H

#include"test.h"
#include"crypto/aescmac.h"
#include"util.h"

#include<vector>

static auto _aescmac_suite = describe("kdf", []()
{
    it("AES-CMAC of empty message with RFC4493 test key", []()
    {
        std::vector<uchar> key;
        std::vector<uchar> input;
        std::vector<uchar> mac(16);
        hex2bin("2b7e151628aed2a6abf7158809cf4f3c", &key);
        AES_CMAC(safeButUnsafeVectorPtr(key),
                 safeButUnsafeVectorPtr(input), 0,
                 safeButUnsafeVectorPtr(mac));
        assert(bin2hex(mac) == "BB1D6929E95937287FA37D129B756746");
    });

    it("AES-CMAC of 16-byte message with RFC4493 test key", []()
    {
        std::vector<uchar> key;
        std::vector<uchar> input;
        std::vector<uchar> mac(16);
        hex2bin("2b7e151628aed2a6abf7158809cf4f3c", &key);
        hex2bin("6bc1bee22e409f96e93d7e117393172a", &input);
        AES_CMAC(safeButUnsafeVectorPtr(key),
                 safeButUnsafeVectorPtr(input), 16,
                 safeButUnsafeVectorPtr(mac));
        assert(bin2hex(mac) == "070A16B46B4D4144F79BDD9DD04A287C");
    });
});

#endif // CRYPTO_AESCMAC_TEST_H
