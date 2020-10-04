/*
 Copyright (C) 2020 Fredrik Öhrström

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

#ifndef THREADS_H
#define THREADS_H

#include "util.h"

#include <assert.h>
#include <errno.h>
#include <functional>
#include <pthread.h>
#include <sys/types.h>
#include <unistd.h>

// Declare all threads and locks used in wmbusmeters!

// When the main thread enters serial_manager->waitForStop()
// this thread is recorded as the main thread. It will
// now just sleep until its time for wmbusmeters to exit.
pthread_t getMainThread();
void recordMyselfAsMainThread();

// The event loop thread runs the event loop and executes callbacks to file descriptor
// listeners. This thread is used for all the important work:
// Wmbus-dongle protocol decoding, followed by parsing of telegrams and eventually
// updating and printing meter values and executing a subshell for mqtt.
//
// This thread is not allowed to send commands to the dongles or update
// wmbus-devices or serial-devices, if it does, then wmbusmeters will deadlock,
// since the callbacks are needed to execute the commands.
pthread_t getEventLoopThread();
void startEventLoopThread(std::function<void()> cb);

// The timer callback thread runs whenever a timer timeout has happened.
// This thread is used to probe for lost/found dongles, send commands to dongles,
// reset dongles due to alarms, and generally monitor the system.
pthread_t getTimerLoopThread();
void startTimerLoopThread(std::function<void()> cb);


#define LOCK(module,func,x) { trace("[LOCKING] " #x " " func " (%s %d)\n", x ## func_, x ## pid_); \
                              pthread_mutex_lock(&x); \
                              x ## func_ = func; \
                              x ## pid_ = getpid(); \
                              trace("[LOCKED] "  #x " " func "\n"); }
#define UNLOCK(module,func,x) { trace("[UNLOCKING] " #x " " func " (%s %d) \n", x ## func_, x ## pid_); \
                                pthread_mutex_unlock(&x); \
                                x ## func_ = ""; \
                                x ## pid_ = 0; \
                                trace("[UNLOCKED] " #x " " func "\n"); }

#define WITH(mutex,func) Lock local_ ## mutex (&mutex, #func)

struct Lock;

struct RecursiveMutex
{
    RecursiveMutex(const char *name);
    ~RecursiveMutex();
    void lock();
    void unlock();

private:

    const char *name_;
    pthread_mutex_t mutex_;
    pthread_mutexattr_t attr_;
    const char *locked_in_func_;
    pid_t       locked_by_pid_;

    friend Lock;
};

struct Lock
{
    Lock(RecursiveMutex *rmutex, const char *func_name);
    ~Lock();

private:

    RecursiveMutex  *rmutex_ {};
    const char *func_name_;
};

struct Semaphore
{
    Semaphore(const char *name);
    ~Semaphore();
    bool wait();
    void notify();

private:

    const char *name_;
    pthread_mutex_t mutex_;
    pthread_cond_t condition_;
};

#endif
