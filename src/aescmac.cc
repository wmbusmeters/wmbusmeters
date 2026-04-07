/*
 Copyright (C) 2020 Fredrik Öhrström (gpl-3.0-or-later)

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

#include"aes.h"
#include"aescmac.h"
#include"util.h"

#include <cstdint>
#include <memory.h>

uint8_t vec87[16] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x87
};

void generateSubkeys(uint8_t *key, uint8_t *K1, uint8_t *K2)
{
    uint8_t L[16];
    uint8_t Z[16];
    uint8_t tmp[16];

    memset(Z, 0, 16);

    AES_ECB_encrypt(Z, key, L, 16);

    if (!(L[0] & 0x80))
    {
        shiftLeft(L, K1, 16);
    }
    else
    {
        shiftLeft(L, tmp, 16);
        xorit(tmp, vec87, K1, 16);
    }

    if (!(K1[0] & 0x80))
    {
        shiftLeft(K1, K2, 16);
    }
    else
    {
        shiftLeft(K1, tmp, 16);
        xorit(tmp, vec87, K2, 16);
    }
}

void pad(uint8_t *in, uint8_t *out, int len)
{
    for (int i = 0; i < 16; i++)
    {
        if (i < len)
        {
            out[i] = in[i];
        }
        else if (i == len)
        {
            out[i] = 0x80;
        }
        else
        {
            out[i] = 0x00;
        }
    }
}

void AES_CMAC(uint8_t *key, uint8_t *input, int len, uint8_t *mac)
{
    bool len_is_multiple_of_block;
    uint8_t X[16], Y[16];
    uint8_t K1[16], K2[16];
    uint8_t M_last[16], padded[16];

    generateSubkeys(key, K1, K2);

    int num_blocks = (len+15)/16;

    if (!num_blocks)
    {
        num_blocks = 1;
        len_is_multiple_of_block = false;
    }
    else
    {
        len_is_multiple_of_block = !(len%16);
    }

    if (len_is_multiple_of_block)
    {
        xorit(input+(16*(num_blocks-1)), K1, M_last, 16);
    }
    else
    {
        pad(input+(16*(num_blocks-1)), padded, len%16);
        xorit(padded, K2, M_last, 16);
    }

    memset(X, 0, 16);

    for (int i=0; i<num_blocks-1; i++)
    {
        xorit(X, input+(16*i), Y, 16);
        AES_ECB_encrypt(Y, key, X, 16);
    }

    xorit(X,M_last,Y, 16);
    AES_ECB_encrypt(Y, key, X, 16);

    memcpy(mac, X, 16);
}
