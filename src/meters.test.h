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

#ifndef METERS_TEST_H
#define METERS_TEST_H

#include"test.h"
#include"meters.h"
#include"config.h"

#include<string>
#include<vector>

static void _testm(std::string arg, bool xok,
                   std::string xdriver, std::string xextras,
                   std::string xbus, std::string xbps, std::string xlm)
{
    MeterInfo mi;
    bool ok = mi.parse("", arg, "12345678", "");
    if (ok != xok)
    {
        char buf[256];
        snprintf(buf, sizeof(buf),
                 "meter parse \"%s\": expected %s but got %s",
                 arg.c_str(), xok ? "OK" : "FALSE", ok ? "OK" : "FALSE");
        test_fail(buf);
        return;
    }
    if (!ok) return;

    if (mi.driverName().str() != xdriver || mi.extras != xextras ||
        mi.bus != xbus || std::to_string(mi.bps) != xbps || mi.link_modes.hr() != xlm)
    {
        char buf[512];
        snprintf(buf, sizeof(buf),
                 "meter parse \"%s\" got\n"
                 "  driver=%s extras=%s bus=%s bps=%s lm=%s\n"
                 "expected\n"
                 "  driver=%s extras=%s bus=%s bps=%s lm=%s",
                 arg.c_str(),
                 mi.driverName().str().c_str(), mi.extras.c_str(),
                 mi.bus.c_str(), std::to_string(mi.bps).c_str(), mi.link_modes.hr().c_str(),
                 xdriver.c_str(), xextras.c_str(), xbus.c_str(), xbps.c_str(), xlm.c_str());
        test_fail(buf);
    }
}

static void _testc(std::string file, std::string file_content,
                   std::string xdriver, std::string xextras,
                   std::string xbus, std::string xbps, std::string xlm)
{
    MeterInfo mi;
    Configuration c;
    std::vector<char> meter_conf(file_content.begin(), file_content.end());
    meter_conf.push_back('\n');
    parseMeterConfig(&c, meter_conf, file);
    assert(c.meters.size() > 0);
    mi = c.meters.back();

    if (mi.driverName().str() != xdriver || mi.extras != xextras ||
        mi.bus != xbus || std::to_string(mi.bps) != xbps || mi.link_modes.hr() != xlm)
    {
        char buf[512];
        snprintf(buf, sizeof(buf),
                 "meter config \"%s\" got\n"
                 "  driver=%s extras=%s bus=%s bps=%s lm=%s\n"
                 "expected\n"
                 "  driver=%s extras=%s bus=%s bps=%s lm=%s",
                 file.c_str(),
                 mi.driverName().str().c_str(), mi.extras.c_str(),
                 mi.bus.c_str(), std::to_string(mi.bps).c_str(), mi.link_modes.hr().c_str(),
                 xdriver.c_str(), xextras.c_str(), xbus.c_str(), xbps.c_str(), xlm.c_str());
        test_fail(buf);
    }
}

static auto _meters_suite = describe("meters", []()
{
    it("parses mbus bus name and baud rate", []()
    {
        _testm("piigth:BUS1:2400",       true, "piigth", "", "BUS1",  "2400", "none");
        _testm("c5isf:MAINO:9600:mbus",  true, "c5isf",  "", "MAINO", "9600", "mbus");
    });

    it("parses wireless link modes", []()
    {
        _testm("c5isf:DONGLE:t1",    true, "c5isf", "", "DONGLE", "0", "t1");
        _testm("c5isf:t1,c1,mbus",   true, "c5isf", "", "",       "0", "mbus,t1,c1");
    });

    it("resolves driver aliases", []()
    {
        _testm("multical21:c1", true, "kamwater", "", "", "0", "c1");
    });

    it("parses driver extras", []()
    {
        _testm("unknown(offset=162)", true, "unknown", "offset=162", "", "0", "none");
    });

    it("parses meter config file with driver and link mode", []()
    {
        _testc("meter/multical21:c1",
               "name=test\ndriver=multical21:c1\nid=01234567\n",
               "kamwater", "", "", "0", "c1");
    });

    it("parses meter config file with driver extras and key", []()
    {
        _testc("meter/apatortest",
               "name=test\ndriver=unknown(offset=99)\nid=01234567\n"
               "key=00000000000000000000000000000000\n",
               "unknown", "offset=99", "", "0", "none");
    });
});

#endif // METERS_TEST_H
