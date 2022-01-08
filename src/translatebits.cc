/*
 Copyright (C) 2021 Fredrik Öhrström

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

void handleRule(Rule& rule, string &s, uint64_t bits)
{
    // Keep only the masked bits.
    bits = bits & rule.mask;

    if (rule.type == Type::BitToString)
    {
        for (Map& m : rule.map)
        {
            if ((~rule.mask & m.from) != 0)
            {
                string tmp;
                strprintf(tmp, "BAD_RULE_%s(from=0x%x mask=0x%x)", rule.name.c_str(), m.from, rule.mask);
                s += tmp+" ";
            }
            uint64_t from = m.from & rule.mask; // Better safe than sorry.
            if ((bits & from) != 0)
            {
                s += m.to+" ";
                bits = bits & ~m.from; // Remove the handled bit.
            }
        }
        if (bits != 0)
        {
            // Oups, there are bits that we have not handled....
            string tmp;
            strprintf(tmp, "UNKNOWN_%s(0x%x)", rule.name.c_str(), bits);
            s += tmp+" ";
        }
    }
    else if (rule.type == Type::IndexToString)
    {
        bool found = false;
        for (Map& m : rule.map)
        {
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
            strprintf(tmp, "UNKNOWN_%s(0x%x)", rule.name.c_str(), bits);
            s += tmp+" ";
        }
    }
    else
    {
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

    while (s.back() == ' ') s.pop_back();
    return s;
}
