/*
 Copyright (C) 2018-2024 Fredrik Öhrström (gpl-3.0-or-later)

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

#include<assert.h>
#include<memory.h>

bool decrypt_ELL_AES_CTR(Telegram *t, vector<uchar> &frame, vector<uchar>::iterator &pos, vector<uchar> &aeskey)
{
    if (aeskey.size() == 0) return true;

    vector<uchar> encrypted_bytes;
    vector<uchar> decrypted_bytes;
    encrypted_bytes.insert(encrypted_bytes.end(), pos, frame.end());
    debugPayload("(ELL) decrypting", encrypted_bytes);

    uchar iv[16];
    int i=0;
    // M-field
    iv[i++] = t->dll_mfct_b[0]; iv[i++] = t->dll_mfct_b[1];
    // A-field
    for (int j=0; j<6; ++j) { iv[i++] = t->dll_a[j]; }
    // CC-field
    // Two bits should be zeroed out:
    // 0x10 H-field Hop-count set when telegram is repeated
    // 0x02 R-field Repeated access field
    iv[i++] = t->ell_cc & ~(0x10) & ~(0x02);
    // SN-field
    for (int j=0; j<4; ++j) { iv[i++] = t->ell_sn_b[j]; }
    // FN
    iv[i++] = 0; iv[i++] = 0;
    // BC
    iv[i++] = 0;

    vector<uchar> ivv(iv, iv+16);
    string s = bin2hex(ivv);
    debug("(ELL) IV %s\n", s.c_str());

    int block = 0;
    for (size_t offset = 0; offset < encrypted_bytes.size(); offset += 16)
    {
        size_t block_size = 16;
        if (offset + block_size > encrypted_bytes.size())
        {
            block_size = encrypted_bytes.size() - offset;
        }

        assert(block_size > 0 && block_size <= 16);

        // Generate the pseudo-random bits from the IV and the key.
        uchar xordata[16];
        AES_ECB_encrypt(iv, safeButUnsafeVectorPtr(aeskey), xordata, 16);

        // Xor the data with the pseudo-random bits to decrypt into tmp.
        uchar tmp[block_size];
        xorit(xordata, &encrypted_bytes[offset], tmp, block_size);

        debug("(ELL) block %d block_size %d offset %zu\n", block, block_size, offset);
        block++;

        vector<uchar> tmpv(tmp, tmp+block_size);
        debugPayload("(ELL) decrypted", tmpv);

        decrypted_bytes.insert(decrypted_bytes.end(), tmpv.begin(), tmpv.end());

        incrementIV(iv, sizeof(iv));
    }
    debugPayload("(ELL) decrypted", decrypted_bytes);

    // Remove the encrypted bytes.
    frame.erase(pos, frame.end());
    // Insert the decrypted bytes.
    frame.insert(frame.end(), decrypted_bytes.begin(), decrypted_bytes.end());

    return true;
}

string frameTypeKamstrupC1(int ft) {
    if (ft == 0x78) return "long frame";
    if (ft == 0x79) return "short frame";
    return "?";
}

bool decrypt_TPL_AES_CBC_IV(Telegram *t,
                            vector<uchar> &frame,
                            vector<uchar>::iterator &pos,
                            vector<uchar> &aeskey,
                            int *num_encrypted_bytes,
                            int *num_not_encrypted_at_end)
{
    vector<uchar> buffer;
    buffer.insert(buffer.end(), pos, frame.end());

    size_t num_bytes_to_decrypt = frame.end()-pos;

    if (t->tpl_num_encr_blocks)
    {
        num_bytes_to_decrypt = t->tpl_num_encr_blocks*16;
    }

    *num_encrypted_bytes = num_bytes_to_decrypt;

    if (buffer.size() < num_bytes_to_decrypt)
    {
        warning("(TPL) warning: aes-cbc-iv decryption received less bytes than expected for decryption! "
                "Got %zu bytes but expected at least %zu bytes since num encr blocks was %d.\n",
                buffer.size(), num_bytes_to_decrypt,
                t->tpl_num_encr_blocks);
        num_bytes_to_decrypt = buffer.size();
        *num_encrypted_bytes = num_bytes_to_decrypt;

        // We must have at least 16 bytes to decrypt. Give up otherwise.
        if (num_bytes_to_decrypt < 16) return false;
    }

    *num_not_encrypted_at_end = buffer.size()-num_bytes_to_decrypt;

    debug("(TPL) num encrypted blocks %zu (%d bytes and remaining unencrypted %zu bytes)\n",
          t->tpl_num_encr_blocks, num_bytes_to_decrypt, buffer.size()-num_bytes_to_decrypt);

    if (aeskey.size() == 0) return false;

    debugPayload("(TPL) AES CBC IV decrypting", buffer);

    // The content should be a multiple of 16 since we are using AES CBC mode.
    if (num_bytes_to_decrypt % 16 != 0)
    {
        warning("(TPL) warning: decryption received non-multiple of 16 bytes! "
                "Got %zu bytes shrinking message to %zu bytes.\n",
                num_bytes_to_decrypt, num_bytes_to_decrypt - num_bytes_to_decrypt % 16);
        num_bytes_to_decrypt -= num_bytes_to_decrypt % 16;
        *num_encrypted_bytes = num_bytes_to_decrypt;
        assert (num_bytes_to_decrypt % 16 == 0);
        // There must be at least 16 bytes remaining.
        if (num_bytes_to_decrypt < 16) return false;
    }

    uchar iv[16];
    int i=0;
    // If there is a tpl_id, then use it, else use ddl_id.
    if (t->tpl_id_found)
    {
        // M-field
        iv[i++] = t->tpl_mfct_b[0]; iv[i++] = t->tpl_mfct_b[1];

        // A-field
        for (int j=0; j<6; ++j) { iv[i++] = t->tpl_a[j]; }
    }
    else
    {
        // M-field
        iv[i++] = t->dll_mfct_b[0]; iv[i++] = t->dll_mfct_b[1];

        // A-field
        for (int j=0; j<6; ++j) { iv[i++] = t->dll_a[j]; }
    }

    // ACC
    for (int j=0; j<8; ++j) { iv[i++] = t->tpl_acc; }

    vector<uchar> ivv(iv, iv+16);
    string s = bin2hex(ivv);
    debug("(TPL) IV %s\n", s.c_str());

    uchar buffer_data[num_bytes_to_decrypt];
    memcpy(buffer_data, safeButUnsafeVectorPtr(buffer), num_bytes_to_decrypt);
    uchar decrypted_data[num_bytes_to_decrypt];

    AES_CBC_decrypt_buffer(decrypted_data, buffer_data, num_bytes_to_decrypt, safeButUnsafeVectorPtr(aeskey), iv);

    // Remove the encrypted bytes.
    frame.erase(pos, frame.end());

    // Insert the decrypted bytes.
    frame.insert(frame.end(), decrypted_data, decrypted_data+num_bytes_to_decrypt);

    debugPayload("(TPL) decrypted ", frame, pos);

    if (num_bytes_to_decrypt < buffer.size())
    {
        frame.insert(frame.end(), buffer.begin()+num_bytes_to_decrypt, buffer.end());
        debugPayload("(TPL) appended  ", frame, pos);
    }
    return true;
}

bool decrypt_TPL_AES_CBC_NO_IV(Telegram *t, vector<uchar> &frame, vector<uchar>::iterator &pos, vector<uchar> &aeskey,
                               int *num_encrypted_bytes,
                               int *num_not_encrypted_at_end)
{
    if (aeskey.size() == 0) return true;

    vector<uchar> buffer;
    buffer.insert(buffer.end(), pos, frame.end());

    size_t num_bytes_to_decrypt = buffer.size();

    if (t->tpl_num_encr_blocks)
    {
        num_bytes_to_decrypt = t->tpl_num_encr_blocks*16;
    }

    *num_encrypted_bytes = num_bytes_to_decrypt;
    if (buffer.size() < num_bytes_to_decrypt)
    {
        warning("(TPL) warning: aes-cbc-no-iv decryption received less bytes than expected for decryption! "
                "Got %zu bytes but expected at least %zu bytes since num encr blocks was %d.\n",
                buffer.size(), num_bytes_to_decrypt,
                t->tpl_num_encr_blocks);
        num_bytes_to_decrypt = buffer.size();
    }

    *num_not_encrypted_at_end = buffer.size()-num_bytes_to_decrypt;

    debug("(TPL) num encrypted blocks %d (%d bytes and remaining unencrypted %d bytes)\n",
          t->tpl_num_encr_blocks, num_bytes_to_decrypt, buffer.size()-num_bytes_to_decrypt);

    if (aeskey.size() == 0) return false;

    // The content should be a multiple of 16 since we are using AES CBC mode.
    if (num_bytes_to_decrypt % 16 != 0)
    {
        warning("(TPL) warning: decryption received non-multiple of 16 bytes! "
                "Got %zu bytes shrinking message to %zu bytes.\n",
                num_bytes_to_decrypt, num_bytes_to_decrypt - num_bytes_to_decrypt % 16);
        num_bytes_to_decrypt -= num_bytes_to_decrypt % 16;
        assert (num_bytes_to_decrypt % 16 == 0);
    }

    uchar iv[16];
    memset(iv, 0, sizeof(iv));

    vector<uchar> ivv(iv, iv+16);
    string s = bin2hex(ivv);
    debug("(TPL) IV %s\n", s.c_str());

    uchar buffer_data[num_bytes_to_decrypt];
    memcpy(buffer_data, safeButUnsafeVectorPtr(buffer), num_bytes_to_decrypt);
    uchar decrypted_data[num_bytes_to_decrypt];

    AES_CBC_decrypt_buffer(decrypted_data, buffer_data, num_bytes_to_decrypt, safeButUnsafeVectorPtr(aeskey), iv);

    // Remove the encrypted bytes and any potentially not decryptes bytes after.
    frame.erase(pos, frame.end());

    // Insert the decrypted bytes.
    frame.insert(frame.end(), decrypted_data, decrypted_data+num_bytes_to_decrypt);

    debugPayload("(TPL) decrypted ", frame, pos);

    if (num_bytes_to_decrypt < buffer.size())
    {
        frame.insert(frame.end(), buffer.begin()+num_bytes_to_decrypt, buffer.end());
        debugPayload("(TPL) appended ", frame, pos);
    }

    return true;
}
