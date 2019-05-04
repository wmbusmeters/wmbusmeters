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
    X(KWH,GJ, {vto=vfrom*0.0036;}) \
    X(GJ,KWH, {vto=vfrom/0.0036;}) \
    X(M3,L, {vto=vfrom*1000.0;}) \
    X(L,M3, {vto=vfrom/1000.0;})

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


Unit toConversionUnit(string s)
{
    if (s == "GJ") {
        return Unit::GJ;
    }
    if (s == "KWH") {
        return Unit::KWH;
    }
    return Unit::Unknown;
}

string unitToStringHR(Unit u)
{
    if (u == Unit::GJ) {
        return "GJ";
    }
    if (u == Unit::KWH) {
        return "kWh";
    }
    return "?";
}

string unitToStringLowerCase(Unit u)
{
    if (u == Unit::GJ) {
        return "gj";
    }
    if (u == Unit::KWH) {
        return "kwh";
    }
    return "?";
}

string unitToStringUpperCase(Unit u)
{
    if (u == Unit::GJ) {
        return "GJ";
    }
    if (u == Unit::KWH) {
        return "KWH";
    }
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
