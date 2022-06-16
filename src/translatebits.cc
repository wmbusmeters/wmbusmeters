/*
 Copyright (C) 2021-2022 Fredrik Öhrström (gpl-3.0-or-later)

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

#include"translatebits.h"
#include"util.h"

#include<assert.h>

using namespace Translate;
using namespace std;

void handleBitToString(Rule& rule, string &out_s, uint64_t bits)
{
    string s;

    bits = bits & rule.mask;
    for (Map& m : rule.map)
    {
        if ((~rule.mask & m.from) != 0)
        {
            // Check that the match rule does not extend outside of the mask!
            // If mask is 0xff then a match for 0x100 will trigger this bad warning!
            string tmp = tostrprintf("BAD_RULE_%s(from=0x%x mask=0x%x)", rule.name.c_str(), m.from, rule.mask);
            s += tmp+" ";
        }

        uint64_t from = m.from & rule.mask; // Better safe than sorry.

        if (m.test == TestBit::Set)
        {
            if ((bits & from) != 0 )
            {
                s += m.to+" ";
                bits = bits & ~m.from; // Remove the handled bit.
            }
        }

        if (m.test == TestBit::NotSet)
        {
            if ((bits & from) == 0)
            {
                s += m.to+" ";
            }
            else
            {
                bits = bits & ~m.from; // Remove the handled bit.
            }
        }
    }
    if (bits != 0)
    {
        // Oups, there are set bits that we have not handled....
        string tmp;
        strprintf(tmp, "%s_%X", rule.name.c_str(), bits);
        s += tmp+" ";
    }

    if (s == "")
    {
        s = rule.no_bits_msg+" ";
    }

    out_s += s;
}

void handleIndexToString(Rule& rule, string &out_s, uint64_t bits)
{
    string s;

    bits = bits & rule.mask;
    bool found = false;
    for (Map& m : rule.map)
    {
        assert(m.test == TestBit::Set);

        if ((~rule.mask & m.from) != 0)
        {
            string tmp;
            strprintf(tmp, "BAD_RULE_%s(from=0x%x mask=0x%x)", rule.name.c_str(), m.from, rule.mask);
            s += tmp+" ";
        }
        uint64_t from = m.from & rule.mask; // Better safe than sorry.
        if (bits == from)
        {
            s += m.to+" ";
            found = true;
        }
    }
    if (!found)
    {
        // Oups, this index has not been found.
        string tmp;
        strprintf(tmp, "%s_%X", rule.name.c_str(), bits);
        s += tmp+" ";
    }

    out_s += s;
}

void handleDecimalsToString(Rule& rule, string &out_s, uint64_t bits)
{
    string s;

    // Switch to signed number here.
    int number = bits % rule.mask;
    if (number == 0)
    {
        s += rule.no_bits_msg+" ";
    }
    for (Map& m : rule.map)
    {
        assert(m.test == TestBit::Set);

        if ((m.from - (m.from % rule.mask)) != 0)
        {
            string tmp;
            strprintf(tmp, "BAD_RULE_%s(from=%d modulomask=%d)", rule.name.c_str(), m.from, rule.mask);
            s += tmp+" ";
        }
        int num = m.from % rule.mask; // Better safe than sorry.
        if ((number - num) >= 0)
        {
            s += m.to+" ";
            number -= num;
        }
    }
    if (number > 0)
    {
        // Oups, this number has not been fully understood.
        string tmp;
        strprintf(tmp, "%s_%d", rule.name.c_str(), number);
        s += tmp+" ";
    }

    out_s += s;
}

void handleRule(Rule& rule, string &s, uint64_t bits)
{
    switch (rule.type)
    {
    case Type::BitToString:
        handleBitToString(rule, s, bits);
        break;

    case Type::IndexToString:
        handleIndexToString(rule, s, bits);
        break;

    case Type::DecimalsToString:
        handleDecimalsToString(rule, s, bits);
        break;

    default:
        assert(0);
    }
}

string Lookup::translate(uint64_t bits)
{
    string s = "";

    for (Rule& r : rules)
    {
        handleRule(r, s, bits);
    }

    while (s.size() > 0 && s.back() == ' ') s.pop_back();
    return s;
}

Lookup NoLookup = {};
