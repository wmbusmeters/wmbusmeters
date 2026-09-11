/*
 Copyright (C) 2018-2026 Fredrik Öhrström (gpl-3.0-or-later)

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

#include"drivers.h"
#include"log.h"
#include"util.h"
#include"utils/signal_handling.h"

#include"test.h"

#include"crypto/crc16.test.h"
#include"crypto/aescmac.test.h"
#include"crypto/aes.test.h"
#include"dvparser.test.h"
#include"address.test.h"
#include"util.test.h"
#include"translatebits.test.h"
#include"units.test.h"
#include"formula.test.h"
#include"meters.test.h"
#include"wmbus.test.h"
#include"config.test.h"
#include"serial.test.h"
#include"drivers.test.h"
#include"utils/slip.test.h"

#include<string.h>

int main(int argc, char **argv)
{
    prepareBuiltinDrivers();
    int i = 1;
    while (i < argc)
    {
        if (!strcmp(argv[i], "--debug"))  { debugEnabled(true); }
        if (!strcmp(argv[i], "--trace"))  { debugEnabled(true); traceEnabled(true); }
        i++;
    }
    onExit([](){});
    return run_all_tests();
}
