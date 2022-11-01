/*
 Copyright (C) 2019-2022 Fredrik Öhrström (gpl-3.0-or-later)

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

#include"units.h"
#include"util.h"
#include<assert.h>
#include<math.h>
#include<string.h>

using namespace std;

#define LIST_OF_CONVERSIONS \
    X(Second, Minute, {vto=vfrom/60.0;}) \
    X(Minute, Second, {vto=vfrom*60.0;}) \
    X(Second, Hour, {vto=vfrom/3600.0;}) \
    X(Hour, Second, {vto=vfrom*3600.0;}) \
    X(Year, Second, {vto=vfrom*3600.0*24.0*365.2425;}) \
    X(Second, Year, {vto=vfrom/3600.0/24.0/365.2425;}) \
    X(Minute, Hour, {vto=vfrom/60.0;}) \
    X(Hour, Minute, {vto=vfrom*60.0;}) \
    X(Minute, Year, {vto=vfrom/60.0/24.0/365.2425;}) \
    X(Year, Minute, {vto=vfrom*60.0*24.0*365.2425;}) \
    X(Hour, Year, {vto=vfrom/24.0/365.2425;}) \
    X(Year, Hour, {vto=vfrom*24.0*365.2425;}) \
    X(Hour,  Day, {vto=vfrom/24.0;}) \
    X(Day,  Hour, {vto=vfrom*24.0;}) \
    X(Day,  Year, {vto=vfrom/365.2425;}) \
    X(Year,  Day, {vto=vfrom*365.2425;}) \
    X(KWH, GJ, {vto=vfrom*0.0036;})     \
    X(KWH, MJ, {vto=vfrom*0.0036*1000.0;})     \
    X(GJ,  KWH,{vto=vfrom/0.0036;}) \
    X(MJ,  GJ,  {vto=vfrom/1000.0;}) \
    X(MJ,  KWH,{vto=vfrom/1000.0/0.0036;}) \
    X(GJ,  MJ,  {vto=vfrom*1000.0;}) \
    X(M3,  L,  {vto=vfrom*1000.0;}) \
    X(M3H, LH, {vto=vfrom*1000.0;}) \
    X(L,   M3, {vto=vfrom/1000.0;}) \
    X(LH,  M3H,{vto=vfrom/1000.0;}) \
    X(C,   K,  {vto=vfrom+273.15;}) \
    X(K,   C,  {vto=vfrom-273.15;}) \
    X(C,   F,  {vto=(vfrom*9.0/5.0)+32.0;}) \
    X(F,   C,  {vto=(vfrom-32)*5.0/9.0;}) \
    X(PA,  BAR,{vto=vfrom/100000.0;}) \
    X(BAR, PA, {vto=vfrom*100000.0;}) \


#define LIST_OF_SI_CONVERSIONS  \
    X(Second,1.0,0,SI_S(1))     \
    X(M,1.0,0,SI_M(1))          \
    X(KG,1.0,0,SI_KG(1))        \
    X(Ampere,1.0,0,SI_A(1))     \
    X(K,1.0,0,SI_K(1))                                                  \
    X(MOL,1.0,0,SI_MOL(1))                                              \
    X(CD,1.0,0,SI_CD(1))                                                \
    X(KWH, 3.6e+06, 0, SI_KG(1)|SI_M(2)|SI_S(-2))                       \
    X(MJ,  1.0e+06, 0, SI_KG(1)|SI_M(2)|SI_S(-2))                       \
    X(GJ,  1.0e+09, 0, SI_KG(1)|SI_M(2)|SI_S(-2))                       \
    X(KVARH,3.6e+06, 0, SI_KG(1)|SI_M(2)|SI_S(-2))                      \
    X(KVAH, 3.6e+06, 0, SI_KG(1)|SI_M(2)|SI_S(-2))                      \
    X(M3C, 1.0,    0, SI_M(3)|SI_K(1))                                  \
    X(M3,  1.0,        0, SI_M(3))                                      \
    X(L,   1.0/1000.0, 0, SI_M(3))                                      \
    X(KW,  1000.0,     0,  SI_KG(1)|SI_M(2)|SI_S(-3))                   \
    X(M3H, 3600.0,     0,  SI_M(3)|SI_S(-1))                            \
    X(LH,  3.600,      0,  SI_M(3)|SI_S(-1))                            \
    X(C,   1.0,        273.15,                   SI_K(1))               \
    X(F,   (5.0/9.0),  (-32.0+(273.15*9.0/5.0)), SI_K(1))               \
    X(RH,      1.0,0,0)                                                 \
    X(HCA,     1.0,0,0)                                                 \
    X(TXT,     1.0,0,0)                                                 \
    X(COUNTER, 1.0,0,0)                                                 \
    X(Minute, 60.0,          0, SI_S(1))                                \
    X(Hour,   3600.0,        0, SI_S(1))                                \
    X(Day,    3600.0*24,     0, SI_S(1))                                \
    X(Year,   3600.0*24*365.2425, 0, SI_S(1))                           \
    X(DateTimeUT,1.0,0,SI_S(1))                                         \
    X(DateTimeUTC,1.0,0,SI_S(1))                                        \
    X(DateTimeLT,1.0,0,SI_S(1))                                         \
    X(Volt,   1.0,           0, SI_KG(1)|SI_M(2)|SI_S(-3)|SI_A(-1))     \
    X(HZ,     1.0,           0, SI_S(-1))                               \
    X(PA,     1.0,           0, SI_KG(1)|SI_M(-1)|SI_S(-2))             \
    X(BAR,    100000.0,      0, SI_KG(1)|SI_M(-1)|SI_S(-2))

bool canConvert(Unit ufrom, Unit uto)
{
    if (ufrom == uto) return true;
#define X(from,to,code) if (Unit::from == ufrom && Unit::to == uto) return true;
LIST_OF_CONVERSIONS
#undef X
    return false;
}

double convert(double vfrom, Unit ufrom, Unit uto)
{
    double vto = -4711.0;
    if (ufrom == uto) { { vto = vfrom; } return vto; }

#define X(from,to,code) if (Unit::from == ufrom && Unit::to == uto) { code return vto; }
LIST_OF_CONVERSIONS
#undef X

    string from = unitToStringHR(ufrom);
    string to = unitToStringHR(uto);

    fprintf(stderr, "Cannot convert between units! from %s to %s\n", from.c_str(), to.c_str());
    assert(0);
    return 0;
}

bool canConvert(SIUnit &ufrom, SIUnit &uto)
{
    return ufrom.sameExponents(uto);
}

double convert(double vfrom, SIUnit &ufrom, SIUnit &uto)
{
    assert(canConvert(ufrom, uto));

    return ufrom.convert(vfrom, uto);
}

double SIUnit::convert(double val, SIUnit &to)
{
    return ((val+offset_)*scale_)/to.scale_-to.offset_;
}

void SIUnit::mul(SIUnit &m)
{
}

SIUnit whenMultiplied(SIUnit left, SIUnit right)
{
    return Unit::Unknown;
}

double multiply(double l, SIUnit left, double r, SIUnit right)
{
    return 0;
}

bool isQuantity(Unit u, Quantity q)
{
#define X(cname,lcname,hrname,quantity,explanation) if (u == Unit::cname) return Quantity::quantity == q;
LIST_OF_UNITS
#undef X

    return false;
}

Quantity toQuantity(Unit u)
{
    switch(u)
    {
#define X(cname,lcname,hrname,quantity,explanation) case Unit::cname: return Quantity::quantity;
LIST_OF_UNITS
#undef X
    default:
        break;
    }
    return Quantity::Unknown;
}

void assertQuantity(Unit u, Quantity q)
{
    if (!isQuantity(u, q))
    {
        error("Internal error! Unit is not of this quantity.\n");
    }
}

Unit defaultUnitForQuantity(Quantity q)
{
#define X(quantity,default_unit) if (q == Quantity::quantity) return Unit::default_unit;
LIST_OF_QUANTITIES
#undef X

    return Unit::Unknown;
}

const char *toString(Quantity q)
{
#define X(quantity,default_unit) if (q == Quantity::quantity) return #quantity;
LIST_OF_QUANTITIES
#undef X
    return "?";
}

Unit toUnit(string s)
{
#define X(cname,lcname,hrname,quantity,explanation) if (s == #cname || s == #lcname) return Unit::cname;
LIST_OF_UNITS
#undef X

    return Unit::Unknown;
}

string unitToStringHR(Unit u)
{
#define X(cname,lcname,hrname,quantity,explanation) if (u == Unit::cname) return hrname;
LIST_OF_UNITS
#undef X

    return "?";
}

string unitToStringLowerCase(Unit u)
{
#define X(cname,lcname,hrname,quantity,explanation) if (u == Unit::cname) return #lcname;
LIST_OF_UNITS
#undef X

    return "?";
}

string unitToStringUpperCase(Unit u)
{
#define X(cname,lcname,hrname,quantity,explanation) if (u == Unit::cname) return #cname;
LIST_OF_UNITS
#undef X

    return "?";
}

string strWithUnitHR(double v, Unit u)
{
    string r = format3fdot3f(v);
    r += " "+unitToStringHR(u);
    return r;
}

string strWithUnitLowerCase(double v, Unit u)
{
    string r = format3fdot3f(v);
    r += " "+unitToStringLowerCase(u);
    return r;
}

Unit replaceWithConversionUnit(Unit u, vector<Unit> cs)
{
    for (Unit c : cs)
    {
        if (canConvert(u, c)) return c;
    }
    return u;
}

string valueToString(double v, Unit u)
{
    if (isnan(v))
    {
        return "null";
    }
    string s = to_string(v);
    while (s.size() > 0 && s.back() == '0') s.pop_back();
    if (s.back() == '.') {
        s.pop_back();
        if (s.length() == 0) return "0";
        return s;
    }
    if (s.length() == 0) return "0";
    return s;
}

bool extractUnit(const string &s, string *vname, Unit *u)
{
    size_t pos;
    string vn;
    const char *c;

    if (s.length() < 3) goto err;

    pos = s.rfind('_');

    if (pos == string::npos) goto err;
    if (pos+1 >= s.length()) goto err;
    vn = s.substr(0,pos);
    pos++;

    c = s.c_str()+pos;

#define X(cname,lcname,hrname,quantity,explanation) if (!strcmp(c, #lcname)) { *u = Unit::cname; *vname = vn; return true; }
LIST_OF_UNITS
#undef X

err:
*vname = "";
*u = Unit::Unknown;
return false;
}

SIUnit::SIUnit(Unit u)
{
    quantity_ = toQuantity(u);

    switch (u)
    {
#define X(cname,si_scale,si_offset,si_exponents) \
        case Unit::cname: scale_ = si_scale; offset_ = si_offset; exponents_ = si_exponents; break;
LIST_OF_SI_CONVERSIONS
#undef X
    default:
        quantity_ = Quantity::Unknown;
        scale_ = 0;
        offset_ = 0;
        exponents_ = 0;
    }
}

SIUnit::SIUnit(string s)
{
}

Unit SIUnit::asUnit()
{
#define X(cname,si_scale,si_offset,si_exponents) \
    if ((scale_ == si_scale) && (offset_ == si_offset) && (exponents_ == (si_exponents)) && quantity_ == toQuantity(Unit::cname)) return Unit::cname;
LIST_OF_SI_CONVERSIONS
#undef X

    return Unit::Unknown;
}

Unit SIUnit::asUnit(Quantity q)
{
#define X(cname,si_scale,si_offset,si_exponents) \
    if ((scale_ == si_scale) && (offset_ == si_offset) && (exponents_ == (si_exponents)) && q == toQuantity(Unit::cname)) return Unit::cname;
LIST_OF_SI_CONVERSIONS
#undef X

    return Unit::Unknown;
}

string super(uchar c)
{
    switch (c)
    {
    case '-': return "⁻";
    case '+': return "⁺";
    case '0': return "⁰";
    case '1': return "¹";
    case '2': return "²";
    case '3': return "³";
    case '4': return "⁴";
    case '5': return "⁵";
    case '6': return "⁶";
    case '7': return "⁷";
    case '8': return "⁸";
    case '9': return "⁹";
    }
    assert(false);
    return "?";
}

string to_superscript(int8_t n)
{
    string out;

    char buf[5];
    sprintf(buf, "%d", n);

    for (int i=0; i<5; ++i)
    {
        if (buf[i] == 0) break;
        out += super(buf[i]);
    }

    return out;
}

string to_superscript(string &s)
{
    string out;

    size_t i = 0;
    size_t len = s.length();

    // Skip non-superscript number.
    while (i<len && (s[i] == '-' || s[i] == '.' || (s[i] >= '0' && s[i] <= '9')))
    {
        out += s[i];
        i++;
    }

    while (i<len && (s[i] == 'e' || s[i] == 'E'))
    {
        i++;
        out += "×10";
    }

    bool found_start = false;
    while (i<len)
    {
        // Remove leading +0
        if (!found_start && s[i] != '+' && s[i] != '0') found_start = true;

        if (found_start)
        {
            out += super(s[i]);
        }
        i++;
    }

    return out;
}

#define DO_UNIT(u) if (u != 0) { if (r.length()>0) { } r += #u; if (u != 1) { r += to_superscript(u); } }

string SIUnit::str()
{
    string r;
    int8_t s = SI_GET_S(exponents_);
    int8_t m = SI_GET_M(exponents_);
    int8_t kg = SI_GET_KG(exponents_);
    int8_t a = SI_GET_A(exponents_);
    int8_t k = SI_GET_K(exponents_);
    int8_t mol = SI_GET_MOL(exponents_);
    int8_t cd = SI_GET_CD(exponents_);

    DO_UNIT(mol);
    DO_UNIT(cd);
    DO_UNIT(kg);
    DO_UNIT(k);
    DO_UNIT(m);
    DO_UNIT(s);
    DO_UNIT(a);

    string num = tostrprintf("%g", scale_);
    string offset;
    if (offset_ > 0) offset = tostrprintf("+%g", offset_);
    if (offset_ < 0) offset = tostrprintf("%g", offset_);

    num = to_superscript(num);
    return num+r+offset;
}


string SIUnit::info()
{
    Unit u = asUnit();
    return tostrprintf("%s[%s]%s",
                       unitToStringLowerCase(u).c_str(),
                       str().c_str(),
                       toString(quantity_));
}
