// wmbusmeters WASM bindings
// wm_main() calls the real main() from main.cc.
// Serial: serial_web.cc (C++) + serial-manager.js (JS async).

#include <emscripten/emscripten.h>
#include <string>
#include <vector>
#include <cstring>
#include <cstdlib>
#include <cstdio>
#include <csetjmp>

#include "serial_web.h"

using namespace std;

// ── exit() trap ───────────────────────────────────────
static jmp_buf g_exit_jmp;
static int g_exit_code = 0;
static volatile bool g_in_wm_main = false;

extern "C" {
void wm_exit_trap(int code)
{
    g_exit_code = code;
    if (g_in_wm_main) longjmp(g_exit_jmp, 1);
}
}

// ── Result buffer ─────────────────────────────────────
static char* g_result = nullptr;

static const char* store(const string& s)
{
    free(g_result);
    g_result = (char*)malloc(s.size() + 1);
    memcpy(g_result, s.c_str(), s.size() + 1);
    return g_result;
}

// ── The real main() from main.cc ──────────────────────
extern int _wm_original_main(int argc, char** argv);

// ── Split "arg1 arg2 'arg 3'" → argc/argv ────────────
static void split(const char* cmd, int& argc,
                  vector<char*>& ptrs, vector<string>& strs)
{
    strs.clear();
    strs.push_back("wmbusmeters");
    string cur;
    bool inq = false; char qc = 0;
    for (const char* p = cmd; *p; p++) {
        char c = *p;
        if (inq) { if (c == qc) inq = false; else cur += c; }
        else if (c == '"' || c == '\'') { inq = true; qc = c; }
        else if (c == ' ' || c == '\t') { if (!cur.empty()) { strs.push_back(cur); cur.clear(); } }
        else cur += c;
    }
    if (!cur.empty()) strs.push_back(cur);
    argc = strs.size();
    ptrs.resize(argc + 1);
    for (int i = 0; i < argc; i++) ptrs[i] = &strs[i][0];
    ptrs[argc] = nullptr;
}

extern "C" {

EMSCRIPTEN_KEEPALIVE
const char* wm_version() { return "wmbusmeters-wasm-5.0"; }

EMSCRIPTEN_KEEPALIVE
void wm_free_result() { free(g_result); g_result = nullptr; }

// ── wm_main(cmdline) — calls the REAL main() ─────────
// Output goes live via Module.print (no capture needed).
// ASYNCIFY yields during emscripten_sleep() in waitForStop()
// and receive(), letting the JS read loop deliver serial data.
// Returns JSON: { "exit_code": N }
EMSCRIPTEN_KEEPALIVE
const char* wm_main(const char* cmdline)
{
    int argc;
    vector<char*> argv;
    vector<string> argv_s;
    split(cmdline ? cmdline : "", argc, argv, argv_s);

    g_exit_code = 0;
    g_in_wm_main = true;

    if (setjmp(g_exit_jmp) == 0) {
        try {
            g_exit_code = _wm_original_main(argc, argv.data());
        } catch (std::exception& e) {
            printf("Exception: %s\n", e.what());
            g_exit_code = 1;
        } catch (...) {
            printf("Unknown exception\n");
            g_exit_code = 1;
        }
    }

    g_in_wm_main = false;
    return store("{\"exit_code\":" + to_string(g_exit_code) + "}");
}

// ── wm_stop() — makes waitForStop() return ────────────
EMSCRIPTEN_KEEPALIVE
void wm_stop()
{
    if (g_web_serial_manager) g_web_serial_manager->stop();
}

// ── Serial bridge ─────────────────────────────────────
// JS opens the port + starts the read loop.
// wm_serial_open registers the device index in C++ so
// listSerialTTYs() / createSerialDeviceTTY() find it.

EMSCRIPTEN_KEEPALIVE
const char* wm_serial_open(int idx, int baud)
{
    if (!g_web_serial_manager) {
        g_web_serial_manager = new SerialCommunicationManagerImp();
    }
    g_web_serial_manager->registerWebDevice(idx);
    return store("{\"ok\":true}");
}

EMSCRIPTEN_KEEPALIVE
void wm_serial_close(int idx)
{
    if (!g_web_serial_manager) return;
    auto dev = g_web_serial_manager->findByIndex(idx);
    if (dev) dev->close();
}

} // extern "C"