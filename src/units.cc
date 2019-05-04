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

double from_KWH_to_GJ(double d);
double from_GJ_to_KWH(double d);

bool canConvert(Unit from, Unit to)
{
    if (from == to) return true;
    if (from == Unit::KWH && to == Unit::GJ) return true;
    if (from == Unit::GJ && to == Unit::KWH) return true;
    return false;
}

double convert(double v, Unit from, Unit to)
{
    if (from == Unit::KWH && to == Unit::GJ) {
        return from_KWH_to_GJ(v);
    }
    if (from == Unit::GJ && to == Unit::KWH) {
        return from_GJ_to_KWH(v);
    }
    error("Cannot convert between units!\n");
    return 0;
}

double from_KWH_to_GJ(double kwh)
{
    return kwh * 0.0036;
}

double from_GJ_to_KWH(double gj)
{
    return gj / 0.0036;
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
