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

#include<cassert>
#include<cstring>

using namespace Translate;
using namespace std;

TriggerBits AlwaysTrigger(~(uint64_t)0);
MaskBits AutoMask(0);
PreShiftRight NoPreShift(0);

void handleBitToString(Rule& rule, string &out_s, uint64_t bits)
{
    string s;

    if (rule.trigger != AlwaysTrigger && (bits & rule.trigger.intValue()) == 0 )
    {
        // The trigger bits are needed and there are no trigger bits. Ignore this rule.
        return;
    }

    uint64_t mask = rule.mask.intValue();

    if (rule.mask == AutoMask)
    {
        mask = 0;
        for (Map& m : rule.map)
        {
            // Collect all listed bits as the mask.
            mask |= m.from;
        }
    }

    bits = bits >> rule.pre_shift_right.intValue();
    bits = bits & mask;
    for (Map& m : rule.map)
    {
        if ((~mask & m.from) != 0)
        {
            // Check that the match rule does not extend outside of the mask!
            // If mask is 0xff then a match for 0x100 will trigger this bad warning!
            string tmp = tostrprintf("BAD_RULE_%s(from=0x%x mask=0x%x)", rule.name.c_str(), m.from, mask);
            s += tmp+" ";
        }

        uint64_t from = m.from & mask; // Better safe than sorry.

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
        strprintf(&tmp, "%s_%X", rule.name.c_str(), bits);
        s += tmp+" ";
    }

    if (s == "")
    {
        s = rule.default_message.stringValue()+" ";
    }

    out_s += s;
}

void handleIndexToString(Rule& rule, string &out_s, uint64_t bits)
{
    string s;

    if (rule.trigger != AlwaysTrigger && (bits & rule.trigger.intValue()) == 0 )
    {
        // The trigger bits are needed and there are no trigger bits. Ignore this rule.
        return;
    }

    uint64_t mask = rule.mask.intValue();

    if (rule.mask == AutoMask)
    {
        mask = 0;
        for (Map& m : rule.map)
        {
            // Collect all listed bits as the mask.
            mask |= m.from;
        }
    }

    bits = bits >> rule.pre_shift_right.intValue();
    bits = bits & mask;
    bool found = false;
    for (Map& m : rule.map)
    {
        assert(m.test == TestBit::Set);

        if ((~mask & m.from) != 0)
        {
            string tmp;
            strprintf(&tmp, "BAD_RULE_%s(from=0x%x mask=0x%x)", rule.name.c_str(), m.from, rule.mask);
            s += tmp+" ";
        }
        uint64_t from = m.from & mask; // Better safe than sorry.
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
        strprintf(&tmp, "%s_%X", rule.name.c_str(), bits);
        s += tmp+" ";
    }

    out_s += s;
}

void handleDecimalsToString(Rule& rule, string &out_s, uint64_t bits)
{
    string s;

    if (rule.trigger != AlwaysTrigger && (bits & rule.trigger.intValue()) == 0 )
    {
        // The trigger bits are needed and there are no trigger bits. Ignore this rule.
        return;
    }

    uint64_t mask = rule.mask.intValue();

    if (rule.mask == AutoMask)
    {
        mask = 0;
        for (Map& m : rule.map)
        {
            // Collect all listed bits as the mask.
            mask |= m.from;
        }
    }

    bits = bits >> rule.pre_shift_right.intValue();

    // Switch to signed number here.
    int number = bits % mask;
    if (number == 0)
    {
        s += rule.default_message.stringValue()+" ";
    }
    for (Map& m : rule.map)
    {
        assert(m.test == TestBit::Set);

        if ((m.from - (m.from % mask)) != 0)
        {
            string tmp;
            strprintf(&tmp, "BAD_RULE_%s(from=%d modulomask=%d)", rule.name.c_str(), m.from, rule.mask);
            s += tmp+" ";
        }
        int num = m.from % mask; // Better safe than sorry.
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
        strprintf(&tmp, "%s_%d", rule.name.c_str(), number);
        s += tmp+" ";
    }

    out_s += s;
}

void Rule::addReservedBitMarkers()
{
    if (!mark_reserved_bits) return;
    if (type != MapType::BitToString) return;
    if (mask == AutoMask) return;

    uint64_t m = mask.value();
    uint64_t covered = 0;
    for (Map &e : map) covered |= e.from;

    for (int bit = 0; bit < 64; ++bit)
    {
        uint64_t bitval = (uint64_t)1 << bit;
        if ((m & bitval) != 0 && (covered & bitval) == 0)
        {
            map.push_back(Map(bitval, "RESERVED_BIT_"+std::to_string(bit), TestBit::Set));
        }
    }
}

void handleRule(Rule& rule, string &s, uint64_t bits)
{
    switch (rule.type)
    {
    case MapType::BitToString:
        handleBitToString(rule, s, bits);
        break;

    case MapType::IndexToString:
        handleIndexToString(rule, s, bits);
        break;

    case MapType::DecimalsToString:
        handleDecimalsToString(rule, s, bits);
        break;

    default:
        assert(0);
    }
}

string Lookup::translate(uint64_t bits)
{
    string total = "";

    for (Rule& r : rules)
    {
        string s;
        handleRule(r, s, bits);
        total = joinStatusEmptyStrings(total, s);
    }

    while (total.size() > 0 && total.back() == ' ') total.pop_back();

    return sortStatusString(total);
}

map<string,bool> Lookup::translateToObject(uint64_t input_bits)
{
    map<string,bool> out;

    for (Rule& r : rules)
    {
        if (r.type != MapType::BitToString)
        {
            // Object output only makes sense for individual named bits/bitgroups.
            continue;
        }

        if (r.trigger != AlwaysTrigger && (input_bits & r.trigger.intValue()) == 0)
        {
            // The trigger bits are needed and there are no trigger bits. Ignore this rule.
            // FIXME(jkt, 2026-09): looks like this is actually an unused feature...
            continue;
        }

        uint64_t mask = r.mask.intValue();

        if (r.mask == AutoMask)
        {
            mask = 0;
            for (Map& m : r.map)
            {
                mask |= m.from;
            }
        }

        uint64_t bits = input_bits >> r.pre_shift_right.intValue();
        bits = bits & mask;

        for (Map& m : r.map)
        {
            uint64_t from = m.from & mask;
            bool value = false;

            if (m.test == TestBit::Set)
            {
                value = (bits & from) != 0;
            }
            else if (m.test == TestBit::NotSet)
            {
                value = (bits & from) == 0;
            }

            out[m.to] = out[m.to] || value;
        }

        // If mark_reserved_bits is not set, then there's very little to do because this feature
        // was designed to avoid adding dynamic IDs. Let's drop them silently.
    }

    return out;
}

string Lookup::str()
{
    string x = " Lookup {\n";

    for (Rule& r : rules)
    {
        x += "    Rulex {\n";
        x += "        name = "+r.name+"\n";
        x += "    }\n";
    }

    x += "}\n";

    return x;
}

Translate::MapType toMapType(const char *s)
{
    if (!strcmp(s, "BitToString")) return Translate::MapType::BitToString;
    if (!strcmp(s, "IndexToString")) return Translate::MapType::IndexToString;
    if (!strcmp(s, "DecimalsToString")) return Translate::MapType::DecimalsToString;
    return Translate::MapType::Unknown;
}

Lookup NoLookup = {};

TestBit toTestBit(const char *s)
{
    if (!strcmp(s, "Set")) return TestBit::Set;
    if (!strcmp(s, "NotSet")) return TestBit::NotSet;
    return TestBit::Unknown;
}
