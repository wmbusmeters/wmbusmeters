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

#ifndef FORMULA_TEST_H
#define FORMULA_TEST_H

#include"test.h"
#include"formula_implementation.h"
#include"meters.h"
#include"util.h"

#include<ctime>
#include<memory>
#include<string>
#include<vector>

static void _test_formula_tree(FormulaImplementation *f, Meter *m,
                                std::string formula, std::string tree)
{
    f->clear();
    f->parse(m, formula);
    std::string t = f->tree();
    if (t != tree)
    {
        char buf[512];
        snprintf(buf, sizeof(buf),
                 "parsing \"%s\": expected tree\n  %s\nbut got\n  %s",
                 formula.c_str(), tree.c_str(), t.c_str());
        test_fail(buf);
    }
}

static void _test_formula_value(FormulaImplementation *f, Meter *m,
                                 std::string formula, double val, Unit unit)
{
    f->clear();
    bool ok = f->parse(m, formula);
    assert(ok);
    double v = f->calculate(unit);
    if (v != val)
    {
        char buf[256];
        snprintf(buf, sizeof(buf),
                 "evaluating \"%s\": expected %.17g but got %.17g",
                 formula.c_str(), val, v);
        test_fail(buf);
    }
}

static void _test_formula_error(FormulaImplementation *f, Meter *m,
                                 std::string formula, Unit unit, std::string errors)
{
    f->clear();
    bool ok = f->parse(m, formula);
    std::string es = f->errors();
    if (es != errors)
    {
        char buf[512];
        snprintf(buf, sizeof(buf),
                 "parsing \"%s\": expected errors:\n%sbut got:\n%s",
                 formula.c_str(), errors.c_str(), es.c_str());
        test_fail(buf);
    }
    assert(!ok);
}

static double _totime(int year, int month = 1, int day = 1,
                      int hour = 0, int min = 0, int sec = 0)
{
    struct tm date {};
    date.tm_year = year - 1900;
    date.tm_mon  = month - 1;
    date.tm_mday = day;
    date.tm_hour = hour;
    date.tm_min  = min;
    date.tm_sec  = sec;
    return (double)mktime(&date);
}

static void _test_datetime(FormulaImplementation *f, std::string formula,
                            int year, int month = 1, int day = 1,
                            int hour = 0, int min = 0, int sec = 0)
{
    f->clear();
    double expected = _totime(year, month, day, hour, min, sec);
    f->parse(NULL, formula);
    double v = f->calculate(Unit::UnixTimestamp);
    if (v != expected)
    {
        char buf[256];
        snprintf(buf, sizeof(buf),
                 "datetime \"%s\": expected %.17g (%04d-%02d-%02d %02d:%02d:%02d) but got %.17g",
                 formula.c_str(), expected, year, month, day, hour, min, sec, v);
        test_fail(buf);
    }
}

static void _test_time(FormulaImplementation *f, std::string formula,
                       int hour = 0, int min = 0, int sec = 0)
{
    f->clear();
    double expected = hour*3600 + min*60 + sec;
    f->parse(NULL, formula);
    double v = f->calculate(Unit::Second);
    if (v != expected)
    {
        char buf[128];
        snprintf(buf, sizeof(buf),
                 "time \"%s\": expected %.17g (%02d:%02d:%02d) but got %.17g",
                 formula.c_str(), expected, hour, min, sec, v);
        test_fail(buf);
    }
}

static auto _formulas_building_consts_suite = describe("formulas_building_consts", []()
{
    it("adds and converts constants in basic formula operations", []()
    {
        auto f = std::unique_ptr<FormulaImplementation>(new FormulaImplementation());

        f->doConstant(Unit::KWH, 17);
        f->doConstant(Unit::KWH, 1);
        f->doAddition(SI_KWH);
        assert(f->calculate(Unit::KWH) == 18.0);

        f->clear();
        f->doConstant(Unit::KWH, 10);
        assert(f->calculate(Unit::MJ) == 36.0);

        f->clear();
        f->doConstant(Unit::GJ, 10);
        f->doConstant(Unit::MJ, 10);
        f->doAddition(SI_GJ);
        assert(f->calculate(Unit::GJ) == 10.01);

        f->clear();
        f->doConstant(Unit::C, 10);
        f->doConstant(Unit::C, 20);
        f->doAddition(SI_C);
        f->doConstant(Unit::C, 22);
        f->doAddition(SI_C);
        assert(f->calculate(Unit::C) == 52);

        f->clear();
        f->doConstant(Unit::Month, 2);
        f->doConstant(Unit::COUNTER, 3);
        f->doMultiplication();
        assert(f->calculate(Unit::Month) == 6);

        f->clear();
        f->doConstant(Unit::UnixTimestamp, 3600*24*11);
        f->doConstant(Unit::Second, 9);
        f->doAddition(SIUnit(Unit::UnixTimestamp));
        assert(f->calculate(Unit::UnixTimestamp) == (double)(3600*24*11+9));

        f->clear();
        f->doConstant(Unit::UnixTimestamp, 3600*24*11);
        f->doConstant(Unit::Month, 1);
        f->doAddition(SIUnit(Unit::UnixTimestamp));
        assert(f->calculate(Unit::UnixTimestamp) == (double)(3600*24*(31+11)));
    });
});

static auto _formulas_building_meters_suite = describe("formulas_building_meters", []()
{
    it("reads meter field values via FormulaImplementation", []()
    {
        {
            MeterInfo mi;
            assert(lookupDriverInfo("multical21"));
            mi.parse("testur", "multical21", "12345678", "");
            auto meter = createMeter(&mi);
            FieldInfo *fi_flow = meter->findFieldInfo("flow_temperature", Quantity::Temperature);
            FieldInfo *fi_ext  = meter->findFieldInfo("min_external_temperature_last_month", Quantity::Temperature);
            assert(fi_flow != NULL);
            assert(fi_ext  != NULL);

            std::vector<uchar> frame;
            hex2bin("2a442d2c785634121B168d2091d37cac217f2d7802ff207100041308190000441308190000615B1f616713", &frame);
            Telegram t;
            MeterKeys mk;
            t.parse(frame, &mk, true);
            std::vector<Address> addresses;
            bool match;
            meter->handleTelegram(t.about, frame, true, &addresses, &match, &t);

            auto f = std::unique_ptr<FormulaImplementation>(new FormulaImplementation());
            f->setMeter(meter.get());
            f->doMeterField(Unit::C, fi_flow);
            assert(f->calculate(Unit::C) == 31);

            f->clear();
            f->setMeter(meter.get());
            f->doMeterField(Unit::C, fi_flow);
            f->doMeterField(Unit::C, fi_ext);
            f->doAddition(SIUnit(Unit::C));
            assert(f->calculate(Unit::C) == 50);
        }

        {
            MeterInfo mi;
            mi.parse("testur", "ebzwmbe", "22992299", "");
            auto meter = createMeter(&mi);
            FieldInfo *fi_p1 = meter->findFieldInfo("current_power_consumption_phase1", Quantity::Power);
            FieldInfo *fi_p2 = meter->findFieldInfo("current_power_consumption_phase2", Quantity::Power);
            FieldInfo *fi_p3 = meter->findFieldInfo("current_power_consumption_phase3", Quantity::Power);
            assert(fi_p1 && fi_p2 && fi_p3);

            std::vector<uchar> frame;
            hex2bin("5B445a149922992202378c20f6900f002c25Bc9e0000BBBBBBBBBBBBBBBB72992299225a140102f6003007102f2f040330f92a0004a9ff01ff24000004a9ff026a29000004a9ff03460600000dfd11063132333435362f2f2f2f2f2f", &frame);
            Telegram t;
            MeterKeys mk;
            t.parse(frame, &mk, true);
            std::vector<Address> id;
            bool match;
            meter->handleTelegram(t.about, frame, true, &id, &match, &t);

            auto f = std::unique_ptr<FormulaImplementation>(new FormulaImplementation());
            f->setMeter(meter.get());
            f->doMeterField(Unit::KW, fi_p1);
            f->doMeterField(Unit::KW, fi_p2);
            f->doAddition(SI_KW);
            f->doMeterField(Unit::KW, fi_p3);
            f->doAddition(SI_KW);
            assert(f->calculate(Unit::KW) == 0.21679);
        }
    });
});

static auto _formulas_datetimes_suite = describe("formulas_datetimes", []()
{
    it("parses date/time literals and arithmetic expressions", []()
    {
        auto f = std::unique_ptr<FormulaImplementation>(new FormulaImplementation());
        _test_datetime(f.get(), "'2022-02-02'", 2022, 2, 2);
        _test_datetime(f.get(), "'2021-02-28'", 2021, 2, 28);
        _test_datetime(f.get(), "'1970-01-01 01:00:00'", 1970, 1, 1, 1, 0, 0);
        _test_datetime(f.get(), "'1970-01-01 01:00'", 1970, 1, 1, 1, 0);
        _test_datetime(f.get(), "'1970-01-01'", 1970, 1, 1);
        _test_time(f.get(), "'00:15'", 0, 15, 0);
        _test_time(f.get(), "'00:00:16'", 0, 0, 16);
        _test_datetime(f.get(), "'2022-01-01 00:00:00' + 1s", 2022, 1, 1, 0, 0, 1);
        _test_datetime(f.get(), "'1971-10-01 02:17' +7d+1h+2min+1s", 1971, 10, 8, 3, 19, 1);
        _test_datetime(f.get(), "'2000-01-01' + 1month", 2000, 2, 1);
        _test_datetime(f.get(), "'2020-12-31' + 2month", 2021, 2, 28);
        _test_datetime(f.get(), "'2020-12-31' - 10month", 2020, 2, 29);
        _test_datetime(f.get(), "'2021-01-31' - 1month", 2020, 12, 31);
        _test_datetime(f.get(), "'2021-01-31' - 2month", 2020, 11, 30);
        _test_datetime(f.get(), "'2021-01-31' - 24month", 2019, 1, 31);
        _test_datetime(f.get(), "'2021-01-31' + 24month", 2023, 1, 31);
        _test_datetime(f.get(), "'2021-01-31' + 22month", 2022, 11, 30);
        _test_datetime(f.get(), "'2021-01-31' + 2y", 2023, 1, 31);
        _test_datetime(f.get(), "'2000-01-01' + 24y + 5month + 3d", 2024, 6, 4);
        _test_datetime(f.get(), "'2021-02-28' -12month", 2020, 2, 29);
        _test_datetime(f.get(), "'2001-02-28' -12month", 2000, 2, 29);
        _test_datetime(f.get(), "'2000-02-29' +(12month * 100counter)", 2100, 2, 28);
    });
});

static auto _formulas_parsing_1_suite = describe("formulas_parsing_1", []()
{
    it("parses arithmetic, field references, and tree structure", []()
    {
        MeterInfo mi;
        mi.parse("testur", "ebzwmbe", "22992299", "");
        auto meter = createMeter(&mi);

        std::vector<uchar> frame;
        hex2bin("5B445a149922992202378c20f6900f002c25Bc9e0000BBBBBBBBBBBBBBBB72992299225a140102f6003007102f2f040330f92a0004a9ff01ff24000004a9ff026a29000004a9ff03460600000dfd11063132333435362f2f2f2f2f2f", &frame);
        Telegram t;
        MeterKeys mk;
        t.parse(frame, &mk, true);
        std::vector<Address> id;
        bool match;
        meter->handleTelegram(t.about, frame, true, &id, &match, &t);

        auto f = std::unique_ptr<FormulaImplementation>(new FormulaImplementation());
        _test_formula_value(f.get(), meter.get(), "10 kwh + 100 kwh", 110, Unit::KWH);
        _test_formula_value(f.get(), meter.get(),
                            "current_power_consumption_phase1_kw + "
                            "current_power_consumption_phase2_kw + "
                            "current_power_consumption_phase3_kw + 100 kw",
                            100.21679, Unit::KW);
        _test_formula_tree(f.get(), meter.get(),
                           "5 c + 7 c + 10 c",
                           "<ADD <ADD <CONST 5 c[1c]Temperature> <CONST 7 c[1c]Temperature> > <CONST 10 c[1c]Temperature> >");
        _test_formula_tree(f.get(), meter.get(),
                           "(5 c + 7 c) + 10 c",
                           "<ADD <ADD <CONST 5 c[1c]Temperature> <CONST 7 c[1c]Temperature> > <CONST 10 c[1c]Temperature> >");
        _test_formula_tree(f.get(), meter.get(),
                           "5 c + (7 c + 10 c)",
                           "<ADD <CONST 5 c[1c]Temperature> <ADD <CONST 7 c[1c]Temperature> <CONST 10 c[1c]Temperature> > >");
        _test_formula_tree(f.get(), meter.get(),
                           "sqrt(22 m * 22 m)",
                           "<SQRT <TIMES <CONST 22 m[1m]Length> <CONST 22 m[1m]Length> > >");
    });
});

static auto _formulas_parsing_2_suite = describe("formulas_parsing_2", []()
{
    it("parses field reference from em24 driver", []()
    {
        MeterInfo mi;
        mi.parse("testur", "em24", "66666666", "");
        auto meter = createMeter(&mi);

        std::vector<uchar> frame;
        hex2bin("35442D2C6666666633028D2070806A0520B4D378_0405F208000004FB82753F00000004853C0000000004FB82F53CCA01000001FD1722", &frame);
        Telegram t;
        MeterKeys mk;
        t.parse(frame, &mk, true);
        std::vector<Address> id;
        bool match;
        meter->handleTelegram(t.about, frame, true, &id, &match, &t);

        auto f = std::unique_ptr<FormulaImplementation>(new FormulaImplementation());
        _test_formula_value(f.get(), meter.get(), "total_energy_consumption_kwh + 18 kwh", 247, Unit::KWH);
    });
});

static auto _formulas_multiply_constants_suite = describe("formulas_multiply_constants", []()
{
    it("multiplies constants with compatible unit products", []()
    {
        FormulaImplementation fi;
        _test_formula_value(&fi, NULL, "100.5 counter * 22 kwh", 2211, Unit::KWH);
        _test_formula_value(&fi, NULL, "5 kw * 10 h", 50, Unit::KWH);
        _test_formula_value(&fi, NULL, "5000 v * 10 a * 700 h", 35000, Unit::KVAH);
    });
});

static auto _formulas_divide_constants_suite = describe("formulas_divide_constants", []()
{
    it("divides constants with compatible unit quotients", []()
    {
        FormulaImplementation fi;
        _test_formula_value(&fi, NULL, "22 kwh / 11 h", 2, Unit::KW);
    });
});

static auto _formulas_sqrt_constants_suite = describe("formulas_sqrt_constants", []()
{
    it("computes square roots of unit products", []()
    {
        FormulaImplementation fi;
        _test_formula_value(&fi, NULL, "sqrt(22 m * 22 m)", 22, Unit::M);
        _test_formula_value(&fi, NULL, "sqrt((2 kwh * 2 kwh) + (3 kvarh * 3 kvarh))",
                            3.6055512754639891, Unit::KVAH);
    });
});

static auto _formulas_errors_suite = describe("formulas_errors", []()
{
    it("reports type-mismatch error for incompatible unit addition", []()
    {
        MeterInfo mi;
        mi.parse("testur", "em24", "66666666", "");
        auto meter = createMeter(&mi);
        auto formula = std::unique_ptr<FormulaImplementation>(new FormulaImplementation());
        _test_formula_error(formula.get(), meter.get(),
                            "10 kwh + 20 kw", Unit::KWH,
                            "Cannot add [kwh|Energy|3.6×10⁶kgm²s⁻²] to [kw|Power|1000kgm²s⁻³]!\n"
                            "10 kwh + 20 kw\n"
                            "       ^~~~~\n");
    });
});

static auto _formulas_dventries_suite = describe("formulas_dventries", []()
{
    it("evaluates storage/tariff/subunit counter expressions against DVEntry", []()
    {
        DVEntry dve;
        dve.storage_nr = 17;
        dve.tariff_nr  = 3;
        dve.subunit_nr = 2;

        auto f = std::unique_ptr<FormulaImplementation>(new FormulaImplementation());

        std::string s = "(storage_counter - 12 counter) *  tariff_counter - subunit_counter";
        f->parse(NULL, s);
        f->setDVEntry(&dve);
        assert(f->calculate(Unit::COUNTER) == 13.0);

        dve.storage_nr = 18;
        dve.tariff_nr  = 0;
        dve.subunit_nr = 0;
        s = "(storage_counter - 8counter) / 2counter";
        f->parse(NULL, s);
        f->setDVEntry(&dve);
        assert(f->calculate(Unit::COUNTER) == 5.0);
    });
});

static auto _formulas_stringinterpolation_suite = describe("formulas_stringinterpolation", []()
{
    it("interpolates formula expressions inside field name templates", []()
    {
        DVEntry dve;
        dve.storage_nr = 17;
        dve.tariff_nr  = 3;
        dve.subunit_nr = 2;

        auto f = std::unique_ptr<StringInterpolator>(new StringInterpolatorImplementation());

        std::string p = "history_{storage_counter-12counter}_value";
        f->parse(NULL, p);
        assert(f->apply(NULL, &dve) == "history_5_value");

        p = "{storage_counter}_{tariff_counter}_{2counter*subunit_counter}";
        f->parse(NULL, p);
        assert(f->apply(NULL, &dve) == "17_3_4");
    });
});

static auto _formulas_rounding_suite = describe("formulas_rounding", []()
{
    it("round() rounds to nearest integer in the unit", []()
    {
        FormulaImplementation fi;
        _test_formula_value(&fi, NULL, "round(100.542 kwh)", 101, Unit::KWH);
        _test_formula_value(&fi, NULL, "round(100.492 kwh)", 100, Unit::KWH);
    });
});

static auto _formulas_extended_ops_suite = describe("formulas_extended_ops", []()
{
    it("supports modulo, exponent, shift, bitwise and logical operators", []()
    {
        FormulaImplementation fi;
        _test_formula_value(&fi, NULL, "7counter % 4counter",  3, Unit::COUNTER);
        _test_formula_value(&fi, NULL, "2counter ** 3counter", 8, Unit::COUNTER);
        _test_formula_value(&fi, NULL, "3counter << 4counter", 48, Unit::COUNTER);
        _test_formula_value(&fi, NULL, "48counter >> 4counter", 3, Unit::COUNTER);
        _test_formula_value(&fi, NULL, "floor(100.999 kwh)", 100, Unit::KWH);
        _test_formula_value(&fi, NULL, "ceil(100.001 kwh)",  101, Unit::KWH);
        _test_formula_value(&fi, NULL, "3counter == 3counter", 1, Unit::COUNTER);
        _test_formula_value(&fi, NULL, "3counter == 4counter", 0, Unit::COUNTER);
        _test_formula_value(&fi, NULL, "3counter != 4counter", 1, Unit::COUNTER);
        _test_formula_value(&fi, NULL, "3counter != 3counter", 0, Unit::COUNTER);
        _test_formula_value(&fi, NULL, "2counter <  3counter", 1, Unit::COUNTER);
        _test_formula_value(&fi, NULL, "3counter <  3counter", 0, Unit::COUNTER);
        _test_formula_value(&fi, NULL, "4counter <  3counter", 0, Unit::COUNTER);
        _test_formula_value(&fi, NULL, "3counter >  2counter", 1, Unit::COUNTER);
        _test_formula_value(&fi, NULL, "3counter >  3counter", 0, Unit::COUNTER);
        _test_formula_value(&fi, NULL, "2counter >  3counter", 0, Unit::COUNTER);
        _test_formula_value(&fi, NULL, "3counter <= 3counter", 1, Unit::COUNTER);
        _test_formula_value(&fi, NULL, "2counter <= 3counter", 1, Unit::COUNTER);
        _test_formula_value(&fi, NULL, "4counter <= 3counter", 0, Unit::COUNTER);
        _test_formula_value(&fi, NULL, "3counter >= 3counter", 1, Unit::COUNTER);
        _test_formula_value(&fi, NULL, "3counter >= 2counter", 1, Unit::COUNTER);
        _test_formula_value(&fi, NULL, "2counter >= 3counter", 0, Unit::COUNTER);
        _test_formula_value(&fi, NULL, "12counter &  10counter",  8, Unit::COUNTER);
        _test_formula_value(&fi, NULL, "12counter |  10counter", 14, Unit::COUNTER);
        _test_formula_value(&fi, NULL, "12counter ^  10counter",  6, Unit::COUNTER);
        _test_formula_value(&fi, NULL, "1counter && 1counter", 1, Unit::COUNTER);
        _test_formula_value(&fi, NULL, "1counter && 0counter", 0, Unit::COUNTER);
        _test_formula_value(&fi, NULL, "0counter && 1counter", 0, Unit::COUNTER);
        _test_formula_value(&fi, NULL, "0counter && 0counter", 0, Unit::COUNTER);
        _test_formula_value(&fi, NULL, "1counter || 0counter", 1, Unit::COUNTER);
        _test_formula_value(&fi, NULL, "0counter || 1counter", 1, Unit::COUNTER);
        _test_formula_value(&fi, NULL, "0counter || 0counter", 0, Unit::COUNTER);
        _test_formula_value(&fi, NULL, "1counter || 1counter", 1, Unit::COUNTER);
        _test_formula_value(&fi, NULL,
                            "(4counter < 12counter) || ((4counter == 12counter) && (27counter < 31counter))",
                            1, Unit::COUNTER);
        _test_formula_value(&fi, NULL,
                            "(12counter < 12counter) || ((12counter == 12counter) && (31counter < 31counter))",
                            0, Unit::COUNTER);
        _test_formula_value(&fi, NULL,
                            "(12counter < 12counter) || ((12counter == 12counter) && (31counter < 15counter))",
                            0, Unit::COUNTER);
    });
});

#endif // FORMULA_TEST_H
