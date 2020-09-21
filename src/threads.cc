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
