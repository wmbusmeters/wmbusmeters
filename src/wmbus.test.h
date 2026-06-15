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

#ifndef WMBUS_TEST_H
#define WMBUS_TEST_H

#include"test.h"
#include"wmbus.h"

#include<string>

static void _testd(std::string arg, bool xok,
                   std::string xalias, std::string xfile, std::string xtype,
                   std::string xid, std::string xextras, std::string xfq,
                   std::string xbps, std::string xlm, std::string xcmd)
{
    SpecifiedDevice d;
    bool ok = d.parse(arg);
    if (ok != xok)
    {
        char buf[256];
        snprintf(buf, sizeof(buf),
                 "device parse \"%s\": expected %s but got %s",
                 arg.c_str(), xok ? "OK" : "FALSE", ok ? "OK" : "FALSE");
        test_fail(buf);
        return;
    }
    if (!ok) return;

    if (d.bus_alias != xalias || d.file != xfile || toString(d.type) != xtype ||
        d.id != xid || d.extras != xextras || d.fq != xfq ||
        d.bps != xbps || d.linkmodes.hr() != xlm || d.command != xcmd)
    {
        char buf[512];
        snprintf(buf, sizeof(buf),
                 "device parse \"%s\" got\n"
                 "  alias=%s file=%s type=%s id=%s extras=%s fq=%s bps=%s lm=%s cmd=%s\n"
                 "expected\n"
                 "  alias=%s file=%s type=%s id=%s extras=%s fq=%s bps=%s lm=%s cmd=%s",
                 arg.c_str(),
                 d.bus_alias.c_str(), d.file.c_str(), toString(d.type),
                 d.id.c_str(), d.extras.c_str(), d.fq.c_str(),
                 d.bps.c_str(), d.linkmodes.hr().c_str(), d.command.c_str(),
                 xalias.c_str(), xfile.c_str(), xtype.c_str(),
                 xid.c_str(), xextras.c_str(), xfq.c_str(),
                 xbps.c_str(), xlm.c_str(), xcmd.c_str());
        test_fail(buf);
    }
}

static void _tests(std::string arg, bool expect,
                   LinkMode link_mode, TelegramFormat format,
                   std::string bus, std::string content)
{
    SendBusContent sbc;
    bool rc = sbc.parse(arg);
    if (rc != expect)
    {
        char buf[256];
        snprintf(buf, sizeof(buf),
                 "sbc parse \"%s\": expected %s but %s",
                 arg.c_str(), expect ? "success" : "failure", rc ? "succeeded" : "failed");
        test_fail(buf);
        return;
    }
    if (!expect) return;

    if (sbc.link_mode != link_mode || sbc.format != format ||
        sbc.bus != bus || sbc.content != content)
    {
        char buf[256];
        snprintf(buf, sizeof(buf),
                 "sbc \"%s\" got (lm=%s fmt=%s bus=%s data=%s) expected (lm=%s fmt=%s bus=%s data=%s)",
                 arg.c_str(),
                 toString(sbc.link_mode), toString(sbc.format), sbc.bus.c_str(), sbc.content.c_str(),
                 toString(link_mode), toString(format), bus.c_str(), content.c_str());
        test_fail(buf);
    }
}

static auto _device_parsing_suite = describe("device_parsing", []()
{
    it("parses device specification strings into SpecifiedDevice fields", []()
    {
        _testd("Bus_4711=/dev/ttyUSB0:im871a[12345678]:9600:868.95M:c1,t1", true,
               "Bus_4711", "/dev/ttyUSB0", "im871a", "12345678", "", "868.95M", "9600", "t1,c1", "");
        _testd("/dev/ttyUSB0:im871a:c1", true, "", "/dev/ttyUSB0", "im871a", "", "", "", "", "c1", "");
        _testd("im871a[12345678]:c1", true, "", "", "im871a", "12345678", "", "", "", "c1", "");
        _testd("im871a(track=7,pi=3.14):c1", true, "", "", "im871a", "", "track=7,pi=3.14", "", "", "c1", "");
        _testd("rtlwmbus:c1,t1:CMD(gurka)", true, "", "", "rtlwmbus", "", "", "", "", "t1,c1", "gurka");
        _testd("rtlwmbus[plast]:c1,t1", true, "", "", "rtlwmbus", "plast", "", "", "", "t1,c1", "");
        _testd("ANTENNA1=rtlwmbus[plast](ppm=5):c1,t1", true, "ANTENNA1", "", "rtlwmbus", "plast", "ppm=5", "", "", "t1,c1", "");
        _testd("stdin:rtlwmbus", true, "", "stdin", "rtlwmbus", "", "", "", "", "none", "");
        _testd("/dev/ttyUSB0:rawtty:9600", true, "", "/dev/ttyUSB0", "rawtty", "", "", "", "9600", "none", "");
        _testd("Makefile:simulation", true, "", "Makefile", "simulation", "", "", "", "", "none", "");
        _testd("auto:c1,t1", true, "", "", "auto", "", "", "", "", "t1,c1", "");
        _testd("auto:Makefile:c1,t1", false, "", "", "", "", "", "", "", "none", "");
        _testd("Vatten", false, "", "", "", "", "", "", "", "none", "");
        _testd("main=/dev/ttyUSB0:mbus:2400", true, "main", "/dev/ttyUSB0", "mbus", "", "", "", "2400", "none", "");
        _testd("cul:c1:CMD(socat TCP:CUNO:2323 STDIO)", true, "", "", "cul", "", "", "", "", "c1", "socat TCP:CUNO:2323 STDIO");
    });
});

static auto _sbc_suite = describe("sbc", []()
{
    it("parses send bus content strings", []()
    {
        _tests("send:t1:wmbus_c_field:BUS1:11223344", true,
               LinkMode::T1, TelegramFormat::WMBUS_C_FIELD, "BUS1", "11223344");
        _tests("send:c1:wmbus_ci_field:alfa:11", true,
               LinkMode::C1, TelegramFormat::WMBUS_CI_FIELD, "alfa", "11");
        _tests("send:t2:wmbus_c_field:OUTBUS:1122334455", true,
               LinkMode::T2, TelegramFormat::WMBUS_C_FIELD, "OUTBUS", "1122334455");
        _tests("alfa:t1", false, LinkMode::UNKNOWN, TelegramFormat::UNKNOWN, "", "");
        _tests("send", false, LinkMode::UNKNOWN, TelegramFormat::UNKNOWN, "", "");
        _tests("send:::::::::::", false, LinkMode::UNKNOWN, TelegramFormat::UNKNOWN, "", "");
        _tests("send:foo", false, LinkMode::UNKNOWN, TelegramFormat::UNKNOWN, "", "");
        _tests("send:t2:wmbus_c_field:OUT:", false, LinkMode::UNKNOWN, TelegramFormat::UNKNOWN, "", "");
        _tests("send:t2:wmbus_c_field:OUT:1", false, LinkMode::UNKNOWN, TelegramFormat::UNKNOWN, "", "");
        _tests("send:mbus:mbus_short_frame:out:5b00", true,
               LinkMode::MBUS, TelegramFormat::MBUS_SHORT_FRAME, "out", "5b00");
        _tests("send:mbus:mbus_long_frame:mbus2:1122334455", true,
               LinkMode::MBUS, TelegramFormat::MBUS_LONG_FRAME, "mbus2", "1122334455");
    });
});

#endif // WMBUS_TEST_H
