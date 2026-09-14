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

#include"always.h"
#include"log.h"
#include"crypto/aes.h"
#include"crypto/des.h"
#include"util.h"
#include"wmbus.h"

#include"crypto/aes.h"

#include<assert.h>
#include<memory.h>

using namespace std;

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

// Security mode 10, OMS security profile D. AES-CCM in counter mode with the
// ephemeral key generated in parseTPLConfig (DC=00, Kenc). The 13 byte nonce
// holds M(2) ID(4) Ver(1) Type(1) 00h MC(4). The authentication tag is a
// suffix at the end of the telegram and is not verified here.
bool decrypt_TPL_AES_CCM(Telegram *t, vector<uchar> &frame, vector<uchar>::iterator &pos, vector<uchar> &aeskey,
                         int *num_encrypted_bytes,
                         int *num_not_encrypted_at_end)
{
    if (aeskey.size() == 0) return true;

    vector<uchar> buffer;
    buffer.insert(buffer.end(), pos, frame.end());

    size_t num_bytes_to_decrypt = buffer.size();

    size_t tag_size = t->tpl_ccm_tag_size;
    if (buffer.size() < tag_size)
    {
        warning("(TPL) warning: aes-ccm telegram received less bytes than expected for decryption! "
                "Got %zu bytes but expected at least %zu bytes since the tag size is %d.\n",
                buffer.size(), tag_size, t->tpl_ccm_tag_size);
        return false;
    }
    num_bytes_to_decrypt -= tag_size;

    // The cfg field bits 7-0 (N) hold the number of encrypted bytes,
    // 0xff means that partial encryption is disabled, all bytes are encrypted.
    int num_encrypted_cfg = t->tpl_cfg & 0xff;
    if (num_encrypted_cfg != 0xff && num_encrypted_cfg < (int)num_bytes_to_decrypt)
    {
        num_bytes_to_decrypt = num_encrypted_cfg;
    }

    *num_encrypted_bytes = num_bytes_to_decrypt;
    *num_not_encrypted_at_end = buffer.size()-num_bytes_to_decrypt;

    debug("(TPL) num encrypted bytes %zu and remaining unencrypted %zu bytes (tag %d bytes)\n",
          num_bytes_to_decrypt, buffer.size()-num_bytes_to_decrypt, t->tpl_ccm_tag_size);

    // The 13 byte nonce holds the address fields from the tpl header,
    // if present, else the address fields from the dll header.
    uchar *mfct_b = t->dll_mfct_b;
    uchar *id_b = t->dll_id_b;
    uchar version = t->dll_version;
    uchar type = t->dll_type;
    if (t->tpl_id_found)
    {
        mfct_b = t->tpl_mfct_b;
        id_b = t->tpl_id_b;
        version = t->tpl_version;
        type = t->tpl_type;
    }

    uchar nonce[13];
    int i=0;
    // M-field
    nonce[i++] = mfct_b[0]; nonce[i++] = mfct_b[1];
    // A-field
    nonce[i++] = id_b[0]; nonce[i++] = id_b[1];
    nonce[i++] = id_b[2]; nonce[i++] = id_b[3];
    // Version and type.
    nonce[i++] = version; nonce[i++] = type;
    // Separator.
    nonce[i++] = 0x00;
    // The message counter is sent little endian but enters the nonce big endian.
    nonce[i++] = t->tpl_counter_b[3];
    nonce[i++] = t->tpl_counter_b[2];
    nonce[i++] = t->tpl_counter_b[1];
    nonce[i++] = t->tpl_counter_b[0];

    vector<uchar> noncev(nonce, nonce+13);
    string s = bin2hex(noncev);
    debug("(TPL) nonce %s\n", s.c_str());

    // Remove the encrypted bytes, any potentially not encrypted bytes and the tag.
    frame.erase(pos, frame.end());

    if (num_bytes_to_decrypt > 0)
    {
        uchar buffer_data[num_bytes_to_decrypt];
        memcpy(buffer_data, safeButUnsafeVectorPtr(buffer), num_bytes_to_decrypt);
        uchar decrypted_data[num_bytes_to_decrypt];

        // AES-CCM encrypts in counter mode: the keystream block for counter i
        // (starting at 1) is E(key, 0x01 || nonce || i).
        size_t num_full_blocks = (num_bytes_to_decrypt+15)/16;
        uchar keystream[num_full_blocks*16];
        for (size_t b = 0; b < num_full_blocks; ++b)
        {
            uchar a[16];
            a[0] = 0x01;
            memcpy(a+1, nonce, 13);
            a[14] = ((b+1) >> 8) & 0xff;
            a[15] = (b+1) & 0xff;
            AES_ECB_encrypt(a, safeButUnsafeVectorPtr(aeskey), keystream+b*16, 16);
        }

        xorit(keystream, buffer_data, decrypted_data, num_bytes_to_decrypt);

        // Insert the decrypted bytes.
        frame.insert(frame.end(), decrypted_data, decrypted_data+num_bytes_to_decrypt);
    }

    debugPayload("(TPL) decrypted ", frame, pos);

    // Append any potentially not encrypted bytes and the authentication tag.
    if (num_bytes_to_decrypt < buffer.size())
    {
        frame.insert(frame.end(), buffer.begin()+num_bytes_to_decrypt, buffer.end());
        debugPayload("(TPL) appended  ", frame, pos);
    }

    // The authentication tag is a suffix after the APL content,
    // record its size so that the dvparser does not try to parse it.
    t->suffix_size = tag_size;

    vector<uchar>::iterator tagpos = frame.end()-tag_size;
    vector<uchar> tagv(tagpos, frame.end());
    string tags = bin2hex(tagv);
    t->addExplanationAndIncrementPos(tagpos, tag_size, KindOfData::PROTOCOL, Understanding::FULL,
                                     "%s aes-ccm-tag", tags.c_str());

    return true;
}

bool decrypt_TPL_DES_CBC(Telegram *t, vector<uchar> &frame, vector<uchar>::iterator &pos,
                         vector<uchar> &deskey, const uchar *iv8,
                         int *num_encrypted_bytes,
                         int *num_not_encrypted_at_end)
{
    if (deskey.size() < 8) return false;

    vector<uchar> buffer;
    buffer.insert(buffer.end(), pos, frame.end());

    // EN 13757-7:2018 Table 29: tpl_num_encr_blocks holds byte count for DES modes.
    size_t claimed = (t->tpl_num_encr_blocks > 0)
                     ? (size_t)t->tpl_num_encr_blocks
                     : buffer.size();
    size_t num_bytes_to_decrypt = (min(claimed, buffer.size()) / 8) * 8;

    *num_encrypted_bytes      = (int)num_bytes_to_decrypt;
    *num_not_encrypted_at_end = (int)(buffer.size() - num_bytes_to_decrypt);

    if (num_bytes_to_decrypt == 0) return false;

    vector<uchar> ivv(iv8, iv8 + 8);
    debugPayload("(TPL) DES CBC IV", ivv);
    debugPayload("(TPL) DES CBC decrypting", buffer);

    uchar decrypted[num_bytes_to_decrypt];
    bool ok = DES_CBC_decrypt(buffer.data(), deskey.data(), iv8, decrypted, num_bytes_to_decrypt);
    if (!ok)
    {
        warning("(TPL) DES decryption failed.\n");
        return false;
    }

    frame.erase(pos, frame.end());
    frame.insert(frame.end(), decrypted, decrypted + num_bytes_to_decrypt);

    debugPayload("(TPL) DES decrypted", frame, pos);

    if (*num_not_encrypted_at_end > 0)
        frame.insert(frame.end(), buffer.begin() + num_bytes_to_decrypt, buffer.end());

    return true;
}
