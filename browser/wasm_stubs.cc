/*
 Copyright (C) 2026 Aras Abbasi (gpl-3.0-or-later)

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

// ── Semaphore (polling via usleep) ──────────────────────
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
        // Only drain serial data — do NOT fire timers here.
        // tickCallbacks() would fire HOT_PLUG_DETECTOR, triggering a second
        // detectAndConfigureWmbusDevices while a probe is already in progress.
        // Each nested probe also calls Semaphore::wait → usleep, growing the
        // ASYNCIFY saved-stack until the 1 MB buffer overflows ("function
        // signature mismatch" on second start).
        if (g_web_serial_manager) g_web_serial_manager->tickDataOnly();
        usleep(10000); // 10ms
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