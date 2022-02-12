/*
 Copyright (C) 2019 Fredrik Öhrström (gpl-3.0-or-later)

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

#include"cmdline.h"
#include"meters.h"
#include"printer.h"
#include"serial.h"
#include"util.h"
#include"wmbus.h"
#include"dvparser.h"

#include<string.h>
#include<unistd.h>

using namespace std;

int main(int argc, char **argv)
{
    // Fuzzying currently only tests the parsing of difvif wmbus data.
    // The binary difvif data is sent on stdin.
    char buf[1024];
    vector<uchar> databytes;
    vector<char> *ptr = reinterpret_cast<vector<char>*>(&databytes);
    if (argc > 1 && argv[1][0] != 0)
    {
        // Read from file.
        loadFile(string(argv[1]), ptr);
    }
    else
    {
        // Read from stdin
        for (;;) {
            size_t len = read(0, buf, sizeof(buf));
            if (len <= 0) break;
            databytes.insert(databytes.end(), buf, buf+len);
        }
    }

    map<string,pair<int,DVEntry>> values;
    Telegram t;
    vector<uchar>::iterator i = databytes.begin();

    parseDV(&t, databytes, i, databytes.size(), &values);
}
