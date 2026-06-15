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

#ifndef UTIL_TEST_H
#define UTIL_TEST_H

#include"test.h"
#include"util.h"

#include<ctime>
#include<string>

static void _testp(time_t now, std::string period, bool expected)
{
    bool rc = isInsideTimePeriod(now, period);
    if (expected == rc) return;
    char buf[256];
    snprintf(buf, sizeof(buf),
             "period \"%s\": expected %s but got %s",
             period.c_str(), expected ? "inside" : "outside", rc ? "inside" : "outside");
    test_fail(buf);
}

static void _test_month(int y, int m, int day, int mdiff, std::string from, std::string to)
{
    struct tm date {};
    date.tm_year = y - 1900;
    date.tm_mon  = m - 1;
    date.tm_mday = day;

    std::string s = strdate(&date);

    struct tm d = date;
    addMonths(&d, mdiff);
    std::string os = strdate(&d);

    if (s != from || os != to)
    {
        char buf[256];
        snprintf(buf, sizeof(buf),
                 "expected %s + %d months = %s but got %s = %s",
                 from.c_str(), mdiff, to.c_str(), s.c_str(), os.c_str());
        test_fail(buf);
    }
}

static void _test_year(int y, int m, int day, int ydiff, std::string from, std::string to)
{
    struct tm date {};
    date.tm_year = y - 1900;
    date.tm_mon  = m - 1;
    date.tm_mday = day;

    std::string s = strdate(&date);

    struct tm d = date;
    addYears(&d, ydiff);
    std::string os = strdate(&d);

    if (s != from || os != to)
    {
        char buf[256];
        snprintf(buf, sizeof(buf),
                 "expected %s + %d years = %s but got %s = %s",
                 from.c_str(), ydiff, to.c_str(), s.c_str(), os.c_str());
        test_fail(buf);
    }
}

static void _test_is_hex(const char *hex, bool expected_ok, bool expected_invalid, bool strict)
{
    bool got_invalid;
    bool got_ok = strict ? isHexStringStrict(hex, &got_invalid) : isHexStringFlex(hex, &got_invalid);
    if (got_ok != expected_ok || got_invalid != expected_invalid)
    {
        char buf[256];
        snprintf(buf, sizeof(buf),
                 "hex string \"%s\": expected ok=%d invalid=%d but got ok=%d invalid=%d",
                 hex, expected_ok, expected_invalid, got_ok, got_invalid);
        test_fail(buf);
    }
}

static auto _periods_suite = describe("periods", []()
{
    it("isInsideTimePeriod is timezone-independent for known Thursday 01:00", []()
    {
        time_t t = 3600*24*7+3600;
        struct tm value {};
        localtime_r(&t, &value);
        t -= value.tm_gmtoff;

        _testp(t, "mon-sun(00-23)", true);
        _testp(t, "mon(00-23)", false);
        _testp(t, "thu-fri(01-01)", true);
        _testp(t, "mon-wed(00-23),thu(02-23),fri-sun(00-23)", false);
        _testp(t, "mon-wed(00-23),thu(01-23),fri-sun(00-23)", true);
        _testp(t, "thu(00-00)", false);
        _testp(t, "thu(01-01)", true);
    });
});

static auto _months_suite = describe("months", []()
{
    it("addMonths handles end-of-month clamping and leap years", []()
    {
        _test_month(2020,12,31,  2, "2020-12-31", "2021-02-28");
        _test_month(2020,12,31,-10, "2020-12-31", "2020-02-29");
        _test_month(2021, 1,31, -1, "2021-01-31", "2020-12-31");
        _test_month(2021, 1,31, -2, "2021-01-31", "2020-11-30");
        _test_month(2021, 1,31,-24, "2021-01-31", "2019-01-31");
        _test_month(2021, 1,31, 24, "2021-01-31", "2023-01-31");
        _test_month(2021, 1,31, 22, "2021-01-31", "2022-11-30");
        _test_month(2021, 2,28,-12, "2021-02-28", "2020-02-29");
        _test_month(2001, 2,28,-12, "2001-02-28", "2000-02-29");
        _test_month(2000, 2,29, 12*100, "2000-02-29", "2100-02-28");
    });
});

static auto _years_suite = describe("years", []()
{
    it("addYears handles leap year clamping", []()
    {
        _test_year(2020,12,31,  2, "2020-12-31", "2022-12-31");
        _test_year(2020,12,31, -2, "2020-12-31", "2018-12-31");
        _test_year(1900, 1, 1,222, "1900-01-01", "2122-01-01");
        _test_year(2021, 2,28, -1, "2021-02-28", "2020-02-29");
        _test_year(2001, 2,28, -1, "2001-02-28", "2000-02-29");
        _test_year(2000, 2,29,100, "2000-02-29", "2100-02-28");
    });
});

static auto _hex_suite = describe("hex", []()
{
    it("isHexStringStrict accepts valid even-length hex, rejects odd-length and bad chars", []()
    {
        _test_is_hex("00112233445566778899aabbccddeeff", true, false, true);
        _test_is_hex("00112233445566778899AABBCCDDEEFF", true, false, true);
        _test_is_hex("00112233445566778899AABBCCDDEEF",  true, true,  true);
        _test_is_hex("00112233445566778899AABBCCDDEEFG", false, false, true);
    });

    it("isHexStringFlex accepts separated and mixed hex strings", []()
    {
        _test_is_hex("00 11 22 33#44|55#66 778899aabbccddeeff", true, false, false);
        _test_is_hex("00 11 22 33#4|55#66 778899aabbccddeeff",  true, true,  false);
    });
});

static auto _ascii_detection_suite = describe("ascii_detection", []()
{
    it("isLikelyAscii returns false for binary-looking hex strings", []()
    {
        assert(!isLikelyAscii("000008"));
        assert(!isLikelyAscii("000041194300"));
    });

    it("isLikelyAscii returns true for ASCII-range hex strings", []()
    {
        assert(isLikelyAscii("41424344"));
        assert(isLikelyAscii("000041424344"));
    });
});

#endif // UTIL_TEST_H
