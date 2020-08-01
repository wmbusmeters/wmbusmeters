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

#ifndef TIMINGS_H
#define TIMINGS_H

// Default select timeout, every 10 seconds if no other timer/timeout cuts it short.
#define SELECT_TIMEOUT 10
// When running internal tests on timeouts use 5 seconds instead.
#define SELECT_TIMEOUT_INTERNAL_TESTING 5

// Default checkStatus callback frequency every 60 seconds, when an alarmtimeout has been set.
#define CHECKSTATUS_TIMER 60
// When running internal tests on timeouts use 2 seconds instead.
#define CHECKSTATUS_TIMER_INTERNAL_TESTING 2

#endif
