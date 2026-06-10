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

#include "always.h"
#include "log.h"
#include "wmbus/link_mode.h"

#include <cassert>
#include <cstring>
#include <cinttypes>
#include <set>
#include <string>

struct LinkModeInfo
{
    LinkMode mode;
    const char *name;
    const char *lcname;
    const char *option;
    uint64_t val;
};

LinkModeInfo link_modes_[] = {
#define X(name,lcname,option,val) { LinkMode::name, #name , #lcname, #option, val },
LIST_OF_LINK_MODES
#undef X
};

const char *toString(LinkMode lm)
{
#define X(name,lcname,option,val) if (lm == LinkMode::name) return #lcname;
LIST_OF_LINK_MODES
#undef X

    return "unknown";
}

LinkModeInfo *getLinkModeInfo(LinkMode lm)
{
    for (auto& s : link_modes_)
    {
        if (s.mode == lm)
        {
            return &s;
        }
    }
    assert(0);
    return NULL;
}

LinkModeInfo *getLinkModeInfoFromBit(uint64_t bit)
{
    for (auto& s : link_modes_)
    {
        if (s.val == bit)
        {
            return &s;
        }
    }
    assert(0);
    return NULL;
}

LinkMode isLinkModeOption(const char *arg)
{
    for (auto& s : link_modes_) {
        if (!strcmp(arg, s.option)) {
            return s.mode;
        }
    }
    return LinkMode::UNKNOWN;
}

LinkMode toLinkMode(const char *arg)
{
    for (auto& s : link_modes_) {
        if (!strcmp(arg, s.lcname)) {
            return s.mode;
        }
    }
    return LinkMode::UNKNOWN;
}

LinkModeSet parseLinkModes(std::string m)
{
    LinkModeSet lms;
    char buf[m.length()+1];
    strcpy(buf, m.c_str());
    char *saveptr {};
    const char *tok = strtok_r(buf, ",", &saveptr);
    while (tok != NULL)
    {
        LinkMode lm = toLinkMode(tok);
        if (lm == LinkMode::UNKNOWN)
        {
            error(EXIT_FAILURE, "(wmbus) not a valid link mode: %s\n", tok);
        }
        lms.addLinkMode(lm);
        tok = strtok_r(NULL, ",", &saveptr);
    }
    return lms;
}

bool isValidLinkModes(std::string m)
{
    LinkModeSet lms;
    char buf[m.length()+1];
    strcpy(buf, m.c_str());
    char *saveptr {};
    const char *tok = strtok_r(buf, ",", &saveptr);
    while (tok != NULL)
    {
        LinkMode lm = toLinkMode(tok);
        if (lm == LinkMode::UNKNOWN)
        {
            return false;
        }
        lms.addLinkMode(lm);
        tok = strtok_r(NULL, ",", &saveptr);
    }
    return true;
}

LinkModeSet &LinkModeSet::addLinkMode(LinkMode lm)
{
    for (auto& s : link_modes_) {
        if (s.mode == lm) {
            set_ |= s.val;
        }
    }
    return *this;
}

void LinkModeSet::unionLinkModeSet(LinkModeSet lms)
{
    set_ |= lms.set_;
}

void LinkModeSet::disjunctionLinkModeSet(LinkModeSet lms)
{
    set_ &= lms.set_;
}

bool LinkModeSet::supports(LinkModeSet lms)
{
    // Will return false, if lms is UKNOWN (=0).
    return (set_ & lms.set_) != 0;
}

bool LinkModeSet::has(LinkMode lm)
{
    LinkModeInfo *lmi = getLinkModeInfo(lm);
    return (set_ & lmi->val) != 0;
}

bool LinkModeSet::hasAll(LinkModeSet lms)
{
    return (set_ & lms.set_) == lms.set_;
}

std::string LinkModeSet::hr()
{
    std::string r;
    if (set_ == Any_bit) return "any";
    if (set_ == 0) return "none";
    for (auto& s : link_modes_)
    {
        if (s.mode == LinkMode::Any) continue;
        if (set_ & s.val)
        {
            r += s.lcname;
            r += ",";
        }
    }
    r.pop_back();
    return r;
}

std::string linkModeName(LinkMode link_mode)
{

    for (auto& s : link_modes_) {
        if (link_mode == s.mode) {
            return s.name;
        }
    }
    return "UnknownLinkMode";
}