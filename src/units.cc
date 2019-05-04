/*
 Copyright (C) 2019 Fredrik Öhrström

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

using namespace std;

#define LIST_OF_CONVERSIONS \
    X(KWH, GJ, {vto=vfrom*0.0036;}) \
    X(GJ,  KWH,{vto=vfrom/0.0036;}) \
    X(M3,  L,  {vto=vfrom*1000.0;}) \
    X(L,   M3, {vto=vfrom/1000.0;})

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

    error("Cannot convert between units!\n");
    return 0;
}

bool isQuantity(Unit u, Quantity q)
{
#define X(cname,lcname,hrname,quantity,explanation) if (u == Unit::cname) return Quantity::quantity == q;
LIST_OF_UNITS
#undef X

    return false;
}

void assertQuantity(Unit u, Quantity q)
{
    if (!isQuantity(u, q))
    {
        error("Internal error! Unit is not of this quantity.\n");
    }
}

Unit toUnit(string s)
{
#define X(cname,lcname,hrname,quantity,explanation) if (s == #cname) return Unit::cname;
LIST_OF_UNITS
#undef X

    return Unit::Unknown;
}

string unitToStringHR(Unit u)
{
#define X(cname,lcname,hrname,quantity,explanation) if (u == Unit::cname) return #hrname;
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
