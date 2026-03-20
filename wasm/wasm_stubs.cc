// wasm_stubs.cc
// Stubs für Hardware/Threading-Funktionen die im WASM-Build nicht benötigt werden.

#include "util.h"
#include "threads.h"

// ── Semaphore ─────────────────────────────────────────────────────────────────
Semaphore::Semaphore(const char *name) : name_(name) {}
Semaphore::~Semaphore() {}
bool Semaphore::wait() { return false; }
void Semaphore::notify() {}

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

// ── BusManager ────────────────────────────────────────────────────────────────
#include "bus.h"
BusDevice *BusManager::findBus(string bus_alias) { return nullptr; }

