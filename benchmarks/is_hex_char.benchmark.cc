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

#include"benchmark.h"
#include"util.h"

#include<cstring>

using namespace std;

int main(int argc, char **argv)
{
    int64_t n = benchmark::iterations(argc, argv, 50LL*1000*1000);

    const char *chars = "0123456789abcdefABCDEFghijklmnopqrstuvwxyz!@#$%^&* ";
    const int num_chars = (int)strlen(chars);

    benchmark::run("isHexChar", n, [&](int64_t i) -> uint64_t {
        return isHexChar((uchar)chars[i % num_chars]) ? 1 : 0;
    });

    return 0;
}
