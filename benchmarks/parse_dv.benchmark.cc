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

// Benchmark for parseDV(), the DIF/VIF data record parser (formerly perf.cc).
//
//   make benchmark parse_dv                 # embedded sample telegram
//   ./build/parse_dv.benchmark <file> <reps># parse each hex line in <file> reps times
//   ./build/parse_dv.benchmark <iterations> # embedded telegram, custom iteration count
//
// scripts/perftest.sh feeds a collected corpus of difvif segments via the
// <file> <reps> form.

#include"benchmark.h"
#include"dvparser.h"
#include"util.h"
#include"wmbus.h"

#include<string>
#include<unordered_map>
#include<utility>
#include<vector>

using namespace std;

// A rich set of M-Bus data records (taken from the internal test suite) used
// when no corpus file is supplied, so the benchmark runs standalone.
static const char *EMBEDDED_TELEGRAM =
    "0C1348550000426CE1F14C130000000082046C21298C0413330000008D04931E3A3CFE3300000033000000330000003300000033000000330000003300000033000000330000003300000033000000330000004300000034180000046D0D0B5C2B03FD6C5E150082206C5C290BFD0F0200018C4079678885238310FD3100000082106C01018110FD610002FD66020002FD170000";

// Load one telegram per hex line from 'file'. Returns false if nothing usable.
static bool loadTelegrams(const char *file, vector<vector<uchar>> *out)
{
    vector<char> lines;
    loadFile(file, &lines);
    auto i = lines.begin();
    for (;;)
    {
        bool eof = false, err = false;
        string line = eatTo(lines, i, '\n', 100*1000*1000, &eof, &err);
        if (line.length() > 0)
        {
            vector<uchar> buf;
            if (hex2bin(line, &buf)) out->push_back(std::move(buf));
        }
        if (eof || err) break;
    }
    return !out->empty();
}

int main(int argc, char **argv)
{
    vector<vector<uchar>> telegrams;
    int64_t iterations;

    if (argc >= 2 && checkFileExists(argv[1]))
    {
        // Corpus mode: parse each telegram 'reps' times (perf.cc semantics).
        if (!loadTelegrams(argv[1], &telegrams))
        {
            fprintf(stderr, "No usable telegrams in %s\n", argv[1]);
            return 1;
        }
        int64_t reps = argc >= 3 ? atoll(argv[2]) : 1000;
        iterations = reps * (int64_t)telegrams.size();
    }
    else
    {
        // Embedded mode: a single telegram, iteration count from argv[1].
        vector<uchar> buf;
        hex2bin(EMBEDDED_TELEGRAM, &buf);
        telegrams.push_back(std::move(buf));
        iterations = benchmark::iterations(argc, argv, 100LL*1000);
    }

    const size_t count = telegrams.size();

    benchmark::run("parseDV", iterations, [&](int64_t i) -> uint64_t {
        vector<uchar> &buf = telegrams[i % count];
        unordered_map<string,pair<int,DVEntry>> dv_entries;
        Telegram t;
        vector<uchar>::iterator pos = buf.begin();
        parseDV(&t, buf, pos, buf.size(), &dv_entries);
        return (uint64_t)dv_entries.size();
    });

    return 0;
}
