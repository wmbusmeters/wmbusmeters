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

#ifndef CRYPTO_AES_TEST_H
#define CRYPTO_AES_TEST_H

#include"test.h"
#include"crypto/aes.h"
#include"util.h"

#include<cstring>
#include<vector>

static auto _aes_suite = describe("aes", []()
{
    it("AES-CBC encrypt/decrypt round-trips with IV", []()
    {
        std::vector<uchar> key;
        hex2bin("0123456789abcdef0123456789abcdef", &key);

        std::string poe =
            "Once upon a midnight dreary, while I pondered, weak and weary,\n"
            "Over many a quaint and curious volume of forgotten lore\n";
        while (poe.length() % 16 != 0) poe += ".";

        uchar iv[16];
        memset(iv, 0xaa, 16);

        uchar in[128];
        memcpy(in, poe.data(), poe.size());

        uchar out[sizeof(in)];
        AES_CBC_encrypt_buffer(out, in, poe.size(), safeButUnsafeVectorPtr(key), iv);

        uchar back[sizeof(in)];
        AES_CBC_decrypt_buffer(back, out, poe.size(), safeButUnsafeVectorPtr(key), iv);

        assert(std::string(back, back + poe.size()) == poe);
    });

    it("AES-ECB encrypt/decrypt round-trips without IV", []()
    {
        std::vector<uchar> key;
        hex2bin("0123456789abcdef0123456789abcdef", &key);

        std::string poe =
            "Once upon a midnight dreary, while I pondered, weak and weary,\n"
            "Over many a quaint and curious volume of forgotten lore\n";
        while (poe.length() % 16 != 0) poe += ".";

        uchar in[128];
        memcpy(in, poe.data(), poe.size());

        uchar out[sizeof(in)];
        AES_ECB_encrypt(in, safeButUnsafeVectorPtr(key), out, poe.size());

        uchar back[sizeof(in)];
        AES_ECB_decrypt(out, safeButUnsafeVectorPtr(key), back, poe.size());

        assert(memcmp(back, in, poe.size()) == 0);
    });
});

#endif // CRYPTO_AES_TEST_H
