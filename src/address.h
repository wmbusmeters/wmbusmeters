/*
 Copyright (C) 2017-2022 Fredrik Öhrström (gpl-3.0-or-later)

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

#ifndef ADDRESS_H_
#define ADDRESS_H_

#include "util.h"
#include <string>

struct Address
{
    // Example address: 12345678
    // Or fully qualified: 12345678.M=PII.T=1b.V=01
    // which means manufacturer triplet PII, type/media=0x1b, version=0x01
    std::string id;
    bool wildcard_used {}; // The id contains a *
    bool mbus_primary {}; // Signals that the id is 0-250
    uint16_t mfct {}; // If 0xffff then any mfct matches this address.
    uchar type {}; // If 0xff then any type matches this address.
    uchar version {}; // If 0xff then any version matches this address.
    bool negate {}; // When used for testing this address was negated. !12345678

    bool parse(std::string &s);
    bool match(Address *a);
};

bool isValidMatchExpression(const std::string& s, bool non_compliant);
bool isValidMatchExpressions(const std::string& s, bool non_compliant);
bool doesIdMatchExpression(const std::string& id, std::string match_rule);
bool doesIdMatchExpressions(const std::string& id, std::vector<std::string>& match_rules, bool *used_wildcard);
bool doesIdsMatchExpressions(std::vector<std::string> &ids, std::vector<std::string>& match_rules, bool *used_wildcard);
std::string toIdsCommaSeparated(std::vector<std::string> &ids);

bool isValidId(const std::string& id, bool accept_non_compliant);

std::vector<std::string> splitMatchExpressions(const std::string& mes);

#endif
