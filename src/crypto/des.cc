// This file is in the public domain.
// DES-CBC implementation
// Standard FIPS 46-3 Data Encryption Standard, CBC mode only.
// Sufficient for OMS TPL security mode 2 and 3.

#include "des.h"
#include <string.h>

// ---------------------------------------------------------------------------
// Standard DES tables (all 1-indexed, MSB first)
// ---------------------------------------------------------------------------

static const uint8_t IP[64] = {
    58, 50, 42, 34, 26, 18, 10,  2,
    60, 52, 44, 36, 28, 20, 12,  4,
    62, 54, 46, 38, 30, 22, 14,  6,
    64, 56, 48, 40, 32, 24, 16,  8,
    57, 49, 41, 33, 25, 17,  9,  1,
    59, 51, 43, 35, 27, 19, 11,  3,
    61, 53, 45, 37, 29, 21, 13,  5,
    63, 55, 47, 39, 31, 23, 15,  7
};

static const uint8_t FP[64] = {
    40,  8, 48, 16, 56, 24, 64, 32,
    39,  7, 47, 15, 55, 23, 63, 31,
    38,  6, 46, 14, 54, 22, 62, 30,
    37,  5, 45, 13, 53, 21, 61, 29,
    36,  4, 44, 12, 52, 20, 60, 28,
    35,  3, 43, 11, 51, 19, 59, 27,
    34,  2, 42, 10, 50, 18, 58, 26,
    33,  1, 41,  9, 49, 17, 57, 25
};

// Expansion: 32 → 48 bits
static const uint8_t E[48] = {
    32,  1,  2,  3,  4,  5,
     4,  5,  6,  7,  8,  9,
     8,  9, 10, 11, 12, 13,
    12, 13, 14, 15, 16, 17,
    16, 17, 18, 19, 20, 21,
    20, 21, 22, 23, 24, 25,
    24, 25, 26, 27, 28, 29,
    28, 29, 30, 31, 32,  1
};

// Permuted Choice 1: 64 → 56 bits (strips parity bits)
static const uint8_t PC1[56] = {
    57, 49, 41, 33, 25, 17,  9,
     1, 58, 50, 42, 34, 26, 18,
    10,  2, 59, 51, 43, 35, 27,
    19, 11,  3, 60, 52, 44, 36,
    63, 55, 47, 39, 31, 23, 15,
     7, 62, 54, 46, 38, 30, 22,
    14,  6, 61, 53, 45, 37, 29,
    21, 13,  5, 28, 20, 12,  4
};

// Permuted Choice 2: 56 → 48 bits
static const uint8_t PC2[48] = {
    14, 17, 11, 24,  1,  5,
     3, 28, 15,  6, 21, 10,
    23, 19, 12,  4, 26,  8,
    16,  7, 27, 20, 13,  2,
    41, 52, 31, 37, 47, 55,
    30, 40, 51, 45, 33, 48,
    44, 49, 39, 56, 34, 53,
    46, 42, 50, 36, 29, 32
};

// P permutation: 32 → 32 bits (after S-boxes)
static const uint8_t P[32] = {
    16,  7, 20, 21, 29, 12, 28, 17,
     1, 15, 23, 26,  5, 18, 31, 10,
     2,  8, 24, 14, 32, 27,  3,  9,
    19, 13, 30,  6, 22, 11,  4, 25
};

// Left rotation counts per round
static const uint8_t ROT[16] = {
    1, 1, 2, 2, 2, 2, 2, 2,
    1, 2, 2, 2, 2, 2, 2, 1
};

// S-boxes S1..S8, each [4 rows][16 columns]
static const uint8_t SBOX[8][4][16] = {
    { // S1
        {14,  4, 13,  1,  2, 15, 11,  8,  3, 10,  6, 12,  5,  9,  0,  7},
        { 0, 15,  7,  4, 14,  2, 13,  1, 10,  6, 12, 11,  9,  5,  3,  8},
        { 4,  1, 14,  8, 13,  6,  2, 11, 15, 12,  9,  7,  3, 10,  5,  0},
        {15, 12,  8,  2,  4,  9,  1,  7,  5, 11,  3, 14, 10,  0,  6, 13}
    },
    { // S2
        {15,  1,  8, 14,  6, 11,  3,  4,  9,  7,  2, 13, 12,  0,  5, 10},
        { 3, 13,  4,  7, 15,  2,  8, 14, 12,  0,  1, 10,  6,  9, 11,  5},
        { 0, 14,  7, 11, 10,  4, 13,  1,  5,  8, 12,  6,  9,  3,  2, 15},
        {13,  8, 10,  1,  3, 15,  4,  2, 11,  6,  7, 12,  0,  5, 14,  9}
    },
    { // S3
        {10,  0,  9, 14,  6,  3, 15,  5,  1, 13, 12,  7, 11,  4,  2,  8},
        {13,  7,  0,  9,  3,  4,  6, 10,  2,  8,  5, 14, 12, 11, 15,  1},
        {13,  6,  4,  9,  8, 15,  3,  0, 11,  1,  2, 12,  5, 10, 14,  7},
        { 1, 10, 13,  0,  6,  9,  8,  7,  4, 15, 14,  3, 11,  5,  2, 12}
    },
    { // S4
        { 7, 13, 14,  3,  0,  6,  9, 10,  1,  2,  8,  5, 11, 12,  4, 15},
        {13,  8, 11,  5,  6, 15,  0,  3,  4,  7,  2, 12,  1, 10, 14,  9},
        {10,  6,  9,  0, 12, 11,  7, 13, 15,  1,  3, 14,  5,  2,  8,  4},
        { 3, 15,  0,  6, 10,  1, 13,  8,  9,  4,  5, 11, 12,  7,  2, 14}
    },
    { // S5
        { 2, 12,  4,  1,  7, 10, 11,  6,  8,  5,  3, 15, 13,  0, 14,  9},
        {14, 11,  2, 12,  4,  7, 13,  1,  5,  0, 15, 10,  3,  9,  8,  6},
        { 4,  2,  1, 11, 10, 13,  7,  8, 15,  9, 12,  5,  6,  3,  0, 14},
        {11,  8, 12,  7,  1, 14,  2, 13,  6, 15,  0,  9, 10,  4,  5,  3}
    },
    { // S6
        {12,  1, 10, 15,  9,  2,  6,  8,  0, 13,  3,  4, 14,  7,  5, 11},
        {10, 15,  4,  2,  7, 12,  9,  5,  6,  1, 13, 14,  0, 11,  3,  8},
        { 9, 14, 15,  5,  2,  8, 12,  3,  7,  0,  4, 10,  1, 13, 11,  6},
        { 4,  3,  2, 12,  9,  5, 15, 10, 11, 14,  1,  7,  6,  0,  8, 13}
    },
    { // S7
        { 4, 11,  2, 14, 15,  0,  8, 13,  3, 12,  9,  7,  5, 10,  6,  1},
        {13,  0, 11,  7,  4,  9,  1, 10, 14,  3,  5, 12,  2, 15,  8,  6},
        { 1,  4, 11, 13, 12,  3,  7, 14, 10, 15,  6,  8,  0,  5,  9,  2},
        { 6, 11, 13,  8,  1,  4, 10,  7,  9,  5,  0, 15, 14,  2,  3, 12}
    },
    { // S8
        {13,  2,  8,  4,  6, 15, 11,  1, 10,  9,  3, 14,  5,  0, 12,  7},
        { 1, 15, 13,  8, 10,  3,  7,  4, 12,  5,  6, 11,  0, 14,  9,  2},
        { 7, 11,  4,  1,  9, 12, 14,  2,  0,  6, 10, 13, 15,  3,  5,  8},
        { 2,  1, 14,  7,  4, 10,  8, 13, 15, 12,  9,  0,  3,  5,  6, 11}
    }
};

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

// Extract bit `bit` (1-indexed, MSB first) from a value with `width` bits.
static inline int getbit(uint64_t val, int width, int bit)
{
    return (val >> (width - bit)) & 1;
}

// Permute `src` (with `src_width` bits) through `table` producing `dst_width` bits.
static uint64_t permute(uint64_t src, int src_width,
                        const uint8_t *table, int dst_width)
{
    uint64_t dst = 0;
    for (int i = 0; i < dst_width; i++)
        dst |= (uint64_t)getbit(src, src_width, table[i]) << (dst_width - 1 - i);
    return dst;
}

// Rotate left within a 28-bit half-key.
static inline uint32_t rot28(uint32_t v, int n)
{
    return ((v << n) | (v >> (28 - n))) & 0x0FFFFFFF;
}

// ---------------------------------------------------------------------------
// Key schedule
// ---------------------------------------------------------------------------

static void des_key_schedule(const uint8_t key[8], uint64_t subkeys[16])
{
    uint64_t k64 = 0;
    for (int i = 0; i < 8; i++)
        k64 = (k64 << 8) | key[i];

    uint64_t kp = permute(k64, 64, PC1, 56);
    uint32_t C  = (uint32_t)(kp >> 28) & 0x0FFFFFFF;
    uint32_t D  = (uint32_t)(kp)       & 0x0FFFFFFF;

    for (int r = 0; r < 16; r++)
    {
        C = rot28(C, ROT[r]);
        D = rot28(D, ROT[r]);
        uint64_t CD = ((uint64_t)C << 28) | D;
        subkeys[r] = permute(CD, 56, PC2, 48);
    }
}

// ---------------------------------------------------------------------------
// Feistel F function
// ---------------------------------------------------------------------------

static uint32_t des_f(uint32_t R, uint64_t subkey)
{
    // Expand R: 32 → 48 bits
    uint64_t ER = permute((uint64_t)R, 32, E, 48);

    // XOR with round subkey
    ER ^= subkey;

    // S-box substitution: 48 → 32 bits (8 groups of 6 bits → 8 groups of 4 bits)
    uint32_t sout = 0;
    for (int i = 0; i < 8; i++)
    {
        uint8_t six = (ER >> (42 - i * 6)) & 0x3F;
        uint8_t row = ((six >> 4) & 2) | (six & 1);
        uint8_t col = (six >> 1) & 0xF;
        sout = (sout << 4) | SBOX[i][row][col];
    }

    // P permutation: 32 → 32 bits
    return (uint32_t)permute((uint64_t)sout, 32, P, 32);
}

// ---------------------------------------------------------------------------
// Single block: decrypt one 8-byte block in CBC (DES_NO_IV) mode
// ---------------------------------------------------------------------------

static void des_block_decrypt(const uint8_t in[8], uint8_t out[8],
                              const uint64_t subkeys[16])
{
    // Load block as big-endian 64-bit integer
    uint64_t block = 0;
    for (int i = 0; i < 8; i++)
        block = (block << 8) | in[i];

    // Initial permutation
    block = permute(block, 64, IP, 64);

    uint32_t L = (uint32_t)(block >> 32);
    uint32_t R = (uint32_t)(block);

    // 16 Feistel rounds in reverse order for decryption.
    // After each step: L = R_{r-1}, R = L_{r-1}.
    for (int r = 15; r >= 0; r--)
    {
        uint32_t newR = L ^ des_f(R, subkeys[r]);
        L = R;
        R = newR;
    }

    // After 16 steps: L = R0, R = L0.  Reconstruct L0 || R0 before FP.
    uint64_t pre    = ((uint64_t)R << 32) | L;
    uint64_t result = permute(pre, 64, FP, 64);

    // Store result as big-endian bytes
    for (int i = 7; i >= 0; i--)
    {
        out[i] = (uint8_t)(result & 0xFF);
        result >>= 8;
    }
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

bool DES_CBC_decrypt(const uint8_t *input, const uint8_t *key, const uint8_t *iv,
                     uint8_t *output, const size_t length)
{
    if ((length & 7) != 0)
    {
        return false;
    }
    uint64_t subkeys[16];
    des_key_schedule(key, subkeys);

    uint8_t prev[8];
    memcpy(prev, iv, 8);

    for (size_t offset = 0; offset + 8 <= length; offset += 8)
    {
        uint8_t block[8];
        des_block_decrypt(input + offset, block, subkeys);
        for (int i = 0; i < 8; i++)
            output[offset + i] = block[i] ^ prev[i];
        memcpy(prev, input + offset, 8);
    }
    return true;
}
