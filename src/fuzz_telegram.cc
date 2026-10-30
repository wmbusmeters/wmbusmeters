// Copyright (C) 2026 alosafuzz (gpl-3.0-or-later)
//
// Clang + libFuzzer in-process harness over the whole telegram decode path
// (DLL/ELL/NWL/AFL/TPL framing, AES decrypt, difvif), built with
// AddressSanitizer + UndefinedBehaviorSanitizer.
//
// The shipped src/fuzz.cc drives parseDV (difvif) only; the rest of the
// telegram path has just the AFL_HARDEN full-binary run (run_fuzz_telegrams),
// which is not ASan/UBSan and treats a call to exit() as a clean exit. This
// harness calls Telegram::parse directly so those are visible.
//
//   make build_fuzz_clang
//   build_debug/fuzztelegram fuzz_testcases/telegrams_clang

#include"wmbus.h"
#include"meters.h"

#include<stddef.h>
#include<stdint.h>
#include<vector>

using namespace std;

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    if (size == 0 || size > 4096) return 0;

    vector<uchar> frame(data, data + size);

    // A fixed test key so the AES decrypt path is reachable from the input.
    MeterKeys mk;
    static const uchar key[16] = {
        0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77,
        0x88, 0x99, 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff
    };
    mk.confidentiality_key.assign(key, key + 16);

    Telegram t;
    t.parse(frame, &mk, false); // warn=false: no noisy logging
    return 0;
}
