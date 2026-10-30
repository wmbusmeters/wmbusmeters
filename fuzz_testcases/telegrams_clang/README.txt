Seed / regression corpus for the clang + libFuzzer telegram harness
(src/fuzz_telegram.cc, built with "make build_fuzz_clang").

Run:  build_debug/fuzztelegram fuzz_testcases/telegrams_clang

These are the minimized inputs from an Oct 2026 disclosure thread. On
current master F1, F4a, F4b, F5 and F6 parse cleanly (the fixes hold).
F2 and F3 still trip UBSan in difvif decoding (hardening): the signed
left-shift at dvparser.cc:1815 and the BCD *10 overflow at :2281. A
plain replay stops there; add -fno-sanitize=shift,signed-integer-overflow
to DEBUG_FLAGS to run past them.
