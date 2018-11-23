/*
 Copyright (C) 2018 Fredrik Öhrström

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

#include"aes.h"
#include"wmbus.h"

void decryptMode1_AES_CTR(Telegram *t, vector<uchar> &aeskey, const char *meter_name)
{
    vector<uchar> content;
    content.insert(content.end(), t->payload.begin(), t->payload.end());
    size_t remaining = content.size();
    if (remaining > 16) remaining = 16;

    uchar iv[16];
    int i=0;
    // M-field
    iv[i++] = t->m_field&255; iv[i++] = t->m_field>>8;
    // A-field
    for (int j=0; j<6; ++j) { iv[i++] = t->a_field[j]; }
    // CC-field
    iv[i++] = t->cc_field;
    // SN-field
    for (int j=0; j<4; ++j) { iv[i++] = t->sn[j]; }
    // FN
    iv[i++] = 0; iv[i++] = 0;
    // BC
    iv[i++] = 0;

    vector<uchar> ivv(iv, iv+16);
    string s = bin2hex(ivv);
    debug("(%s) IV   %s\n", meter_name, s.c_str());

    uchar xordata[16];
    AES_ECB_encrypt(iv, &aeskey[0], xordata, 16);

    uchar decrypt[16];
    xorit(xordata, &content[0], decrypt, remaining);

    vector<uchar> dec(decrypt, decrypt+remaining);
    debugPayload("(C1) decrypted", dec);

    if (content.size() > 22) {
        warning("(%s) warning: C1 decryption received too many bytes of content! "
                "Got %zu bytes, expected at most 22.\n", meter_name, content.size());
    }
    if (content.size() > 16) {
        // Yay! Lets decrypt a second block. Full frame content is 22 bytes.
        // So a second block should enough for everyone!
        remaining = content.size()-16;
        if (remaining > 16) remaining = 16; // Should not happen.

        incrementIV(iv, sizeof(iv));
        vector<uchar> ivv2(iv, iv+16);
        string s2 = bin2hex(ivv2);
        debug("(%s) IV+1 %s\n", meter_name, s2.c_str());

        AES_ECB_encrypt(iv, &aeskey[0], xordata, 16);

        xorit(xordata, &content[16], decrypt, remaining);

        vector<uchar> dec2(decrypt, decrypt+remaining);
        debugPayload("(C1) decrypted", dec2);

        // Append the second decrypted block to the first.
        dec.insert(dec.end(), dec2.begin(), dec2.end());
    }
    t->content.clear();
    t->content.insert(t->content.end(), dec.begin(), dec.end());
}

string frameTypeKamstrupC1(int ft) {
    if (ft == 0x78) return "long frame";
    if (ft == 0x79) return "short frame";
    return "?";
}

void decryptMode5_AES_CBC(Telegram *t, vector<uchar> &aeskey, const char *meter_name)
{
    vector<uchar> content;
    content.insert(content.end(), t->payload.begin(), t->payload.end());
    // The content should be a multiple of 16 since we are using AES CBC mode.
    if (content.size() % 16 != 0)
    {
        warning("(%s) warning: T1 decryption received non-multiple of 16 bytes! "
                "Got %zu bytes shrinking message to %zu bytes.\n",
                meter_name, content.size(), content.size() - content.size() % 16);
        while (content.size() % 16 != 0)
        {
            content.pop_back();
        }
    }

    uchar iv[16];
    int i=0;
    // M-field
    iv[i++] = t->m_field&255; iv[i++] = t->m_field>>8;
    // A-field
    for (int j=0; j<6; ++j) { iv[i++] = t->a_field[j]; }
    // ACC
    iv[i++] = t->acc;
    // SN-field
    for (int j=0; j<4; ++j) { iv[i++] = t->acc; }
    // FN
    iv[i++] = t->acc; iv[i++] = t->acc;
    // BC
    iv[i++] = t->acc;

    vector<uchar> ivv(iv, iv+16);
    string s = bin2hex(ivv);
    verbose("(%s) IV   %s\n", meter_name, s.c_str());

    uchar decrypted_data[16];
    AES_CBC_decrypt_buffer(decrypted_data, &content[0], 16, &aeskey[0], iv);
    vector<uchar> decrypted(decrypted_data, decrypted_data+16);

    if (decrypted_data[0] != 0x2F || decrypted_data[1] != 0x2F) {
        verbose("(%s) decrypt failed!\n", meter_name);
    }

    t->content.clear();
    t->content.insert(t->content.end(), decrypted.begin(), decrypted.end());
    debugPayload("(T1) decrypted", t->content);
}

#endif
