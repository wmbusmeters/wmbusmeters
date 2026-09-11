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

#ifndef ADDRESS_TEST_H
#define ADDRESS_TEST_H

#include"test.h"
#include"address.h"
#include"manufacturers.h"

#include<cstdint>
#include<string>
#include<vector>

static void _test_valid_match_expression(std::string s, bool expected)
{
    bool b = isValidSequenceOfAddressExpressions(s);
    if (b == expected) return;
    char buf[256];
    if (expected)
        snprintf(buf, sizeof(buf), "expected \"%s\" to be valid but it was not", s.c_str());
    else
        snprintf(buf, sizeof(buf), "expected \"%s\" to be invalid but it was reported valid", s.c_str());
    test_fail(buf);
}

static void _test_does_id_match_expression(std::string id, std::string mes,
                                           bool expected, bool expected_uw)
{
    Address a;
    a.id = id;
    std::vector<Address> as;
    as.push_back(a);
    std::vector<AddressExpression> expressions = splitAddressExpressions(mes);
    bool uw = false;
    bool b = doesTelegramMatchExpressions(as, expressions, &uw);
    if (b != expected)
    {
        char buf[256];
        if (expected)
            snprintf(buf, sizeof(buf), "expected \"%s\" to match \"%s\" but it did not", id.c_str(), mes.c_str());
        else
            snprintf(buf, sizeof(buf), "expected \"%s\" to NOT match \"%s\" but it did", id.c_str(), mes.c_str());
        test_fail(buf);
    }
    if (expected_uw != uw)
    {
        char buf[256];
        snprintf(buf, sizeof(buf), "matching \"%s\" \"%s\": expected used_wildcard %d but got %d",
                 id.c_str(), mes.c_str(), expected_uw, uw);
        test_fail(buf);
    }
}

static void _tst_address(std::string s, bool valid,
                          std::string id, bool has_wildcard,
                          std::string mfct,
                          uchar version,
                          uchar type,
                          bool mbus_primary,
                          bool filter_out)
{
    AddressExpression a;
    bool ok = a.parse(s);
    if (ok != valid)
    {
        char buf[256];
        snprintf(buf, sizeof(buf), "parse of address \"%s\" returned %s, expected %s",
                 s.c_str(), ok ? "valid" : "bad", valid ? "valid" : "bad");
        test_fail(buf);
        return;
    }
    if (!ok) return;

    std::string smfct = manufacturerFlag(a.mfct);
    if (id != a.id || has_wildcard != a.has_wildcard || mfct != smfct ||
        version != a.version || type != a.type ||
        mbus_primary != a.mbus_primary || filter_out != a.filter_out)
    {
        char buf[512];
        snprintf(buf, sizeof(buf),
                 "parse of \"%s\": got (id=%s wild=%d mfct=%s v=%02x t=%02x mbus=%d neg=%d) "
                 "expected (id=%s wild=%d mfct=%s v=%02x t=%02x mbus=%d neg=%d)",
                 s.c_str(),
                 a.id.c_str(), a.has_wildcard, smfct.c_str(), a.version, a.type, a.mbus_primary, a.filter_out,
                 id.c_str(), has_wildcard, mfct.c_str(), version, type, mbus_primary, filter_out);
        test_fail(buf);
    }
}

static void _tst_address_match(std::string expr, std::string id,
                                uint16_t m, uchar v, uchar t,
                                bool match, bool filter_out)
{
    AddressExpression e;
    bool ok = e.parse(expr);
    assert(ok);
    bool test = e.match(id, m, v, t);
    if (test != match)
    {
        char buf[256];
        snprintf(buf, sizeof(buf), "address %s %04x %02x %02x: expected %smatch for expression %s",
                 id.c_str(), m, v, t, match ? "" : "no ", expr.c_str());
        test_fail(buf);
    }
    if (match && e.filter_out != filter_out)
    {
        char buf[256];
        snprintf(buf, sizeof(buf), "expected %s from match expression %s",
                 filter_out ? "FILTERED OUT" : "NOT filtered", expr.c_str());
        test_fail(buf);
    }
}

static void _tst_telegram_match(std::string addresses, std::string expressions,
                                 bool match, bool uw)
{
    std::vector<AddressExpression> exprs = splitAddressExpressions(expressions);
    std::vector<AddressExpression> as = splitAddressExpressions(addresses);
    std::vector<Address> addrs;
    for (auto &ad : as)
    {
        Address a;
        a.id = ad.id;
        a.mfct = ad.mfct;
        a.version = ad.version;
        a.type = ad.type;
        addrs.push_back(a);
    }
    bool used_wildcard = false;
    bool m = doesTelegramMatchExpressions(addrs, exprs, &used_wildcard);
    if (m != match)
    {
        char buf[256];
        snprintf(buf, sizeof(buf), "addresses %s: expected %smatch for expressions %s",
                 addresses.c_str(), match ? "" : "NOT ", expressions.c_str());
        test_fail(buf);
    }
    if (uw != used_wildcard)
    {
        char buf[256];
        snprintf(buf, sizeof(buf), "addresses %s / expressions %s: expected %susing wildcard",
                 addresses.c_str(), expressions.c_str(), uw ? "" : "NOT ");
        test_fail(buf);
    }
}

static auto _ids_suite = describe("ids", []()
{
    it("validates correct match expressions", []()
    {
        _test_valid_match_expression("12345678", true);
        _test_valid_match_expression("*", true);
        _test_valid_match_expression("!12345678", true);
        _test_valid_match_expression("12345*", true);
        _test_valid_match_expression("!123456*", true);
        _test_valid_match_expression("2222*,!22224444", true);
    });

    it("rejects invalid match expressions", []()
    {
        _test_valid_match_expression("1234567", false);
        _test_valid_match_expression("", false);
        _test_valid_match_expression("z1234567", false);
        _test_valid_match_expression("123456789", false);
        _test_valid_match_expression("!!12345678", false);
        _test_valid_match_expression("12345678*", false);
        _test_valid_match_expression("**", false);
        _test_valid_match_expression("123**", false);
    });

    it("matches IDs against expressions including wildcards and negation", []()
    {
        _test_does_id_match_expression("12345678", "12345678", true, false);
        _test_does_id_match_expression("12345678", "*", true, true);
        _test_does_id_match_expression("12345678", "2*", false, false);
        _test_does_id_match_expression("12345678", "*,!2*", true, true);
        _test_does_id_match_expression("22222222", "22*,!22222222", false, false);
        _test_does_id_match_expression("22222223", "22*,!22222222", true, true);
        _test_does_id_match_expression("22222223", "*,!22*", false, false);
        _test_does_id_match_expression("12333333", "123*,!1234*,!1235*,!1236*", true, true);
        _test_does_id_match_expression("12366666", "123*,!1234*,!1235*,!1236*", false, false);
        _test_does_id_match_expression("11223344", "22*,33*,44*,55*", false, false);
        _test_does_id_match_expression("55223344", "22*,33*,44*,55*", true, true);
        _test_does_id_match_expression("78563413", "78563412,78563413", true, false);
        _test_does_id_match_expression("78563413", "*,!00156327,!00048713", true, true);
    });
});

static auto _addresses_suite = describe("addresses", []()
{
    it("parses plain 8-digit address", []()
    {
        _tst_address("12345678", true, "12345678", false, "___", 0xff, 0xff, false, false);
    });

    it("rejects malformed addresses", []()
    {
        _tst_address("123k45678", false, "", false, "", 0xff, 0xff, false, false);
        _tst_address("1234", false, "", false, "", 0xff, 0xff, false, false);
    });

    it("parses M-bus primary addresses", []()
    {
        _tst_address("p0",   true, "p0",  false, "___", 0xff, 0xff, true, false);
        _tst_address("p250", true, "p250",false, "___", 0xff, 0xff, true, false);
        _tst_address("p251", false, "", false, "", 0xff, 0xff, false, false);
    });

    it("parses primary address with M/V/T qualifiers", []()
    {
        _tst_address("p0.M=PII.V=01.T=1b",   true, "p0",  false, "PII", 0x01, 0x1b, true, false);
        _tst_address("p123.V=11.M=FOO.T=ff", true, "p123",false, "FOO", 0x11, 0xff, true, false);
        _tst_address("p123.M=FOO",            true, "p123",false, "FOO", 0xff, 0xff, true, false);
        _tst_address("p123.M=FOO.V=33",       true, "p123",false, "FOO", 0x33, 0xff, true, false);
        _tst_address("p123.T=33",             true, "p123",false, "___", 0xff, 0x33, true, false);
        _tst_address("p1.V=33",               true, "p1",  false, "___", 0x33, 0xff, true, false);
        _tst_address("p16.M=BAR",             true, "p16", false, "BAR", 0xff, 0xff, true, false);
    });

    it("parses wmbus address with M/V/T qualifiers", []()
    {
        _tst_address("12345678.M=ABB.V=66.T=16",  true, "12345678", false, "ABB", 0x66, 0x16, false, false);
        _tst_address("!12345678.M=ABB.V=66.T=16", true, "12345678", false, "ABB", 0x66, 0x16, false, true);
        _tst_address("!*.M=ABB",                  true, "*",        true,  "ABB", 0xff, 0xff, false, true);
        _tst_address("!*.V=66.T=06",              true, "*",        true,  "___", 0x66, 0x06, false, true);
    });

    it("parses wildcard addresses", []()
    {
        _tst_address("12*",       true, "12*",      true, "___", 0xff, 0xff, false, false);
        _tst_address("!1234567*", true, "1234567*", true, "___", 0xff, 0xff, false, true);
    });

    it("address expression match: basic id/mfct/version matching", []()
    {
        _tst_address_match("12345678", "12345678", 1, 1, 1, true, false);
        _tst_address_match("12345678.M=ABB.V=77", "12345678", MANUFACTURER_ABB, 0x77, 88, true, false);
        _tst_address_match("1*.V=77", "12345678", MANUFACTURER_ABB, 0x77, 1, true, false);
        _tst_address_match("12345678.M=ABB.V=67.T=06", "12345678", MANUFACTURER_ABB, 0x67, 0x06, true, false);
        _tst_address_match("12345678.M=ABB.V=67.T=06", "12345678", MANUFACTURER_ABB, 0x68, 0x06, false, false);
        _tst_address_match("12345678.M=ABB.V=67.T=06", "12345678", MANUFACTURER_ABB, 0x67, 0x07, false, false);
        _tst_address_match("12345678.M=ABB.V=67.T=06", "12345678", MANUFACTURER_ABB+1, 0x67, 0x06, false, false);
        _tst_address_match("12345678.M=ABB.V=67.T=06", "12345677", MANUFACTURER_ABB, 0x67, 0x06, false, false);
    });

    it("address expression match: filter_out negation", []()
    {
        _tst_address_match("!12345678", "12345677", 1, 1, 1, false, false);
        _tst_address_match("!*.M=ABB", "99999999", MANUFACTURER_ABB, 1, 1, true, true);
        _tst_address_match("*.M=ABB",  "99999999", MANUFACTURER_ABB, 1, 1, true, false);
    });

    it("address expression match: wildcard + version qualifier", []()
    {
        _tst_address_match("9*.V=06", "99999999", MANUFACTURER_ABB, 0x06, 1, true, false);
        _tst_address_match("9*.V=06", "89999999", MANUFACTURER_ABB, 0x06, 1, false, false);
        _tst_address_match("9*.V=06", "99999999", MANUFACTURER_ABB, 0x07, 1, false, false);
        _tst_address_match("!9*.V=06", "99999999", MANUFACTURER_ABB, 0x06, 1, true, true);
        _tst_address_match("!9*.V=06", "89999999", MANUFACTURER_ABB, 0x06, 1, false, true);
        _tst_address_match("!9*.V=06", "99999999", MANUFACTURER_ABB, 0x07, 1, false, true);
        _tst_address_match("!9*.V=06", "89999999", MANUFACTURER_ABB, 0x07, 1, false, true);
    });

    it("telegram match: single and multi-address scenarios", []()
    {
        _tst_telegram_match("12345678", "12345678", true, false);
        _tst_telegram_match("11111111,22222222", "12345678,22*", true, true);
        _tst_telegram_match("11111111,22222222", "12345678,22222222", true, false);
        _tst_telegram_match("11111111.M=KAM,22222222.M=PII", "11111111.M=KAM", true, false);
        _tst_telegram_match("11111111.M=KAF", "11111111.M=KAM", false, false);
    });

    it("telegram match: M/V/T qualifier filtering", []()
    {
        _tst_telegram_match("11111111.M=KAM.V=1b.T=16", "11111111.M=KAM", true, false);
        _tst_telegram_match("11111111.M=KAM.V=1b.T=16", "11111111.M=KAF", false, false);
        _tst_telegram_match("11111111.M=KAM.V=1b.T=16", "11111111", true, false);
        _tst_telegram_match("11111111.M=KAM.V=1b.T=16", "11111111.V=1b", true, false);
        _tst_telegram_match("11111111.M=KAM.V=1b.T=16", "11111111.T=16", true, false);
        _tst_telegram_match("11111111.M=KAM.V=1b.T=16", "11111111.M=KAM.T=16", true, false);
        _tst_telegram_match("11111111.M=KAM.V=1b.T=16", "11111111.M=KAM.V=1b", true, false);
        _tst_telegram_match("11111111.M=KAM.V=1b.T=16", "11111111.T=16.V=1b", true, false);
        _tst_telegram_match("11111111.M=KAM.V=1b.T=16", "11111111.M=KAL", false, false);
        _tst_telegram_match("11111111.M=KAM.V=1b.T=16", "11111111.V=1c", false, false);
        _tst_telegram_match("11111111.M=KAM.V=1b.T=16", "11111111.T=17", false, false);
        _tst_telegram_match("11111111.M=KAM.V=1b.T=16", "11111111.M=KAM.T=17", false, false);
        _tst_telegram_match("11111111.M=KAM.V=1b.T=16", "11111111.M=KAL.V=1b", false, false);
        _tst_telegram_match("11111111.M=KAM.V=1b.T=16", "11111111.T=17.V=1b", false, false);
    });

    it("telegram match: wildcard with negation overrides non-negated match", []()
    {
        _tst_telegram_match("11111111.M=KAM.V=1b.T=16,22222222.M=XXX.V=aa.T=99",
                            "*,!1*.V=1b", false, true);
    });
});

#endif // ADDRESS_TEST_H
