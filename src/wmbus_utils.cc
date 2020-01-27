/*
 Copyright (C) 2018-2019 Fredrik Öhrström

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

bool decrypt_ELL_AES_CTR(Telegram *t, vector<uchar> &frame, vector<uchar>::iterator &pos, vector<uchar> &aeskey)
{
    if (aeskey.size() == 0) return true;

    vector<uchar> content;
    content.insert(content.end(), pos, frame.end());
    frame.erase(pos, frame.end());
    debugPayload("(ELL) decrypting", content);

    uchar iv[16];
    int i=0;
    // M-field
    iv[i++] = t->dll_mft&255; iv[i++] = t->dll_mft>>8;
    // A-field
    for (int j=0; j<6; ++j) { iv[i++] = t->dll_a[j]; }
    // CC-field
    iv[i++] = t->ell_cc;
    // SN-field
    for (int j=0; j<4; ++j) { iv[i++] = t->ell_sn_b[j]; }
    // FN
    iv[i++] = 0; iv[i++] = 0;
    // BC
    iv[i++] = 0;

    vector<uchar> ivv(iv, iv+16);
    string s = bin2hex(ivv);
    debug("(ELL) IV %s\n", s.c_str());

    uchar xordata[16];
    AES_ECB_encrypt(iv, &aeskey[0], xordata, 16);

    size_t remaining = content.size();
    if (remaining > 16) remaining = 16;

    uchar decrypt[16];
    xorit(xordata, &content[0], decrypt, remaining);

    vector<uchar> dec(decrypt, decrypt+remaining);
    debugPayload("(ELL) decrypted first block", dec);

    frame.insert(frame.end(), dec.begin(), dec.end());

    if (content.size() > 16)
    {
        remaining = content.size()-16;
        if (remaining > 16) remaining = 16; // Should not happen.

        incrementIV(iv, sizeof(iv));
        vector<uchar> ivv2(iv, iv+16);
        string s2 = bin2hex(ivv2);
        debug("(ELL) IV+1 %s\n", s2.c_str());

        AES_ECB_encrypt(iv, &aeskey[0], xordata, 16);

        xorit(xordata, &content[16], decrypt, remaining);

        vector<uchar> dec2(decrypt, decrypt+remaining);
        debugPayload("(ELL) decrypted second block", dec2);

        // Append the second decrypted block to the first.
        dec.clear();
        dec.insert(dec.end(), dec2.begin(), dec2.end());
        frame.insert(frame.end(), dec.begin(), dec.end());
    }
    debugPayload("(ELL) decrypted", content);
    return true;
}

string frameTypeKamstrupC1(int ft) {
    if (ft == 0x78) return "long frame";
    if (ft == 0x79) return "short frame";
    return "?";
}

bool decrypt_TPL_AES_CBC_IV(Telegram *t, vector<uchar> &frame, vector<uchar>::iterator &pos, vector<uchar> &aeskey)
{
    if (aeskey.size() == 0) return true;

    vector<uchar> buffer;
    buffer.insert(buffer.end(), pos, frame.end());
    frame.erase(pos, frame.end());
    debugPayload("(TPL) decrypting", buffer);

    // The content should be a multiple of 16 since we are using AES CBC mode.
    if (buffer.size() % 16 != 0)
    {
        warning("(TPL) warning: decryption received non-multiple of 16 bytes! "
                "Got %zu bytes shrinking message to %zu bytes.\n",
                buffer.size(), buffer.size() - buffer.size() % 16);
        while (buffer.size() % 16 != 0)
        {
            buffer.pop_back();
        }
    }

    uchar iv[16];
    int i=0;
    // M-field
    iv[i++] = t->dll_mft&255; iv[i++] = t->dll_mft>>8;
    // A-field
    for (int j=0; j<6; ++j) { iv[i++] = t->dll_a[j]; }
    // ACC
    for (int j=0; j<8; ++j) { iv[i++] = t->tpl_acc; }

    vector<uchar> ivv(iv, iv+16);
    string s = bin2hex(ivv);
    debug("(TPL) IV %s\n", s.c_str());

    uchar buffer_data[buffer.size()];
    memcpy(buffer_data, &buffer[0], buffer.size());
    uchar decrypted_data[buffer.size()];
    AES_CBC_decrypt_buffer(decrypted_data, buffer_data, buffer.size(), &aeskey[0], iv);

    frame.insert(frame.end(), decrypted_data, decrypted_data+buffer.size());
    debugPayload("(TPL) decrypted", frame, pos);
    return true;
}
