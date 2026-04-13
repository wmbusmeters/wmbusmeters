#include <emscripten/emscripten.h>
#include <string>
#include <vector>
#include <cstdio>
#include <csetjmp>
#include <exception>

#include "version.h"
#include "serial_web.h"

// ── Global state ───────
namespace {
    jmp_buf g_exit_jmp;
    int g_exit_code = 0;
    bool g_in_wm_main = false;

    std::string g_result;
}

// ── exit() trap ──────────────────────────────────────
extern "C" {
void wm_exit_trap(int code)
{
    g_exit_code = code;
    if (g_in_wm_main) {
        // WARNING: longjmp skips destructors!
        longjmp(g_exit_jmp, 1);
    }
}
}

// ── Helper: store result safely ──────────────────────
static const char* store(std::string s)
{
    g_result = std::move(s);
    return g_result.c_str();
}

// ── Real main() ──────────────────────────────────────
extern int _wm_original_main(int argc, char** argv);

// ── Argument splitting (safe + modern) ───────────────
static void split(const char* cmd,
                  int& argc,
                  std::vector<char*>& argv,
                  std::vector<std::string>& storage)
{
    storage.clear();
    storage.emplace_back("wmbusmeters");

    std::string current;
    bool in_quotes = false;
    char quote_char = 0;

    for (const char* p = cmd; *p; ++p) {
        char c = *p;

        if (in_quotes) {
            if (c == quote_char) {
                in_quotes = false;
            } else {
                current += c;
            }
        }
        else if (c == '"' || c == '\'') {
            in_quotes = true;
            quote_char = c;
        }
        else if (c == ' ' || c == '\t') {
            if (!current.empty()) {
                storage.push_back(std::move(current));
                current.clear();
            }
        }
        else {
            current += c;
        }
    }

    if (!current.empty()) {
        storage.push_back(std::move(current));
    }

    argc = static_cast<int>(storage.size());

    argv.resize(argc + 1);
    for (int i = 0; i < argc; ++i) {
        argv[i] = storage[i].data(); // safer than &str[0]
    }
    argv[argc] = nullptr;
}

// ── Public API ───────────────────────────────────────
extern "C" {

EMSCRIPTEN_KEEPALIVE
const char* wm_version()
{
    return VERSION;
}

EMSCRIPTEN_KEEPALIVE
int wm_main(const char* cmdline)
{
    int argc = 0;
    std::vector<char*> argv;
    std::vector<std::string> argv_storage;

    split(cmdline ? cmdline : "", argc, argv, argv_storage);

    g_exit_code = 0;
    g_in_wm_main = true;

    if (setjmp(g_exit_jmp) == 0) {
        try {
            g_exit_code = _wm_original_main(argc, argv.data());
        }
        catch (const std::exception& e) {
            std::printf("Exception: %s\n", e.what());
            g_exit_code = 1;
        }
        catch (...) {
            std::printf("Unknown exception\n");
            g_exit_code = 1;
        }
    }

    g_in_wm_main = false;

    return g_exit_code;
}

// ── Stop execution ───────────────────────────────────
EMSCRIPTEN_KEEPALIVE
void wm_stop()
{
    if (g_web_serial_manager) {
        g_web_serial_manager->stop();
    }
}

// ── Serial bridge ────────────────────────────────────
EMSCRIPTEN_KEEPALIVE
void wm_serial_open(int idx)
{
    if (!g_web_serial_manager) {
        g_web_serial_manager = new SerialCommunicationManagerImp();
    }

    g_web_serial_manager->registerWebDevice(idx);
}

EMSCRIPTEN_KEEPALIVE
void wm_serial_close(int idx)
{
    if (!g_web_serial_manager) return;

    g_web_serial_manager->markDeviceDisconnected(idx);
    g_web_serial_manager->cleanupDevice(idx);
}

} // extern "C"