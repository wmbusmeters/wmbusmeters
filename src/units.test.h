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

#ifndef UNITS_TEST_H
#define UNITS_TEST_H

#include"test.h"
#include"units.h"

#include<set>
#include<string>

static void _test_unit(std::string in, bool expected_ok, std::string expected_vname, Unit expected_unit)
{
    Unit unit;
    std::string vname;
    bool ok = extractUnit(in, &vname, &unit);
    if (ok != expected_ok || vname != expected_vname || unit != expected_unit)
    {
        char buf[256];
        snprintf(buf, sizeof(buf),
                 "extractUnit(\"%s\"): expected ok=%d vname=%s unit=%s but got ok=%d vname=%s unit=%s",
                 in.c_str(),
                 expected_ok, expected_vname.c_str(), unitToStringUpperCase(expected_unit).c_str(),
                 ok, vname.c_str(), unitToStringUpperCase(unit).c_str());
        test_fail(buf);
    }
}

static void _test_expected_failed_si_convert(Unit from_unit, Unit to_unit, Quantity q)
{
    SIUnit from_si_unit(from_unit);
    SIUnit to_si_unit(to_unit);
    if (q != from_si_unit.quantity() || q != to_si_unit.quantity())
    {
        test_fail("not the expected quantities");
    }
    if (from_si_unit.convertTo(0, to_si_unit, NULL))
    {
        char buf[128];
        snprintf(buf, sizeof(buf), "should not be able to convert from %s to %s",
                 unitToStringLowerCase(from_si_unit.asUnit()).c_str(),
                 unitToStringLowerCase(to_si_unit.asUnit()).c_str());
        test_fail(buf);
    }
}

static void _test_si_convert(double from_value, double expected_value,
                              Unit from_unit, std::string expected_from_unit,
                              Unit to_unit,   std::string expected_to_unit,
                              Quantity q,
                              std::set<Unit> *from_set,
                              std::set<Unit> *to_set)
{
    SIUnit from_si_unit(from_unit);
    SIUnit to_si_unit(to_unit);
    std::string fu = unitToStringLowerCase(from_si_unit.asUnit(q));
    std::string tu = unitToStringLowerCase(to_si_unit.asUnit(q));

    from_set->erase(from_unit);
    to_set->erase(to_unit);

    double e {};
    from_si_unit.convertTo(from_value, to_si_unit, &e);

    std::string evs = tostrprintf("%.15g", expected_value);
    std::string es  = tostrprintf("%.15g", e);

    if (canConvert(from_unit, to_unit))
    {
        double ee = convert(from_value, from_unit, to_unit);
        std::string ees = tostrprintf("%.15g", ee);
        if (es != ees)
        {
            char buf[256];
            snprintf(buf, sizeof(buf),
                     "SI convert %.15g (%s) from %.15g differs from unit convert %.15g (%s)",
                     e, es.c_str(), from_value, ee, ees.c_str());
            test_fail(buf);
        }
    }
    if (fu != expected_from_unit)
    {
        char buf[128];
        snprintf(buf, sizeof(buf), "expected from unit %s but got %s (si: %s)",
                 expected_from_unit.c_str(), fu.c_str(), from_si_unit.str().c_str());
        test_fail(buf);
    }
    if (tu != expected_to_unit)
    {
        char buf[128];
        snprintf(buf, sizeof(buf), "expected to unit %s but got %s (si: %s)",
                 expected_to_unit.c_str(), tu.c_str(), to_si_unit.str().c_str());
        test_fail(buf);
    }
    if (es != evs)
    {
        char buf[256];
        snprintf(buf, sizeof(buf),
                 "expected %.17g [%s] but got %.17g [%s] converting %.17g from %s (%s) to %s (%s)",
                 expected_value, evs.c_str(), e, es.c_str(), from_value,
                 from_si_unit.str().c_str(), fu.c_str(),
                 to_si_unit.str().c_str(), tu.c_str());
        test_fail(buf);
    }
}

static void _fill_with_units_from(Quantity q, std::set<Unit> *s)
{
    s->clear();
#define X(cname,lcname,hrname,quantity,explanation) if (q == Quantity::quantity) s->insert(Unit::cname);
LIST_OF_UNITS
#undef X
}

static void _check_units_tested(std::set<Unit> &from_set, std::set<Unit> &to_set, Quantity q)
{
    if (!from_set.empty())
    {
        std::string msg = std::string("not all source units in ") + toString(q) + " tested:";
        for (Unit u : from_set) msg += " " + unitToStringLowerCase(u);
        test_fail(msg.c_str());
    }
    if (!to_set.empty())
    {
        std::string msg = std::string("not all target units in ") + toString(q) + " tested:";
        for (Unit u : to_set) msg += " " + unitToStringLowerCase(u);
        test_fail(msg.c_str());
    }
}

static auto _units_extraction_suite = describe("units_extraction", []()
{
    it("extractUnit parses unit suffixes from field names", []()
    {
        _test_unit("total_kwh", true, "total", Unit::KWH);
        _test_unit("total_", false, "", Unit::Unknown);
        _test_unit("total", false, "", Unit::Unknown);
        _test_unit("", false, "", Unit::Unknown);
        _test_unit("_c", false, "", Unit::Unknown);
        _test_unit("work__c", true, "work_", Unit::C);
        _test_unit("water_c", true, "water", Unit::C);
        _test_unit("walk_counter", true, "walk", Unit::COUNTER);
        _test_unit("work_kvarh", true, "work", Unit::KVARH);
        _test_unit("now_mjh", true, "now", Unit::MJH);
        _test_unit("current_power_consumption_phase1_kw", true,
                   "current_power_consumption_phase1", Unit::KW);
    });
});

static auto _si_units_siexp_suite = describe("si_units_siexp", []()
{
    it("SIExp builds and formats exponent strings correctly", []()
    {
        SIExp e = SIExp::build().s(-1).m(3);
        assert(e.str() == "m³s⁻¹");

        SIExp f = SIExp::build().s(1);
        assert(f.str() == "s");

        SIExp g = e.mul(f);
        assert(g.str() == "m³");

        SIExp h = SIExp::build().s(127);
        SIExp i = h.mul(f);
        assert(i.str() == "!s⁻¹²⁸-Invalid!");

        SIExp j = e.div(e);
        assert(j.str() == "");

        SIExp bad = SIExp::build().k(1).c(1);
        assert(bad.str() == "!kc-Invalid!");
    });
});

static auto _si_units_basic_suite = describe("si_units_basic", []()
{
    it("SIUnit constructs kWh and Celsius from scratch and from enum", []()
    {
        SIUnit kwh(Quantity::Energy, 3.6E6, SIExp().kg(1).m(2).s(-2));
        std::string expected = "3.6×10⁶kgm²s⁻²";
        assert(kwh.str() == expected);

        SIUnit kwh2(Unit::KWH);
        assert(kwh2.str() == expected);

        SIUnit celsius(Quantity::Temperature, 1, SIExp().c(1));
        assert(celsius.str() == "1c");

        SIUnit celsius2(Unit::C);
        assert(celsius2.str() == "1c");
    });
});

static auto _si_units_conversion_suite = describe("si_units_conversion", []()
{
    it("Time: s, min, h, d, month, y", []()
    {
        std::set<Unit> from, to;
        _fill_with_units_from(Quantity::Time, &from);
        _fill_with_units_from(Quantity::Time, &to);
        _test_si_convert(60.0, 1.0, Unit::Second, "s", Unit::Minute, "min", Quantity::Time, &from, &to);
        _test_si_convert(3600.0, 1.0, Unit::Second, "s", Unit::Hour, "h", Quantity::Time, &from, &to);
        _test_si_convert(3600.0, 0.041666666666666664, Unit::Second, "s", Unit::Day, "d", Quantity::Time, &from, &to);
        _test_si_convert(3600.0, 1.0/24.0, Unit::Second, "s", Unit::Day, "d", Quantity::Time, &from, &to);
        _test_si_convert(1.0, 60.0, Unit::Minute, "min", Unit::Second, "s", Quantity::Time, &from, &to);
        _test_si_convert(1.0, 24, Unit::Day, "d", Unit::Hour, "h", Quantity::Time, &from, &to);
        _test_si_convert(1.0, 1.0, Unit::Month, "month", Unit::Month, "month", Quantity::Time, &from, &to);
        _test_si_convert(1.0, 1.0, Unit::Year, "y", Unit::Year, "y", Quantity::Time, &from, &to);
        _test_si_convert(100.0, 100.0/24.0, Unit::Hour, "h", Unit::Day, "d", Quantity::Time, &from, &to);
        _check_units_tested(from, to, Quantity::Time);
    });

    it("Length: m", []()
    {
        std::set<Unit> from, to;
        _fill_with_units_from(Quantity::Length, &from);
        _fill_with_units_from(Quantity::Length, &to);
        _test_si_convert(111.1, 111.1, Unit::M, "m", Unit::M, "m", Quantity::Length, &from, &to);
        _check_units_tested(from, to, Quantity::Length);
    });

    it("Mass: kg", []()
    {
        std::set<Unit> from, to;
        _fill_with_units_from(Quantity::Mass, &from);
        _fill_with_units_from(Quantity::Mass, &to);
        _test_si_convert(222.1, 222.1, Unit::KG, "kg", Unit::KG, "kg", Quantity::Mass, &from, &to);
        _check_units_tested(from, to, Quantity::Mass);
    });

    it("Amperage: a", []()
    {
        std::set<Unit> from, to;
        _fill_with_units_from(Quantity::Amperage, &from);
        _fill_with_units_from(Quantity::Amperage, &to);
        _test_si_convert(999.9, 999.9, Unit::Ampere, "a", Unit::Ampere, "a", Quantity::Amperage, &from, &to);
        _check_units_tested(from, to, Quantity::Amperage);
    });

    it("Temperature: c, k, f", []()
    {
        std::set<Unit> from, to;
        _fill_with_units_from(Quantity::Temperature, &from);
        _fill_with_units_from(Quantity::Temperature, &to);
        _test_si_convert(0, 273.15, Unit::C, "c", Unit::K, "k", Quantity::Temperature, &from, &to);
        _test_si_convert(10.85, 284.0, Unit::C, "c", Unit::K, "k", Quantity::Temperature, &from, &to);
        _test_si_convert(100.0, -173.15, Unit::K, "k", Unit::C, "c", Quantity::Temperature, &from, &to);
        _test_si_convert(100.0, -279.67, Unit::K, "k", Unit::F, "f", Quantity::Temperature, &from, &to);
        _test_si_convert(100.0, 37.77777777777777, Unit::F, "f", Unit::C, "c", Quantity::Temperature, &from, &to);
        _test_si_convert(0.0, -17.7777777777778, Unit::F, "f", Unit::C, "c", Quantity::Temperature, &from, &to);
        _check_units_tested(from, to, Quantity::Temperature);
    });

    it("Energy: kwh, wh, mj, gj, m3c", []()
    {
        std::set<Unit> from, to;
        _fill_with_units_from(Quantity::Energy, &from);
        _fill_with_units_from(Quantity::Energy, &to);
        _test_si_convert(1.0, 1000, Unit::KWH, "kwh", Unit::WH, "wh", Quantity::Energy, &from, &to);
        _test_si_convert(2000.0, 2.0, Unit::WH, "wh", Unit::KWH, "kwh", Quantity::Energy, &from, &to);
        _test_si_convert(1.0, 3.6, Unit::KWH, "kwh", Unit::MJ, "mj", Quantity::Energy, &from, &to);
        _test_si_convert(1.0, 0.0036, Unit::KWH, "kwh", Unit::GJ, "gj", Quantity::Energy, &from, &to);
        _test_si_convert(1.0, 1000.0, Unit::GJ, "gj", Unit::MJ, "mj", Quantity::Energy, &from, &to);
        _test_si_convert(10, 2.7777777777777777, Unit::MJ, "mj", Unit::KWH, "kwh", Quantity::Energy, &from, &to);
        _test_si_convert(1.0/3600000.0, 0.000001, Unit::KWH, "kwh", Unit::MJ, "mj", Quantity::Energy, &from, &to);
        _test_si_convert(99.0, 99.0, Unit::M3C, "m3c", Unit::M3C, "m3c", Quantity::Energy, &from, &to);
        _test_expected_failed_si_convert(Unit::M3C, Unit::KWH, Quantity::Energy);
        _check_units_tested(from, to, Quantity::Energy);
    });

    it("Reactive_Energy: kvarh", []()
    {
        std::set<Unit> from, to;
        _fill_with_units_from(Quantity::Reactive_Energy, &from);
        _fill_with_units_from(Quantity::Reactive_Energy, &to);
        _test_si_convert(1.0, 1.0, Unit::KVARH, "kvarh", Unit::KWH, "kvarh", Quantity::Reactive_Energy, &from, &to);
        _test_si_convert(1.0, 1.0, Unit::KWH, "kvarh", Unit::KVARH, "kvarh", Quantity::Reactive_Energy, &from, &to);
        _check_units_tested(from, to, Quantity::Reactive_Energy);
    });

    it("Apparent_Energy: kvah", []()
    {
        std::set<Unit> from, to;
        _fill_with_units_from(Quantity::Apparent_Energy, &from);
        _fill_with_units_from(Quantity::Apparent_Energy, &to);
        _test_si_convert(1.0, 1.0, Unit::KVAH, "kvah", Unit::KWH, "kvah", Quantity::Apparent_Energy, &from, &to);
        _test_si_convert(1.0, 1.0, Unit::KWH, "kvah", Unit::KVAH, "kvah", Quantity::Apparent_Energy, &from, &to);
        _check_units_tested(from, to, Quantity::Apparent_Energy);
    });

    it("Volume: m3, l", []()
    {
        std::set<Unit> from, to;
        _fill_with_units_from(Quantity::Volume, &from);
        _fill_with_units_from(Quantity::Volume, &to);
        _test_si_convert(1, 1000.0, Unit::M3, "m3", Unit::L, "l", Quantity::Volume, &from, &to);
        _test_si_convert(1, 1.0/1000.0, Unit::L, "l", Unit::M3, "m3", Quantity::Volume, &from, &to);
        _check_units_tested(from, to, Quantity::Volume);
    });

    it("Voltage: v", []()
    {
        std::set<Unit> from, to;
        _fill_with_units_from(Quantity::Voltage, &from);
        _fill_with_units_from(Quantity::Voltage, &to);
        _test_si_convert(1, 1, Unit::Volt, "v", Unit::Volt, "v", Quantity::Voltage, &from, &to);
        _check_units_tested(from, to, Quantity::Voltage);
    });

    it("Power: kw, w, jh, mjh, dbm, m3ch", []()
    {
        std::set<Unit> from, to;
        _fill_with_units_from(Quantity::Power, &from);
        _fill_with_units_from(Quantity::Power, &to);
        _test_si_convert(1, 1000, Unit::KW, "kw", Unit::W, "w", Quantity::Power, &from, &to);
        _test_si_convert(1000, 1, Unit::W, "w", Unit::KW, "kw", Quantity::Power, &from, &to);
        _test_si_convert(1, double(1.0)/3600.0, Unit::JH, "jh", Unit::W, "w", Quantity::Power, &from, &to);
        _test_si_convert(double(1.0)/3600.0, 1, Unit::W, "w", Unit::JH, "jh", Quantity::Power, &from, &to);
        _test_si_convert(1, double(1000.0)/3600.0, Unit::MJH, "mjh", Unit::KW, "kw", Quantity::Power, &from, &to);
        _test_si_convert(double(1000.0)/3600.0, 1, Unit::KW, "kw", Unit::MJH, "mjh", Quantity::Power, &from, &to);
        _test_si_convert(-1.2, -1.2, Unit::DBM, "dbm", Unit::DBM, "dbm", Quantity::Power, &from, &to);
        _test_si_convert(0, 0.001, Unit::DBM, "dbm", Unit::W, "w", Quantity::Power, &from, &to);
        _test_si_convert(0.001, 0, Unit::W, "w", Unit::DBM, "dbm", Quantity::Power, &from, &to);
        _test_si_convert(0, 0.000001, Unit::DBM, "dbm", Unit::KW, "kw", Quantity::Power, &from, &to);
        _test_si_convert(0.000001, 0, Unit::KW, "kw", Unit::DBM, "dbm", Quantity::Power, &from, &to);
        _test_si_convert(0.003, 34.771212547196626, Unit::KW, "kw", Unit::DBM, "dbm", Quantity::Power, &from, &to);
        _test_si_convert(99.0, 99.0, Unit::M3CH, "m3ch", Unit::M3CH, "m3ch", Quantity::Power, &from, &to);
        _test_expected_failed_si_convert(Unit::M3CH, Unit::KW, Quantity::Power);
        _check_units_tested(from, to, Quantity::Power);
    });

    it("Reactive_Power: kvar", []()
    {
        std::set<Unit> from, to;
        _fill_with_units_from(Quantity::Reactive_Power, &from);
        _fill_with_units_from(Quantity::Reactive_Power, &to);
        _test_si_convert(1.0, 1.0, Unit::KVAR, "kvar", Unit::KW, "kvar", Quantity::Reactive_Power, &from, &to);
        _test_si_convert(1.0, 1.0, Unit::KW, "kvar", Unit::KVAR, "kvar", Quantity::Reactive_Power, &from, &to);
        _check_units_tested(from, to, Quantity::Reactive_Power);
    });

    it("Apparent_Power: kva", []()
    {
        std::set<Unit> from, to;
        _fill_with_units_from(Quantity::Apparent_Power, &from);
        _fill_with_units_from(Quantity::Apparent_Power, &to);
        _test_si_convert(1.0, 1.0, Unit::KVA, "kva", Unit::KW, "kva", Quantity::Apparent_Power, &from, &to);
        _test_si_convert(1.0, 1.0, Unit::KW, "kva", Unit::KVA, "kva", Quantity::Apparent_Power, &from, &to);
        _check_units_tested(from, to, Quantity::Apparent_Power);
    });

    it("Flow: m3h, lh", []()
    {
        std::set<Unit> from, to;
        _fill_with_units_from(Quantity::Flow, &from);
        _fill_with_units_from(Quantity::Flow, &to);
        _test_si_convert(1, 1000.0, Unit::M3H, "m3h", Unit::LH, "lh", Quantity::Flow, &from, &to);
        _test_si_convert(1000.0, 1.0, Unit::LH, "lh", Unit::M3H, "m3h", Quantity::Flow, &from, &to);
        _check_units_tested(from, to, Quantity::Flow);
    });

    it("AmountOfSubstance: mol", []()
    {
        std::set<Unit> from, to;
        _fill_with_units_from(Quantity::AmountOfSubstance, &from);
        _fill_with_units_from(Quantity::AmountOfSubstance, &to);
        _test_si_convert(1.1717, 1.1717, Unit::MOL, "mol", Unit::MOL, "mol", Quantity::AmountOfSubstance, &from, &to);
        _check_units_tested(from, to, Quantity::AmountOfSubstance);
    });

    it("LuminousIntensity: cd", []()
    {
        std::set<Unit> from, to;
        _fill_with_units_from(Quantity::LuminousIntensity, &from);
        _fill_with_units_from(Quantity::LuminousIntensity, &to);
        _test_si_convert(1.1717, 1.1717, Unit::CD, "cd", Unit::CD, "cd", Quantity::LuminousIntensity, &from, &to);
        _check_units_tested(from, to, Quantity::LuminousIntensity);
    });

    it("RelativeHumidity: rh", []()
    {
        std::set<Unit> from, to;
        _fill_with_units_from(Quantity::RelativeHumidity, &from);
        _fill_with_units_from(Quantity::RelativeHumidity, &to);
        _test_si_convert(1.1717, 1.1717, Unit::RH, "rh", Unit::RH, "rh", Quantity::RelativeHumidity, &from, &to);
        _check_units_tested(from, to, Quantity::RelativeHumidity);
    });

    it("HCA: hca", []()
    {
        std::set<Unit> from, to;
        _fill_with_units_from(Quantity::HCA, &from);
        _fill_with_units_from(Quantity::HCA, &to);
        _test_si_convert(11717, 11717, Unit::HCA, "hca", Unit::HCA, "hca", Quantity::HCA, &from, &to);
        _check_units_tested(from, to, Quantity::HCA);
    });

    it("Pressure: bar, pa", []()
    {
        std::set<Unit> from, to;
        _fill_with_units_from(Quantity::Pressure, &from);
        _fill_with_units_from(Quantity::Pressure, &to);
        _test_si_convert(1.1717, 117170, Unit::BAR, "bar", Unit::PA, "pa", Quantity::Pressure, &from, &to);
        _test_si_convert(1.1717, 1.1717e-05, Unit::PA, "pa", Unit::BAR, "bar", Quantity::Pressure, &from, &to);
        _check_units_tested(from, to, Quantity::Pressure);
    });

    it("Frequency: hz", []()
    {
        std::set<Unit> from, to;
        _fill_with_units_from(Quantity::Frequency, &from);
        _fill_with_units_from(Quantity::Frequency, &to);
        _test_si_convert(440, 440, Unit::HZ, "hz", Unit::HZ, "hz", Quantity::Frequency, &from, &to);
        _check_units_tested(from, to, Quantity::Frequency);
    });

    it("Dimensionless: counter, factor, number, pct, ppm", []()
    {
        std::set<Unit> from, to;
        _fill_with_units_from(Quantity::Dimensionless, &from);
        _fill_with_units_from(Quantity::Dimensionless, &to);
        _test_si_convert(2211717, 2211717, Unit::COUNTER, "counter", Unit::FACTOR,  "counter", Quantity::Dimensionless, &from, &to);
        _test_si_convert(2211717, 2211717, Unit::FACTOR,  "counter", Unit::COUNTER, "counter", Quantity::Dimensionless, &from, &to);
        _test_si_convert(2211717, 2211717, Unit::NUMBER,  "counter", Unit::COUNTER, "counter", Quantity::Dimensionless, &from, &to);
        _test_si_convert(2211717, 2211717, Unit::FACTOR,  "counter", Unit::NUMBER,  "counter", Quantity::Dimensionless, &from, &to);
        _test_si_convert(22,   0.22,       Unit::PERCENTAGE, "pct",  Unit::NUMBER,  "counter", Quantity::Dimensionless, &from, &to);
        _test_si_convert(0.22, 22,         Unit::NUMBER,  "counter", Unit::PERCENTAGE, "pct",  Quantity::Dimensionless, &from, &to);
        _test_si_convert(222,  0.000222,   Unit::PPM,     "ppm",     Unit::NUMBER,  "counter", Quantity::Dimensionless, &from, &to);
        _test_si_convert(1.234, 1234000.0, Unit::NUMBER,  "counter", Unit::PPM,     "ppm",     Quantity::Dimensionless, &from, &to);
        _check_units_tested(from, to, Quantity::Dimensionless);
    });

    it("Angle: deg, rad", []()
    {
        std::set<Unit> from, to;
        _fill_with_units_from(Quantity::Angle, &from);
        _fill_with_units_from(Quantity::Angle, &to);
        _test_si_convert(180,                   3.1415926535897931l, Unit::DEGREE, "deg", Unit::RADIAN, "rad", Quantity::Angle, &from, &to);
        _test_si_convert(3.1415926535897931l, 180,                   Unit::RADIAN, "rad", Unit::DEGREE, "deg", Quantity::Angle, &from, &to);
        _check_units_tested(from, to, Quantity::Angle);
    });
});

#endif // UNITS_TEST_H
