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

#ifndef TRANSLATEBITS_TEST_H
#define TRANSLATEBITS_TEST_H

#include"test.h"
#include"translatebits.h"

#include<cstdint>
#include<string>

static void _test_join(std::string a, std::string b, std::string expected)
{
    std::string t = joinStatusOKStrings(a, b);
    if (t != expected)
    {
        char buf[256];
        snprintf(buf, sizeof(buf),
                 "joinStatusOKStrings(\"%s\",\"%s\"): expected \"%s\" but got \"%s\"",
                 a.c_str(), b.c_str(), expected.c_str(), t.c_str());
        test_fail(buf);
    }
}

static void _test_sort(std::string in, std::string expected)
{
    std::string t = sortStatusString(in);
    if (t != expected)
    {
        char buf[256];
        snprintf(buf, sizeof(buf),
                 "sortStatusString(\"%s\"): expected \"%s\" but got \"%s\"",
                 in.c_str(), expected.c_str(), t.c_str());
        test_fail(buf);
    }
}

static auto _translate_suite = describe("translate", []()
{
    it("BitToString lookup produces correct status strings", []()
    {
        Translate::Lookup lookup1 =
            Translate::Lookup()
            .add(Translate::Rule("ACCESS_BITS", Translate::MapType::BitToString)
                 .set(MaskBits(0xf0))
                 .add(Translate::Map(0x10, "NO_ACCESS", TestBit::Set))
                 .add(Translate::Map(0x20, "ALL_ACCESS", TestBit::Set))
                 .add(Translate::Map(0x40, "TEMP_ACCESS", TestBit::Set))
                )
            .add(Translate::Rule("ACCESSOR_TYPE", Translate::MapType::IndexToString)
                 .set(MaskBits(0x0f))
                 .add(Translate::Map(0x00, "ACCESSOR_RED", TestBit::Set))
                 .add(Translate::Map(0x07, "ACCESSOR_GREEN", TestBit::Set))
                );

        std::string s = sortStatusString(lookup1.translate(0xa0));
        std::string e = sortStatusString("ALL_ACCESS ACCESS_BITS_80 ACCESSOR_RED");
        assert(s == e);

        s = sortStatusString(lookup1.translate(0x35));
        e = sortStatusString("NO_ACCESS ALL_ACCESS ACCESSOR_TYPE_5");
        assert(s == e);
    });

    it("BitToString lookup with default message and explicit bit flags", []()
    {
        Translate::Lookup lookup2 =
        {
            {
                {
                    "FLOW_FLAGS",
                    Translate::MapType::BitToString,
                    AlwaysTrigger,
                    MaskBits(0x3f),
                    "OOOK",
                    {
                        { 0x01, "BACKWARD_FLOW" },
                        { 0x02, "DRY" },
                        { 0x10, "TRIG" },
                        { 0x20, "COS" },
                    }
                },
            },
        };

        assert(lookup2.translate(0x02) == "DRY");
        assert(lookup2.translate(0x00) == "OOOK");
    });

    it("TestBit::NotSet inverts bit test for NOT_INSTALLED flag", []()
    {
        Translate::Lookup lookup3 =
        {
            {
                {
                    "NO_FLAGS",
                    Translate::MapType::BitToString,
                    AlwaysTrigger,
                    MaskBits(0x03),
                    "OK",
                    {
                        { 0x01, "NOT_INSTALLED", TestBit::NotSet },
                        { 0x02, "FOO" },
                    }
                },
            },
        };

        std::string s = sortStatusString(lookup3.translate(0x02));
        std::string e = sortStatusString("NOT_INSTALLED FOO");
        assert(s == e);
        assert(lookup3.translate(0x01) == "OK");
    });

    it("TriggerBits conditional lookup activates only when trigger bit set", []()
    {
        Translate::Lookup lookup4 =
            Translate::Lookup()
            .add(Translate::Rule("CURRENT_ALARMS_GENERAL", Translate::MapType::BitToString)
                 .set(TriggerBits(0x000080))
                 .set(MaskBits(0xFAA000))
                 .add(Translate::Map(0x008000, "general_alarm"))
                 .add(Translate::Map(0x002000, "general_alarm"))
                 .add(Translate::Map(0x800000, "general_alarm"))
                 .add(Translate::Map(0x400000, "general_alarm"))
                 .add(Translate::Map(0x200000, "general_alarm"))
                 .add(Translate::Map(0x100000, "general_alarm"))
                 .add(Translate::Map(0x080000, "general_alarm"))
                 .add(Translate::Map(0x020000, "general_alarm"))
                )
            .add(Translate::Rule("CURRENT_ALARMS", Translate::MapType::BitToString)
                 .set(MaskBits(0xFAA000))
                 .set(DefaultMessage("no_alarm"))
                 .add(Translate::Map(0x008000, "leakage"))
                 .add(Translate::Map(0x002000, "meter_blocked"))
                 .add(Translate::Map(0x800000, "back_flow"))
                 .add(Translate::Map(0x400000, "underflow"))
                 .add(Translate::Map(0x200000, "overflow"))
                 .add(Translate::Map(0x100000, "submarine"))
                 .add(Translate::Map(0x080000, "sensor_fraud"))
                 .add(Translate::Map(0x020000, "mechanical_fraud"))
                );

        assert(lookup4.translate((uint64_t)0x000080) == "no_alarm");
        assert(lookup4.translate((uint64_t)0x402000) == "meter_blocked underflow");
        assert(lookup4.translate((uint64_t)0x402080) == "general_alarm meter_blocked underflow");
    });
});

static auto _status_join_suite = describe("status_join", []()
{
    it("joinStatusOKStrings merges two status strings correctly", []()
    {
        _test_join("OK", "OK", "OK");
        _test_join("", "", "OK");
        _test_join("OK", "", "OK");
        _test_join("", "OK", "OK");
        _test_join("null", "OK", "OK");
        _test_join("null", "null", "OK");
        _test_join("ERROR FLOW", "OK", "ERROR FLOW");
        _test_join("ERROR FLOW", "", "ERROR FLOW");
        _test_join("OK", "ERROR FLOW", "ERROR FLOW");
        _test_join("", "ERROR FLOW", "ERROR FLOW");
        _test_join("ERROR", "FLOW", "ERROR FLOW");
        _test_join("ERROR", "null", "ERROR");
        _test_join("A B C", "D E F G", "A B C D E F G");
    });
});

static auto _status_sort_suite = describe("status_sort", []()
{
    it("sortStatusString deduplicates and alphabetises tokens", []()
    {
        _test_sort("C B A", "A B C");
        _test_sort("ERROR BUSY FLOW ERROR", "BUSY ERROR FLOW");
        _test_sort("X X X Y Y Z A B C A A AAAA AA AAA", "A AA AAA AAAA B C X Y Z");
    });
});

#endif // TRANSLATEBITS_TEST_H
