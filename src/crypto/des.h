// This file is in the public domain.
// DES decryption
// EN 13757-7:2018 Table 47: security modes 2 and 3 use DES-CBC.
//   Mode 2 (DES_NO_IV_DEPRECATED): DES-CBC, IV = 0
//   Mode 3 (DES_IV_DEPRECATED):    DES-CBC, IV = ID[4] + mfct[2] + date-G[2]
#ifndef DES_H
#define DES_H

#include <stddef.h>
#include <stdint.h>

// Decrypt `length` bytes using DES-CBC with the given 8-byte IV.
// Pass iv = all-zeros for mode 2 (DES_NO_IV_DEPRECATED).
bool DES_CBC_decrypt(const uint8_t *input, const uint8_t *key, const uint8_t *iv,
                     uint8_t *output, const size_t length);

#endif // DES_H
