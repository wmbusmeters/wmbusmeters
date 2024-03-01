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

struct AddressExpression
{
    // An address expression is used to select which telegrams to decode for a driver.
    // An address expression is also used to select a specific meter to poll for data.
    // Example address: 12345678
    // Or fully qualified: 12345678.M=PII.T=1b.V=01
    // which means manufacturer triplet PII, type/media=0x1b, version=0x01
    // Or wildcards in id: 12*.T=16
    // which matches all cold water meters whose ids start with 12.
    // Or negated tests: 12345678.V!=66
    // which will decode all telegrams from 12345678 except those where the version is 0x66.
    // Or every telegram which is does not start with 12 and is not from ABB:
    // !12*.M!=ABB

    std::string id; // 1 or 12345678 or non-compliant hex: 1234abcd
    bool has_wildcard {}; // The id contains a *
    bool mbus_primary {}; // Signals that the id is 0-250

    uint16_t mfct {}; // If 0xffff then any mfct matches this address.
    uchar type {}; // If 0xff then any type matches this address.
    uchar version {}; // If 0xff then any version matches this address.

    bool filter_out {}; // Telegrams matching this rule should be filtered out!

    bool parse(const std::string &s);
    bool match(const std::string &id, uint16_t mfct, uchar version, uchar type);
};

bool isValidMatchExpression(const std::string& s, bool *has_wildcard);
bool isValidMatchExpressions(const std::string& s);

bool doesIdMatchExpression(const std::string& id,
                           std::string match_rule);
bool doesIdMatchExpressions(const std::string& id,
                            std::vector<std::string>& match_rules,
                            bool *used_wildcard);
bool doesIdsMatchExpressions(std::vector<std::string> &ids,
                             std::vector<std::string>& match_rules,
                             bool *used_wildcard);
std::string toIdsCommaSeparated(std::vector<std::string> &ids);

bool isValidId(const std::string& id);

std::vector<std::string> splitMatchExpressions(const std::string& mes);

bool flagToManufacturer(const char *s, uint16_t *out_mfct);

#endif
