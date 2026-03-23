// wasm_stubs.cc
// Stubs für Hardware/Threading/OS-Funktionen die im WASM-Build nicht verfügbar sind.
// BusManager ist NICHT mehr hier — kommt aus bus.cc.

#include "util.h"
#include "threads.h"
#include "serial_web.h"

#include <signal.h>
#include <string>
#include <vector>
#include <functional>
#include <memory>
#include <map>

using namespace std;

// ── Semaphore (polling via usleep → Asyncify-kompatibel) ──────────────────────
// threads.h bleibt unverändert, Zustand wird extern getrackt.
static map<Semaphore*, bool> g_semaphore_notified;

Semaphore::Semaphore(const char *name) : name_(name)
{
    g_semaphore_notified[this] = false;
}

Semaphore::~Semaphore()
{
    g_semaphore_notified.erase(this);
}

bool Semaphore::wait()
{
    g_semaphore_notified[this] = false;
    // 500 × 10ms = 5s Timeout
    for (int i = 0; i < 500 && !g_semaphore_notified[this]; i++) {
        // Tick serial callbacks so processSerialData() runs during
        // waitForResponse() — replaces the native event loop thread.
        if (g_web_serial_manager) g_web_serial_manager->tickCallbacks();
        usleep(10000); // 10ms — wird via usleep_async.h zu emscripten_sleep(10)
    }
    bool ok = g_semaphore_notified[this];
    g_semaphore_notified[this] = false;
    return ok;
}

void Semaphore::notify()
{
    g_semaphore_notified[this] = true;
}

// ── RecursiveMutex ────────────────────────────────────────────────────────────
RecursiveMutex::RecursiveMutex(const char *name) : name_(name), locked_in_func_(""), locked_by_pid_(0) {}
RecursiveMutex::~RecursiveMutex() {}
void RecursiveMutex::lock() {}
void RecursiveMutex::unlock() {}

// ── Lock ──────────────────────────────────────────────────────────────────────
Lock::Lock(RecursiveMutex *rmutex, const char *func_name) : rmutex_(rmutex), func_name_(func_name) {}
Lock::~Lock() {}

// ── Thread-Funktionen ─────────────────────────────────────────────────────────
pthread_t getMainThread() { return 0; }
void recordMyselfAsMainThread() {}
pthread_t getEventLoopThread() { return 0; }
void startEventLoopThread(std::function<void()> cb) {}
pthread_t getTimerLoopThread() { return 0; }
void startTimerLoopThread(std::function<void()> cb) {}
size_t getPeakRSS() { return 0; }
size_t getCurrentRSS() { return 0; }

// ── pthread_kill ──────────────────────────────────────────────────────────────
extern "C" int pthread_kill(pthread_t, int) { return 0; }

// ── Process detection ─────────────────────────────────────────────────────────
void detectProcesses(string name, vector<int> *pids)
{
    if (pids) pids->clear();
}

// ── Shell ─────────────────────────────────────────────────────────────────────
#include "shell.h"

void invokeShell(string command, vector<string> envs, vector<string> args) {}

int invokeShellCaptureOutput(string program, vector<string> args, vector<string> envs, string *out, bool do_not_warn_if_fail)
{
    if (out) *out = "";
    return -1;
}

// ── Bus device open/detect stubs (devices not relevant for WebSerial) ─────
#include "wmbus.h"

shared_ptr<BusDevice> openMBUS(Detected, shared_ptr<SerialCommunicationManager>, shared_ptr<SerialDevice>) { return nullptr; }
shared_ptr<BusDevice> openXmqTTY(Detected, shared_ptr<SerialCommunicationManager>, shared_ptr<SerialDevice>) { return nullptr; }
shared_ptr<BusDevice> openIU880B(Detected, shared_ptr<SerialCommunicationManager>, shared_ptr<SerialDevice>) { return nullptr; }
shared_ptr<BusDevice> openSocket(Detected, shared_ptr<SerialCommunicationManager>, shared_ptr<SerialDevice>) { return nullptr; }
shared_ptr<BusDevice> openRTLWMBUS(Detected, string, bool, shared_ptr<SerialCommunicationManager>, shared_ptr<SerialDevice>) { return nullptr; }
shared_ptr<BusDevice> openRTL433(Detected, string, bool, shared_ptr<SerialCommunicationManager>, shared_ptr<SerialDevice>) { return nullptr; }

AccessCheck detectMBUS(Detected*, shared_ptr<SerialCommunicationManager>) { return AccessCheck::NoSuchDevice; }
AccessCheck detectRTLSDR(string, Detected*) { return AccessCheck::NoSuchDevice; }

vector<string> listRtlSdrDevices() { return {}; }
bool check_if_rtl433_exists_in_path() { return false; }