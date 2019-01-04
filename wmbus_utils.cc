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


#include"aes.h"
#include"util.h"
#include"wmbus.h"

#include<memory.h>

void decryptMode1_AES_CTR(Telegram *t, vector<uchar> &aeskey)
{
    vector<uchar> content;
    content.insert(content.end(), t->payload.begin(), t->payload.end());
    debugPayload("(Mode1) decrypting", content);

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
    debug("(Mode1) IV %s\n", s.c_str());

    uchar xordata[16];
    AES_ECB_encrypt(iv, &aeskey[0], xordata, 16);

    uchar decrypt[16];
    xorit(xordata, &content[0], decrypt, remaining);

    vector<uchar> dec(decrypt, decrypt+remaining);
    debugPayload("(Mode1) decrypted first block", dec);

    if (content.size() > 16) {
        remaining = content.size()-16;
        if (remaining > 16) remaining = 16; // Should not happen.

        incrementIV(iv, sizeof(iv));
        vector<uchar> ivv2(iv, iv+16);
        string s2 = bin2hex(ivv2);
        debug("(Mode1) IV+1 %s\n", s2.c_str());

        AES_ECB_encrypt(iv, &aeskey[0], xordata, 16);

        xorit(xordata, &content[16], decrypt, remaining);

        vector<uchar> dec2(decrypt, decrypt+remaining);
        debugPayload("(Mode1) decrypted second block", dec2);

        // Append the second decrypted block to the first.
        dec.insert(dec.end(), dec2.begin(), dec2.end());
    }
    t->content.clear();
    t->content.insert(t->content.end(), dec.begin(), dec.end());
    debugPayload("(Mode1) decrypted", t->content);
}

string frameTypeKamstrupC1(int ft) {
    if (ft == 0x78) return "long frame";
    if (ft == 0x79) return "short frame";
    return "?";
}

void decryptMode5_AES_CBC(Telegram *t, vector<uchar> &aeskey)
{
    vector<uchar> content;
    content.insert(content.end(), t->payload.begin(), t->payload.end());
    debugPayload("(Mode5) decrypting", content);

    // The content should be a multiple of 16 since we are using AES CBC mode.
    if (content.size() % 16 != 0)
    {
        warning("(Mode5) warning: decryption received non-multiple of 16 bytes! "
                "Got %zu bytes shrinking message to %zu bytes.\n",
                content.size(), content.size() - content.size() % 16);
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
    for (int j=0; j<8; ++j) { iv[i++] = t->acc; }

    vector<uchar> ivv(iv, iv+16);
    string s = bin2hex(ivv);
    debug("(Mode5) IV %s\n", s.c_str());

    uchar content_data[content.size()];
    memcpy(content_data, &content[0], content.size());
    uchar decrypted_data[content.size()];
    AES_CBC_decrypt_buffer(decrypted_data, content_data, content.size(), &aeskey[0], iv);

    if (decrypted_data[0] != 0x2F || decrypted_data[1] != 0x2F) {
        verbose("(Mode5) decrypt failed!\n");
    }

    t->content.clear();
    t->content.insert(t->content.end(), decrypted_data, decrypted_data+content.size());
    debugPayload("(Mode5) decrypted", t->content);
}

bool loadFormatBytesFromSignature(uint16_t format_signature, vector<uchar> *format_bytes)
{
    // The format signature is used to find the proper format string.
    // But since the crc calculation is not yet functional. This functionality
    // has to wait a bit. So we hardcode the format string here.
    if (format_signature == 0xeda8)
    {
        hex2bin("02FF2004134413", format_bytes);
        // The hash of this string should equal the format signature above.
        uint16_t format_hash1 = crc16_EN13757(&(*format_bytes)[0], 7);
        uint16_t format_hash2 = ~crc16_CCITT(&(*format_bytes)[0], 7);
        debug("(utils) format signature %4X format hash a=%4X b=%4X c=%4X d=%4X\n",
              format_signature, format_hash1, (uint16_t)(~format_hash1), format_hash2, (uint16_t)(~format_hash2));
        return true;
    }
    // Unknown format signature.
    return false;
}
