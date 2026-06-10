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

using namespace std;

int main(int argc, char **argv)
{
    int64_t n = benchmark::iterations(argc, argv, 50LL*1000*1000);

    benchmark::run("bcd2bin", n, [&](int64_t i) -> uint64_t {
        return bcd2bin((uchar)(i & 0xff));
    });

    return 0;
}
