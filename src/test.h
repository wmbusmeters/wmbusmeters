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

#ifndef TEST_FRAMEWORK_H
#define TEST_FRAMEWORK_H

#include <cassert>
#include <cstdio>
#include <cstring>
#include <functional>
#include <string>
#include <vector>

struct _TestCase
{
    std::string name;
    std::function<void()> fn;
    bool failed = false;
    std::string failure_msg;
};

struct _Suite
{
    std::string name;
    std::vector<_TestCase> cases;
};

inline std::vector<_Suite>& _test_suites()
{
    static std::vector<_Suite> s;
    return s;
}

inline _Suite*& _current_suite()
{
    static _Suite* p = nullptr;
    return p;
}

inline _TestCase*& _current_test()
{
    static _TestCase* p = nullptr;
    return p;
}

// When inside an it() block: records failure and continues.
// When outside (e.g. legacy X-macro tests): aborts as the standard assert would.
inline void test_fail(const char* msg)
{
    if (_current_test())
    {
        _current_test()->failed = true;
        _current_test()->failure_msg = msg;
    }
    else
    {
        fprintf(stderr, "%s\n", msg);
        abort();
    }
}

// Override assert: non-aborting inside it() blocks, aborting outside.
#undef assert
#define assert(expr) \
    do { \
        if (!(expr)) { \
            char _assert_buf[512]; \
            snprintf(_assert_buf, sizeof(_assert_buf), \
                     "%s:%d: assert(%s) failed", __FILE__, __LINE__, #expr); \
            test_fail(_assert_buf); \
        } \
    } while (0)

inline int describe(const char* name, std::function<void()> fn)
{
    _test_suites().push_back({name, {}});
    _current_suite() = &_test_suites().back();
    fn();
    _current_suite() = nullptr;
    return 0;
}

inline void it(const char* name, std::function<void()> fn)
{
    if (!_current_suite()) return;
    _current_suite()->cases.push_back({name, fn, false, ""});
}

inline int run_all_tests()
{
#define _CLR_RESET  "\033[0m"
#define _CLR_BOLD   "\033[1m"
#define _CLR_RED    "\033[31m"
#define _CLR_GREEN  "\033[32m"
#define _CLR_DIM    "\033[2m"

    unsigned int total = 0, passed = 0, failed = 0;
    for (auto& suite : _test_suites())
    {
        printf(_CLR_BOLD "%s" _CLR_RESET "\n", suite.name.c_str());
        for (auto& tc : suite.cases)
        {
            _current_test() = &tc;
            tc.fn();
            _current_test() = nullptr;
            total++;
            if (tc.failed)
            {
                printf("  " _CLR_RED "✗" _CLR_RESET " %s\n", tc.name.c_str());
                printf("    " _CLR_DIM "%s" _CLR_RESET "\n", tc.failure_msg.c_str());
                failed++;
            }
            else
            {
                printf("  " _CLR_GREEN "✓" _CLR_RESET " %s\n", tc.name.c_str());
                passed++;
            }
        }
        printf("\n");
    }
    if (failed)
        printf(_CLR_RED "%u/%u passed — FAILURES!" _CLR_RESET "\n", passed, total);
    else
        printf(_CLR_GREEN "%u/%u passed" _CLR_RESET "\n", passed, total);

#undef _CLR_RESET
#undef _CLR_BOLD
#undef _CLR_RED
#undef _CLR_GREEN
#undef _CLR_DIM

    return failed > 0 ? 1 : 0;
}

#endif // TEST_FRAMEWORK_H
