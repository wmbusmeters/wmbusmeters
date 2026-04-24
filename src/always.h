/*
 Copyright (C) 2017-2026 Fredrik Öhrström (gpl-3.0-or-later)

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

#ifndef ALWAYS_H
#define ALWAYS_H

#include<stdbool.h>

typedef unsigned char uchar;

struct VoidCallback
{
    void* obj = nullptr;
    void(*fn)(void*) = nullptr;
    void operator()() const { if (fn) fn(obj); }
    explicit operator bool() const { return fn != nullptr; }
};

#define call(A,B) VoidCallback{(void*)(A), [](void* _p){ static_cast<decltype(A)>(_p)->B(); }}
#define calll(A,B,T) ([&](T t){A->B(t);})

#endif // ALWAYS_H
