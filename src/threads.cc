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

#include "threads.h"

#include <unistd.h>
#include <sys/resource.h>
#include <stdio.h>

using namespace std;

pthread_t main_thread_ {};

pthread_t event_loop_thread_ {};
function<void()> event_loop_entry_point_;

pthread_t timer_loop_thread_ {};
function<void()> timer_loop_entry_point_;

pthread_t getMainThread()
{
    return main_thread_;
}

void recordMyselfAsMainThread()
{
    main_thread_ = pthread_self();
}

pthread_t getEventLoopThread()
{
    return event_loop_thread_;
}

void *dispatch(void *ptr)
{
    function<void()> *cb = static_cast<function<void()>*>(ptr);
    (*cb)();
    return NULL;
}

void startEventLoopThread(function<void()> cb)
{
    event_loop_entry_point_ = cb;
    pthread_create(&event_loop_thread_, NULL, dispatch, &event_loop_entry_point_);
}

pthread_t getTimerLoopThread()
{
    return timer_loop_thread_;
}

void startTimerLoopThread(function<void()> cb)
{
    timer_loop_entry_point_ = cb;
    pthread_create(&timer_loop_thread_, NULL, dispatch, &timer_loop_entry_point_);
}

pthread_mutex_t wmbus_devices_lock_ = PTHREAD_MUTEX_INITIALIZER;
const char *wmbus_devices_lock_func_ = "";
pid_t       wmbus_devices_lock_pid_;

pthread_mutex_t serial_devices_lock_ = PTHREAD_MUTEX_INITIALIZER;
const char *serial_devices_lock_func_ = "";
pid_t       serial_devices_lock_pid_;

pthread_mutex_t event_loop_lock_ = PTHREAD_MUTEX_INITIALIZER;
const char *event_loop_lock_func_ = "";
pid_t       event_loop_lock_pid_;

pthread_mutex_t timers_lock_ = PTHREAD_MUTEX_INITIALIZER;
const char *timers_lock_func_ = "";
pid_t       timers_lock_pid_;

RecursiveMutex serial_devices_mutex_("serial_devices_mutex");

RecursiveMutex::RecursiveMutex(const char *name)
    : name_(name), locked_in_func_(""), locked_by_pid_(0)
{
    pthread_mutexattr_init(&attr_);
    pthread_mutexattr_settype(&attr_, PTHREAD_MUTEX_RECURSIVE);
    pthread_mutex_init(&mutex_, &attr_);
}

RecursiveMutex::~RecursiveMutex()
{
    pthread_mutex_destroy(&mutex_);
    pthread_mutexattr_destroy(&attr_);
}

void RecursiveMutex::lock()
{
    pthread_mutex_lock(&mutex_);
}

void RecursiveMutex::unlock()
{
    pthread_mutex_unlock(&mutex_);
}

Lock::Lock(RecursiveMutex *rmutex, const char *func_name)
{
    rmutex_ = rmutex;
    func_name_ = func_name;
    trace("[LOCKING] %s %s (%s %d)\n", rmutex_->name_, func_name_, rmutex_->locked_in_func_, rmutex->locked_by_pid_);
    pthread_mutex_lock(&rmutex_->mutex_);
    rmutex->locked_in_func_ = func_name;
    rmutex->locked_by_pid_ = getpid();
    trace("[LOCKED]  %s %s (%s %d)\n", rmutex_->name_, func_name_, rmutex_->locked_in_func_, rmutex->locked_by_pid_);
}

Lock::~Lock()
{
    trace("[UNLOCKING] %s %s (%s %d)\n", rmutex_->name_, func_name_, rmutex_->locked_in_func_, rmutex_->locked_by_pid_);
    pthread_mutex_unlock(&rmutex_->mutex_);
    rmutex_->locked_in_func_ = "";
    rmutex_->locked_by_pid_ = 0;
    trace("[UNLOCKED]  %s %s (%s %d)\n", rmutex_->name_, func_name_, rmutex_->locked_in_func_, rmutex_->locked_by_pid_);
}


Semaphore::Semaphore(const char *name)
    : name_(name)
{
    pthread_cond_init(&condition_, NULL);
    pthread_mutex_init(&mutex_, NULL);
}

Semaphore::~Semaphore()
{
    pthread_mutex_destroy(&mutex_);
    pthread_cond_destroy(&condition_);
}

bool Semaphore::wait()
{
    trace("[WAITING] %s\n", name_);

    pthread_mutex_lock(&mutex_);
    struct timespec wait_until;
    clock_gettime(CLOCK_REALTIME, &wait_until);
    wait_until.tv_sec += 5;

    int rc = 0;
    for (;;)
    {
        rc = pthread_cond_timedwait(&condition_, &mutex_, &wait_until);
        if (!rc) break;
        if (rc == EINTR) continue;
        if (rc == ETIMEDOUT) break;
        error("(thread) pthread cond timedwait ERROR %d\n", rc);
    }

    pthread_mutex_unlock(&mutex_);

    trace("[WAITED] %s %s\n", name_, (rc==ETIMEDOUT)?"TIMEOUT":"OK");

    // Return true if proper wait.
    // Return false if timeout!!!!
    return rc != ETIMEDOUT;
}

void Semaphore::notify()
{
    trace("[NOTIFY] %s\n", name_);
    int rc = pthread_cond_signal(&condition_);
    if (rc)
    {
        error("(thread) pthread cond signal ERROR\n");
    }
}

size_t getPeakRSS()
{
    struct rusage rusage;

    getrusage( RUSAGE_SELF, &rusage );

#if defined(__APPLE__) && defined(__MACH__)
    return (size_t)rusage.ru_maxrss;
#else
    return (size_t)(rusage.ru_maxrss * 1024L);
#endif
}

size_t getCurrentRSS()
{
#if defined(__APPLE__) && defined(__MACH__)

    struct mach_task_basic_info info;

    mach_msg_type_number_t info_count = MACH_TASK_BASIC_INFO_COUNT;

    int rc = task_info(mach_task_self(), MACH_TASK_BASIC_INFO, (task_info_t)&info, &info_count);
    if (rc != KERN_SUCCESS)
    {
        return 0;
    }
    return (size_t)info.resident_size;

#elif defined(__linux__) || defined(__linux) || defined(linux) || defined(__gnu_linux__)

    long rss = 0;
    FILE *fp = fopen("/proc/self/statm", "r");
    if (!fp)
    {
        return 0;
    }
    int rc = fscanf(fp, "%*s%ld", &rss);
    if (rc != 1)
    {
        fclose(fp);
        return 0;
    }
    fclose(fp);
    return (size_t)rss * (size_t)sysconf( _SC_PAGESIZE);

#endif
}
