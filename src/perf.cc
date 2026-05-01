/*
 Copyright (C) 2026 Fredrik Öhrström (gpl-3.0-or-later)

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

#include"meters.h"
#include"printer.h"
#include"serial.h"
#include"util.h"
#include"wmbus.h"
#include"dvparser.h"

#include<unistd.h>
#include<inttypes.h>
#include<math.h>
#include<stdio.h>
#include<time.h>

using namespace std;

int64_t current_time_micros()
{
    int64_t us;
    time_t s;
    struct timespec spec;

    clock_gettime(CLOCK_MONOTONIC, &spec);

    s  = spec.tv_sec;
    us = round(spec.tv_nsec / 1.0e3);
    if (us > 999999)
    {
        s++;
        us = 0;
    }
    us += s*1000000;

    return us;
}

int main(int argc, char **argv)
{
    if (argc < 2)
    {
        printf("Usage: perf [file] [reps]\n");
        return 0;
    }

    const char *file = argv[1];
    int reps = atoi(argv[2]);

    vector<char> lines;
    loadFile(file, &lines);
    auto i = lines.begin();

    double sum = 0;
    int    num = 0;

    for (;;)
    {
        bool eof = false;
        bool err = false;
        string line = eatTo(lines, i, '\n', 100*1000*1000, &eof, &err);

        if (line.length() == 0) break;
        vector<uchar> buf;
        bool ok = hex2bin(line, &buf);

        if (ok)
        {
            int64_t start = current_time_micros();
            for (int j = 0; j < reps; ++j)
            {
                map<string,pair<int,DVEntry>> dv_entries;
                Telegram t;
                vector<uchar>::iterator i = buf.begin();
                parseDV(&t, buf, i, buf.size(), &dv_entries);
            }
            int64_t stop = current_time_micros();
            double t1 = ((double)(stop-start))/((double)buf.size());
            double t2 = t1/((double)reps);
            //printf("(%d) %f ns %d bytes %d times\n", num, t2*1000.0, buf.size(), reps);
            sum += t2;
            num ++;
        }
        if (eof || err) break;
    }

    double avg = (sum/((double)num))*1000.0;
    printf("%f\n", avg);
}
