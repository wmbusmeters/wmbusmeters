/*
 Copyright (C) 2017-2024 Fredrik Öhrström (gpl-3.0-or-later)

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

#include"address.h"
#include"manufacturers.h"

#include<assert.h>

using namespace std;

bool isValidMatchExpression(const string& s, bool *has_wildcard)
{
    string me = s;

    // Examples of valid match expressions:
    //  12345678
    //  *
    //  123*
    // !12345677
    //  2222222*
    // !22222222

    // A match expression cannot be empty.
    if (me.length() == 0) return false;

    // An me can be negated with an exclamation mark first.
    if (me.front() == '!') me.erase(0, 1);

    // A match expression cannot be only a negation mark.
    if (me.length() == 0) return false;

    int count = 0;
    // Some non-compliant meters have full hex in the id,
    // but according to the standard there should only be bcd here...
    // We accept hex anyway.
    while (me.length() > 0 &&
           ((me.front() >= '0' && me.front() <= '9') ||
            (me.front() >= 'a' && me.front() <= 'f')))
    {
        me.erase(0,1);
        count++;
    }

    bool wildcard_used = false;
    // An expression can end with a *
    if (me.length() > 0 && me.front() == '*')
    {
        me.erase(0,1);
        wildcard_used = true;
        if (has_wildcard) *has_wildcard = true;
    }

    // Now we should have eaten the whole expression.
    if (me.length() > 0) return false;

    // Check the length of the matching bcd/hex
    // If no wildcard is used, then the match expression must be exactly 8 digits.
    if (!wildcard_used) return count == 8;

    // If wildcard is used, then the match expressions must be 7 or less digits,
    // even zero is allowed which means a single *, which matches any bcd/hex id.
    return count <= 7;
}

bool isValidMatchExpressions(const string& mes)
{
    vector<string> v = splitMatchExpressions(mes);

    for (string me : v)
    {
        if (!isValidMatchExpression(me, NULL)) return false;
    }
    return true;
}

bool isValidId(const string& id)
{

    for (size_t i=0; i<id.length(); ++i)
    {
        if (id[i] >= '0' && id[i] <= '9') continue;
        // Some non-compliant meters have hex in their id.
        if (id[i] >= 'a' && id[i] <= 'f') continue;
        if (id[i] >= 'A' && id[i] <= 'F') continue;
        return false;
    }
    return true;
}

bool doesIdMatchExpression(const string& s, string match)
{
    string id = s;
    if (id.length() == 0) return false;

    // Here we assume that the match expression has been
    // verified to be valid.
    bool can_match = true;

    // Now match bcd/hex until end of id, or '*' in match.
    while (id.length() > 0 && match.length() > 0 && match.front() != '*')
    {
        if (id.front() != match.front())
        {
            // We hit a difference, it cannot match.
            can_match = false;
            break;
        }
        id.erase(0,1);
        match.erase(0,1);
    }

    bool wildcard_used = false;
    if (match.length() && match.front() == '*')
    {
        wildcard_used = true;
        match.erase(0,1);
    }

    if (can_match)
    {
        // Ok, now the match expression should be empty.
        // If wildcard is true, then the id can still have digits,
        // otherwise it must also be empty.
        if (wildcard_used)
        {
            can_match = match.length() == 0;
        }
        else
        {
            can_match = match.length() == 0 && id.length() == 0;
        }
    }

    return can_match;
}

bool hasWildCard(const string& mes)
{
    return mes.find('*') != string::npos;
}

bool doesIdsMatchExpressions(vector<string> &ids, vector<string>& mes, bool *used_wildcard)
{
    bool match = false;
    for (string &id : ids)
    {
        if (doesIdMatchExpressions(id, mes, used_wildcard))
        {
            match = true;
        }
        // Go through all ids even though there is an early match.
        // This way we can see if theres an exact match later.
    }
    return match;
}

bool doesIdMatchExpressions(const string& id, vector<string>& mes, bool *used_wildcard)
{
    bool found_match = false;
    bool found_negative_match = false;
    bool exact_match = false;
    *used_wildcard = false;

    // Goes through all possible match expressions.
    // If no expression matches, neither positive nor negative,
    // then the result is false. (ie no match)

    // If more than one positive match is found, and no negative,
    // then the result is true.

    // If more than one negative match is found, irrespective
    // if there is any positive matches or not, then the result is false.

    // If a positive match is found, using a wildcard not any exact match,
    // then *used_wildcard is set to true.

    for (string me : mes)
    {
        bool has_wildcard = hasWildCard(me);
        bool is_negative_rule = (me.length() > 0 && me.front() == '!');
        if (is_negative_rule)
        {
            me.erase(0, 1);
        }

        bool m = doesIdMatchExpression(id, me);

        if (is_negative_rule)
        {
            if (m) found_negative_match = true;
        }
        else
        {
            if (m)
            {
                found_match = true;
                if (!has_wildcard)
                {
                    exact_match = true;
                }
            }
        }
    }

    if (found_negative_match)
    {
        return false;
    }
    if (found_match)
    {
        if (exact_match)
        {
            *used_wildcard = false;
        }
        else
        {
            *used_wildcard = true;
        }
        return true;
    }
    return false;
}

bool doesIdMatchAddressExpressions(const string& id, vector<AddressExpression>& aes, bool *used_wildcard)
{
/*    bool found_match = false;
    bool found_negative_match = false;
    bool exact_match = false;*/
    *used_wildcard = false;

    // Goes through all possible match expressions.
    // If no expression matches, neither positive nor negative,
    // then the result is false. (ie no match)

    // If more than one positive match is found, and no negative,
    // then the result is true.

    // If more than one negative match is found, irrespective
    // if there is any positive matches or not, then the result is false.

    // If a positive match is found, using a wildcard not any exact match,
    // then *used_wildcard is set to true.
/*
    for (AddressExpression &ae : aes)
    {
        bool has_wildcard = ae.has_wildcard;
        bool is_negative_rule = (me.length() > 0 && me.front() == '!');
        if (is_negative_rule)
        {
            me.erase(0, 1);
        }

        bool m = doesIdMatchExpression(id, me);

        if (is_negative_rule)
        {
            if (m) found_negative_match = true;
        }
        else
        {
            if (m)
            {
                found_match = true;
                if (!has_wildcard)
                {
                    exact_match = true;
                }
            }
        }
    }

    if (found_negative_match)
    {
        return false;
    }
    if (found_match)
    {
        if (exact_match)
        {
            *used_wildcard = false;
        }
        else
        {
            *used_wildcard = true;
        }
        return true;
    }
*/
    return false;
}

string toIdsCommaSeparated(vector<string> &ids)
{
    string cs;
    for (string& s: ids)
    {
        cs += s;
        cs += ",";
    }
    if (cs.length() > 0) cs.pop_back();
    return cs;
}

bool AddressExpression::match(const std::string &i, uint16_t m, uchar v, uchar t)
{
    if (!(mfct == 0xffff || mfct == m)) return false;
    if (!(version == 0xff || version == v)) return false;
    if (!(type == 0xff || type == t)) return false;
    if (!doesIdMatchExpression(i, id)) return false;

    return true;
}

bool AddressExpression::parse(const string &in)
{
    string s = in;
    // Example: 12345678
    // or       12345678.M=PII.T=1B.V=01
    // or       1234*
    // or       1234*.M=PII
    // or       1234*.V=01
    // or       12 // mbus primary
    // or       0  // mbus primary
    // or       250.MPII.V01.T1B // mbus primary
    // or       !12345678
    // or       !*.M=ABC
    id = "";
    mbus_primary = false;
    mfct = 0xffff;
    type = 0xff;
    version = 0xff;
    filter_out = false;

    if (s.size() == 0) return false;

    if (s.size() > 1 && s[0] == '!')
    {
        filter_out = true;
        s = s.substr(1);
    }

    vector<string> parts = splitString(s, '.');

    assert(parts.size() > 0);

    id = parts[0];
    if (!isValidMatchExpression(id, &has_wildcard))
    {
        // Not a long id, so lets check if it is p0 to p250 for primary mbus ids.
        if (id.size() < 2) return false;
        if (id[0] != 'p') return false;
        for (size_t i=1; i < id.length(); ++i)
        {
            if (!isdigit(id[i])) return false;
        }
        // All digits good.
        int v = atoi(id.c_str()+1);
        if (v < 0 || v > 250) return false;
        // It is 0-250 which means it is an mbus primary address.
        mbus_primary = true;
    }

    for (size_t i=1; i<parts.size(); ++i)
    {
        if (parts[i].size() == 4) // V=xy or T=xy
        {
            if (parts[i][1] != '=') return false;

            vector<uchar> data;
            bool ok = hex2bin(&parts[i][2], &data);
            if (!ok) return false;
            if (data.size() != 1) return false;

            if (parts[i][0] == 'V')
            {
                version = data[0];
            }
            else if (parts[i][0] == 'T')
            {
                type = data[0];
            }
            else
            {
                return false;
            }
        }
        else if (parts[i].size() == 5) // M=xyz
        {
            if (parts[i][1] != '=') return false;
            if (parts[i][0] != 'M') return false;

            bool ok = flagToManufacturer(&parts[i][2], &mfct);
            if (!ok) return false;
        }
        else
        {
            return false;
        }
    }

    return true;
}

bool flagToManufacturer(const char *s, uint16_t *out_mfct)
{
    if (s[0] == 0 || s[1] == 0 || s[2] == 0 || s[3] != 0) return false;
    if (s[0] < '@' || s[0] > 'Z' ||
        s[1] < '@' || s[1] > 'Z' ||
        s[2] < '@' || s[2] > 'Z') return false;

    *out_mfct = MANFCODE(s[0],s[1],s[2]);
    return true;
}
