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

// Tiny micro benchmark framework.
//
// A benchmark "case" is a standalone program that includes this header,
// includes whatever it wants to measure (e.g. util.h) and calls
// benchmark::run() one or more times from main().
//
// run() grabs a high resolution timestamp, runs the function under test in a
// tight loop, grabs a second timestamp and reports the achieved throughput in
// operations per second (plus the average cost per op in ns).
//
// See benchmarks/char2int.benchmark.cc for an example and the Makefile
// 'benchmark' target for how cases are built and run.

#ifndef BENCHMARK_H
#define BENCHMARK_H

#include<chrono>
#include<cstdint>
#include<cstdio>
#include<cstdlib>

namespace benchmark
{

// Stop the optimizer from deleting the work whose result we never store.
// Writing 'value' through a volatile sink forces the computation to actually
// happen on every loop iteration. Without this the entire benchmark loop can
// be optimised away at -O2, yielding meaningless "infinite" throughput.
inline volatile uint64_t &sink()
{
    static volatile uint64_t s = 0;
    return s;
}

inline void keep(uint64_t value)
{
    sink() = sink() + value;
}

// High resolution monotonic clock, in nanoseconds.
inline int64_t now_nanos()
{
    using namespace std::chrono;
    return duration_cast<nanoseconds>(steady_clock::now().time_since_epoch()).count();
}

// Run 'fn' 'iterations' times and print the throughput.
//
// 'fn' is a callable taking the iteration index and returning a value that
// gets fed to keep(), so the benchmarked work cannot be optimised out.
template<typename Fn>
void run(const char *name, int64_t iterations, Fn fn)
{
    int64_t start = now_nanos();
    for (int64_t i = 0; i < iterations; ++i)
    {
        keep(fn(i));
    }
    int64_t stop = now_nanos();

    double elapsed_s = (double)(stop - start) / 1e9;
    double ops_per_s = (double)iterations / elapsed_s;
    double ns_per_op = (double)(stop - start) / (double)iterations;

    printf("%-22s %14.0f ops/s   %8.3f ns/op   (%lld iters in %.3f s)\n",
           name, ops_per_s, ns_per_op, (long long)iterations, elapsed_s);
}

// Iteration count: argv[1] if supplied, otherwise the supplied default.
// Lets every case be run with a custom count, e.g. ./build/char2int.benchmark 1000000
inline int64_t iterations(int argc, char **argv, int64_t def)
{
    return argc >= 2 ? atoll(argv[1]) : def;
}

}

#endif
