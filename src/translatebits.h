/*
 Copyright (C) 2021 Fredrik Öhrström (gpl-3.0-or-later)

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

#ifndef TRANSLATEBITS_H
#define TRANSLATEBITS_H

#include<stdint.h>
#include<string>
#include<vector>

#include"util.h"

namespace Translate
{
    enum class Type
    {
        BitToString, // A bit translates to a text string.
        IndexToString, // A masked set of bits (a number) translates to a lookup index with text strings.
        DecimalsToString // Numbers are successively subtracted from input, each successfull subtraction translate into a text string.
    };

    struct Map
    {
        uint64_t from;
        std::string to;
        TestBit test;
    };

    struct Rule
    {
        std::string name;
        Type type;
        uint64_t mask; // Bits to be used are set as 1.
        std::string no_bits_msg; // If no bits are set print this, typically "OK".
        std::vector<Map> map;
    };

    struct Lookup
    {
        std::vector<Rule> rules;

        std::string translate(uint64_t bits);
        bool hasLookups() { return rules.size() > 0; }
    };
};

extern Translate::Lookup NoLookup;

#endif
