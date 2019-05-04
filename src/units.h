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

#ifndef UNITS_H
#define UNITS_H

#include<string>
#include<vector>

enum class Unit
{
    Unknown, KWH, GJ
};

bool canConvert(Unit from, Unit to);
double convert(double v, Unit from, Unit to);

Unit toConversionUnit(std::string s);

std::string strWithUnitHR(double v, Unit u);
std::string strWithUnitLowerCase(double v, Unit u);

#endif
