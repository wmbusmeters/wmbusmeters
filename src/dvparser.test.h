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

#ifndef DVPARSER_TEST_H
#define DVPARSER_TEST_H

#include"test.h"
#include"dvparser.h"
#include"wmbus.h"
#include"util.h"
#include"xmq.h"

#include<string>
#include<unordered_map>
#include<utility>
#include<vector>

static bool _tst_parse(const char *data, std::unordered_map<std::string,std::pair<int,DVEntry>> *dv_entries, int testnr)
{
    Telegram t;
    std::vector<uchar> databytes;
    hex2bin(data, &databytes);
    std::vector<uchar>::iterator i = databytes.begin();
    return parseDV(&t, databytes, i, databytes.size(), dv_entries);
}

static bool _tst_ixmlparse(const char *hex, const char *grammar,
                            std::unordered_map<std::string,std::pair<int,DVEntry>> *dv_entries,
                            int testnr)
{
    XMQDoc *ixml = xmqNewDoc();
    bool b = xmqParseBufferWithType(ixml, grammar, NULL, NULL, XMQ_CONTENT_IXML, 0);
    if (!b)
    {
        char buf[512];
        snprintf(buf, sizeof(buf), "test %d: failed to parse IXML grammar: %s", testnr, grammar);
        test_fail(buf);
        return false;
    }
    std::string data(hex);
    Telegram t;
    b = parseWithIXML(&t, 0, data, ixml, dv_entries);
    if (!b)
    {
        char buf[512];
        snprintf(buf, sizeof(buf), "test %d: failed to parse %s using IXML grammar: %s", testnr, hex, grammar);
        test_fail(buf);
    }
    xmqFreeDoc(ixml);
    return b;
}

static void _tst_double(std::unordered_map<std::string,std::pair<int,DVEntry>> &values,
                        const char *key, double v, int testnr)
{
    int offset;
    double value;
    bool b = extractDVdouble(&values, key, &offset, &value);
    if (!b || value != v)
    {
        char buf[512];
        snprintf(buf, sizeof(buf), "test %d: key %s got %lf expected %lf", testnr, key, value, v);
        test_fail(buf);
    }
}

static void _tst_string(std::unordered_map<std::string,std::pair<int,DVEntry>> &values,
                        const char *key, const char *v, int testnr)
{
    int offset;
    std::string value;
    bool b = extractDVHexString(&values, key, &offset, &value);
    if (!b || value != v)
    {
        char buf[512];
        snprintf(buf, sizeof(buf), "test %d: key %s got \"%s\" expected \"%s\"", testnr, key, value.c_str(), v);
        test_fail(buf);
    }
}

static void _tst_date(std::unordered_map<std::string,std::pair<int,DVEntry>> &values,
                      const char *key, std::string date_expected, int testnr)
{
    int offset;
    struct tm value;
    bool b = extractDVdate(&values, key, &offset, &value);
    char buf2[256];
    strftime(buf2, sizeof(buf2), "%Y-%m-%d %H:%M:%S", &value);
    std::string date_got = buf2;
    if (!b || date_got != date_expected)
    {
        char buf[512];
        snprintf(buf, sizeof(buf), "test %d: key %s got >%s< expected >%s<",
                 testnr, key, date_got.c_str(), date_expected.c_str());
        test_fail(buf);
    }
}

static void _tst_no_key(std::unordered_map<std::string,std::pair<int,DVEntry>> &values,
                        const char *key, int testnr)
{
    if (hasKey(&values, key))
    {
        char buf[256];
        snprintf(buf, sizeof(buf), "test %d: key %s should not exist", testnr, key);
        test_fail(buf);
    }
}

static void _tst_subunit(std::unordered_map<std::string,std::pair<int,DVEntry>> &values,
                         const char *key, int expected_subunit, int testnr)
{
    if (!hasKey(&values, key))
    {
        char buf[256];
        snprintf(buf, sizeof(buf), "test %d: key %s does not exist", testnr, key);
        test_fail(buf);
        return;
    }
    int got = values[key].second.subunit_nr.intValue();
    if (got != expected_subunit)
    {
        char buf[256];
        snprintf(buf, sizeof(buf), "test %d: key %s subunit %d but expected %d",
                 testnr, key, got, expected_subunit);
        test_fail(buf);
    }
}

static auto _dvparser_suite = describe("dvparser", []()
{
    it("parses basic numeric, extended and string DV entries", []()
    {
        std::unordered_map<std::string,std::pair<int,DVEntry>> dv_entries;
        int testnr = 1;
        _tst_parse("2F 2F 0B 13 56 34 12 8B 82 00 93 3E 67 45 23 0D FD 10 0A 30 31 32 33 34 35 36 37 38 39 0F 88 2F", &dv_entries, testnr);
        _tst_double(dv_entries, "0B13", 123.456, testnr);
        _tst_double(dv_entries, "8B8200933E", 234.567, testnr);
        _tst_string(dv_entries, "0DFD10", "30313233343536373839", testnr);
    });

    it("parses date entry 2010-12-31", []()
    {
        std::unordered_map<std::string,std::pair<int,DVEntry>> dv_entries;
        int testnr = 2;
        _tst_parse("82046C 5f1C", &dv_entries, testnr);
        _tst_date(dv_entries, "82046C", "2010-12-31 00:00:00", testnr);
    });

    it("parses long telegram with multiple value types and dates", []()
    {
        std::unordered_map<std::string,std::pair<int,DVEntry>> dv_entries;
        int testnr = 3;
        _tst_parse("0C1348550000426CE1F14C130000000082046C21298C0413330000008D04931E3A3CFE3300000033000000330000003300000033000000330000003300000033000000330000003300000033000000330000003300000034180000046D0D0B5C2B03FD6C5E150082206C5C290BFD0F0200018C4079678885238310FD3100000082106C01018110FD610002FD66020002FD170000", &dv_entries, testnr);
        _tst_double(dv_entries, "0C13", 5.548, testnr);
        _tst_date(dv_entries, "426C", "2127-01-01 00:00:00", testnr);
        _tst_date(dv_entries, "82106C", "2000-01-01 00:00:00", testnr);
    });

    it("parses date 2007-04-30", []()
    {
        std::unordered_map<std::string,std::pair<int,DVEntry>> dv_entries;
        int testnr = 4;
        _tst_parse("426C FE04", &dv_entries, testnr);
        _tst_date(dv_entries, "426C", "2007-04-30 00:00:00", testnr);
    });

    it("compact profile: increment mode produces correct history values", []()
    {
        std::unordered_map<std::string,std::pair<int,DVEntry>> dv_entries;
        int testnr = 5;
        _tst_parse("0213E803 0D9313 06 72FE05000700", &dv_entries, testnr);
        _tst_double(dv_entries, "42137F77", 0.995, testnr);
        _tst_double(dv_entries, "8201137F77", 0.988, testnr);
    });

    it("compact profile: decrement mode produces correct history values", []()
    {
        std::unordered_map<std::string,std::pair<int,DVEntry>> dv_entries;
        int testnr = 6;
        _tst_parse("0213E803 0D9313 06 B2FE05000700", &dv_entries, testnr);
        _tst_double(dv_entries, "42137F77", 1.005, testnr);
        _tst_double(dv_entries, "8201137F77", 1.012, testnr);
    });

    it("compact profile: signed-difference mode produces correct history values", []()
    {
        std::unordered_map<std::string,std::pair<int,DVEntry>> dv_entries;
        int testnr = 7;
        _tst_parse("0213E803 0D9313 06 F2FEFBFF0700", &dv_entries, testnr);
        _tst_double(dv_entries, "42137F77", 1.005, testnr);
        _tst_double(dv_entries, "8201137F77", 0.998, testnr);
    });

    it("compact profile: half-month spacing (0xFD + days/month unit)", []()
    {
        std::unordered_map<std::string,std::pair<int,DVEntry>> dv_entries;
        int testnr = 8;
        _tst_parse("0213E803 0D9313 04 72FD0500", &dv_entries, testnr);
        _tst_double(dv_entries, "42137F77", 0.995, testnr);
    });

    it("compact profile: spacing 0 means array mode", []()
    {
        std::unordered_map<std::string,std::pair<int,DVEntry>> dv_entries;
        int testnr = 9;
        _tst_parse("0213E803 0D9313 04 72000500", &dv_entries, testnr);
        _tst_double(dv_entries, "42137F77", 0.995, testnr);
    });

    it("compact profile: reserved spacing 251 rejected", []()
    {
        std::unordered_map<std::string,std::pair<int,DVEntry>> dv_entries;
        int testnr = 10;
        _tst_parse("0213E803 0D9313 04 72FB0500", &dv_entries, testnr);
        _tst_no_key(dv_entries, "42137F77", testnr);
    });

    it("compact profile: invalid half-month combination rejected", []()
    {
        std::unordered_map<std::string,std::pair<int,DVEntry>> dv_entries;
        int testnr = 11;
        _tst_parse("0213E803 0D9313 04 52FD0500", &dv_entries, testnr);
        _tst_no_key(dv_entries, "42137F77", testnr);
    });

    it("compact profile: absolute mode with signed binary values", []()
    {
        std::unordered_map<std::string,std::pair<int,DVEntry>> dv_entries;
        int testnr = 12;
        _tst_parse("0213E803 0D9313 04 32FEFBFF", &dv_entries, testnr);
        _tst_double(dv_entries, "42137F77", -0.005, testnr);
    });

    it("compact profile: invalid delta stops processing", []()
    {
        std::unordered_map<std::string,std::pair<int,DVEntry>> dv_entries;
        int testnr = 13;
        _tst_parse("0213E803 0D9313 08 72FE0500FFFF0700", &dv_entries, testnr);
        _tst_double(dv_entries, "42137F77", 0.995, testnr);
        _tst_no_key(dv_entries, "8201137F77", testnr);
    });

    it("compact profile: signed-difference illegal minimum stops processing", []()
    {
        std::unordered_map<std::string,std::pair<int,DVEntry>> dv_entries;
        int testnr = 14;
        _tst_parse("0213E803 0D9313 08 F2FE010000800700", &dv_entries, testnr);
        _tst_double(dv_entries, "42137F77", 0.999, testnr);
        _tst_no_key(dv_entries, "8201137F77", testnr);
    });

    it("compact profile with day spacing generates history dates", []()
    {
        std::unordered_map<std::string,std::pair<int,DVEntry>> dv_entries;
        int testnr = 15;
        _tst_parse("820413E803 82046C0A31 8D04931306720205000300", &dv_entries, testnr);
        _tst_double(dv_entries, "C204137F77", 0.995, testnr);
        _tst_double(dv_entries, "8205137F77", 0.992, testnr);
        _tst_date(dv_entries, "C2046C7F77", "2024-01-08 00:00:00", testnr);
        _tst_date(dv_entries, "82056C7F77", "2024-01-06 00:00:00", testnr);
    });

    it("compact profile with half-month spacing generates history dates", []()
    {
        std::unordered_map<std::string,std::pair<int,DVEntry>> dv_entries;
        int testnr = 16;
        _tst_parse("0213E803 026C1031 0D9313 06 72FD05000300", &dv_entries, testnr);
        _tst_double(dv_entries, "42137F77", 0.995, testnr);
        _tst_double(dv_entries, "8201137F77", 0.992, testnr);
        _tst_date(dv_entries, "426C7F77", "2024-01-01 00:00:00", testnr);
        _tst_date(dv_entries, "82016C7F77", "2023-12-16 00:00:00", testnr);
    });

    it("compact profile with register numbers maps to correct forward storage slots", []()
    {
        std::unordered_map<std::string,std::pair<int,DVEntry>> dv_entries;
        int testnr = 17;
        _tst_parse("820413E803 82046C1031 8D04931E0632FEE903EA03", &dv_entries, testnr);
        _tst_double(dv_entries, "C204137F77", 1.001, testnr);
        _tst_double(dv_entries, "8205137F77", 1.002, testnr);
        _tst_date(dv_entries, "C2046C7F77", "2024-02-16 00:00:00", testnr);
        _tst_date(dv_entries, "82056C7F77", "2024-03-16 00:00:00", testnr);
    });

    it("compact profile: array mode with column addressing sets subunit offset", []()
    {
        std::unordered_map<std::string,std::pair<int,DVEntry>> dv_entries;
        int testnr = 18;
        _tst_parse("0213E803 0D9313 04 12000500", &dv_entries, testnr);
        _tst_double(dv_entries, "42137F77", 0.005, testnr);
        _tst_subunit(dv_entries, "42137F77", 1, testnr);
    });
});

static auto _ixmlparser_suite = describe("ixmlparser", []()
{
    it("parses volume value via IXML grammar", []()
    {
        std::unordered_map<std::string,std::pair<int,DVEntry>> dv_entries;
        int testnr = 1;
        _tst_ixmlparse("10351F0400",
                       "decode = total. -hex  = ['A'-'F';'0'-'9']. -byte = hex, hex. -word = byte, byte. -quad = byte, byte, byte, byte. total     = -'10', quad, @DV_0413. DV_0413>dvk=+'0413'.",
                       &dv_entries, testnr);
        _tst_double(dv_entries, "0413", 270.133, testnr);
    });
});

static auto _dvs_suite = describe("dvs", []()
{
    it("DifVifKey parses DIF and VIF correctly", []()
    {
        DifVifKey dvk("0B2B");
        assert(dvk.dif() == 0x0b);
        assert(dvk.vif() == 0x2b);
        assert(!dvk.hasDifes());
        assert(!dvk.hasVifes());
    });

    it("vifeType returns correct description for third extension table", []()
    {
        std::string third = vifeType(0, 0xef, 0x01);
        assert(third == "Reserved for future third extension table");
    });

    it("vifeType: high continuation bit does not affect VIFE code", []()
    {
        std::string third_cont = vifeType(0, 0xef, 0x81);
        assert(third_cont == "Reserved for future third extension table");
    });

    it("vifeType returns manufacturer specific for 0xff/0x22", []()
    {
        std::string mfct = vifeType(0, 0xff, 0x22);
        assert(mfct == "Manufacturer specific");
    });
});

static auto _field_matcher_suite = describe("field_matcher", []()
{
    it("matches entry with MeasurementType and VIFRange", []()
    {
        FieldMatcher m1 = FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::Volume);

        std::string v1 = "2F4E0000";
        DVEntry e1(0, DifVifKey("0413"), MeasurementType::Instantaneous,
                   Vif(0x13), { }, { }, StorageNr(0), TariffNr(0), SubUnitNr(0), v1);
        assert(m1.matches(e1));
    });

    it("matches entry with StorageNr and VIFCombinable::Any", []()
    {
        FieldMatcher m2 = FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(StorageNr(2))
            .set(VIFRange::Volume)
            .add(VIFCombinable::Any);

        std::string v2 = "03";
        DVEntry e2(0, DifVifKey("810110FC0C"), MeasurementType::Instantaneous,
                   Vif(0x10), { VIFCombinable::DeltaBetweenImportAndExport }, { },
                   StorageNr(2), TariffNr(0), SubUnitNr(0), v2);
        assert(m2.matches(e2));
    });

    it("matches entry with specific VIFCombinable", []()
    {
        FieldMatcher m3 = FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(StorageNr(2))
            .set(VIFRange::Volume)
            .add(VIFCombinable::DeltaBetweenImportAndExport);

        std::string v2 = "03";
        DVEntry e2(0, DifVifKey("810110FC0C"), MeasurementType::Instantaneous,
                   Vif(0x10), { VIFCombinable::DeltaBetweenImportAndExport }, { },
                   StorageNr(2), TariffNr(0), SubUnitNr(0), v2);
        assert(m3.matches(e2));
    });

    it("does not match entry with wrong VIFCombinable", []()
    {
        FieldMatcher m4 = FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(StorageNr(2))
            .set(VIFRange::Volume)
            .add(VIFCombinable::ValueDuringUpperLimitExceeded);

        std::string v2 = "03";
        DVEntry e2(0, DifVifKey("810110FC0C"), MeasurementType::Instantaneous,
                   Vif(0x10), { VIFCombinable::DeltaBetweenImportAndExport }, { },
                   StorageNr(2), TariffNr(0), SubUnitNr(0), v2);
        assert(!m4.matches(e2));
    });
});

#endif // DVPARSER_TEST_H
