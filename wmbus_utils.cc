// Copyright (c) 2018 Fredrik Öhrström
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#ifndef WMBUS_UTILS_H
#define WMBUS_UTILS_H

#include"aes.h"
#include"wmbus.h"

void decryptKamstrupC1(Telegram *t, vector<uchar> &aeskey)
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
    debug("(multical21) IV   %s\n", s.c_str());

    uchar xordata[16];
    AES_ECB_encrypt(iv, &aeskey[0], xordata, 16);

    uchar decrypt[16];
    xorit(xordata, &content[0], decrypt, remaining);

    vector<uchar> dec(decrypt, decrypt+remaining);
    debugPayload("(multical21) decrypted", dec);

    if (content.size() > 22) {
        warning("(multical21) warning: Received too many bytes of content! "
                "Got %zu bytes, expected at most 22.\n", content.size());
    }
    if (content.size() > 16) {
        // Yay! Lets decrypt a second block. Full frame content is 22 bytes.
        // So a second block should enough for everyone!
        remaining = content.size()-16;
        if (remaining > 16) remaining = 16; // Should not happen.

        incrementIV(iv, sizeof(iv));
        vector<uchar> ivv2(iv, iv+16);
        string s2 = bin2hex(ivv2);
        debug("(multical21) IV+1 %s\n", s2.c_str());

        AES_ECB_encrypt(iv, &aeskey[0], xordata, 16);

        xorit(xordata, &content[16], decrypt, remaining);

        vector<uchar> dec2(decrypt, decrypt+remaining);
        debugPayload("(multical21) decrypted", dec2);

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

#endif
