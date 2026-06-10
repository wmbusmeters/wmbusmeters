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

#ifndef WMBUS_LINK_MODE_H
#define WMBUS_LINK_MODE_H

#include<cinttypes>
#include<set>
#include<string>

// In link mode S1, is used when both the transmitter and receiver are stationary.
// It can be transmitted relatively seldom.

// In link mode T1, the meter transmits a telegram every few seconds or minutes.
// Suitable for drive-by/walk-by collection of meter values.

// Link mode C1 is like T1 but uses less energy when transmitting due to
// a different radio encoding. Also significant is:
// S1/T1 usually uses the A format for the data link layer, more CRCs.
// C1 usually uses the B format for the data link layer, less CRCs = less overhead.

// The im871a can for example receive C1a, but it is unclear if there are any meters that use it.

#define LIST_OF_LINK_MODES \
    X(Any,any,--anylinkmode,(~0UL)) \
    X(MBUS,mbus,--mbus,(1UL<<1))    \
    X(S1,s1,--s1,      (1UL<<2))    \
    X(S1m,s1m,--s1m,   (1UL<<3))    \
    X(S2,s2,--s2,      (1UL<<4))    \
    X(T1,t1,--t1,      (1UL<<5))    \
    X(T2,t2,--t2,      (1UL<<6))    \
    X(C1,c1,--c1,      (1UL<<7))    \
    X(C2,c2,--c2,      (1UL<<8))    \
    X(N1a,n1a,--n1a,   (1UL<<9))    \
    X(N2a,n2a,--n2a,   (1UL<<10))    \
    X(N1b,n1b,--n1b,   (1UL<<11))    \
    X(N2b,n2b,--n2b,   (1UL<<12))    \
    X(N1c,n1c,--n1c,   (1UL<<13))    \
    X(N2c,n2c,--n2c,   (1UL<<14))    \
    X(N1d,n1d,--n1d,   (1UL<<15))    \
    X(N2d,n2d,--n2d,   (1UL<<16))    \
    X(N1e,n1e,--n1e,   (1UL<<17))    \
    X(N2e,n2e,--n2e,   (1UL<<18))    \
    X(N1f,n1f,--n1f,   (1UL<<19))    \
    X(N2f,n2f,--n2f,   (1UL<<20))    \
    X(R2a,r2a,--r2a,   (1UL<<21))    \
    X(R2b,r2b,--r2b,   (1UL<<22))    \
    X(R2c,r2c,--r2c,   (1UL<<23))    \
    X(R2d,r2d,--r2d,   (1UL<<24))    \
    X(R2e,r2e,--r2e,   (1UL<<25))    \
    X(R2f,r2f,--r2f,   (1UL<<26))    \
    X(R2g,r2g,--r2g,   (1UL<<27))    \
    X(R2h,r2h,--r2h,   (1UL<<28))    \
    X(R2i,r2i,--r2i,   (1UL<<29))    \
    X(R2j,r2j,--r2j,   (1UL<<30))    \
    X(LORA,lora,--lora,   (1UL<<31))    \
    X(UNKNOWN,unknown,----,0x0UL)

enum class LinkMode {
#define X(name,lcname,option,val) name,
LIST_OF_LINK_MODES
#undef X
};

#define X(name,lcname,option,val) const uint64_t name##_bit = val;
LIST_OF_LINK_MODES
#undef X

LinkMode toLinkMode(const char *arg);
LinkMode isLinkModeOption(const char *arg);
const char *toString(LinkMode lm);

struct LinkModeSet
{
    // Add the link mode to the set of link modes.
    LinkModeSet &addLinkMode(LinkMode lm);
    void unionLinkModeSet(LinkModeSet lms);
    void disjunctionLinkModeSet(LinkModeSet lms);
    // Does this set support listening to the given link mode set?
    // If this set is C1 and T1 and the supplied set contains just C1,
    // then supports returns true.
    // Or if this set is just T1 and the supplied set contains just C1,
    // then supports returns false.
    // Or if this set is just C1 and the supplied set contains C1 and T1,
    // then supports returns true.
    // Or if this set is S1 and T1, and the supplied set contains C1 and T1,
    // then supports returns true.
    //
    // It will do a bitwise and of the linkmode bits. If the result
    // of the and is not zero, then support returns true.
    bool supports(LinkModeSet lms);
    // Check if this set contains the given link mode.
    bool has(LinkMode lm);
    // Check if all link modes are supported.
    bool hasAll(LinkModeSet lms);
    // Check if any link mode has been set.
    bool empty() { return set_ == 0; }
    // Clear the set to empty.
    void clear() { set_ = 0; }
    // Mark set as all linkmodes!
    void setAll() { set_ = (int)LinkMode::Any; }
    // For bit counting etc.
    int asBits() { return set_; }

    // Return a human readable string.
    std::string hr();

    LinkModeSet() { }
    LinkModeSet(uint64_t s) : set_(s) {}

private:

    uint64_t set_ {};
};

LinkModeSet parseLinkModes(std::string modes);
bool isValidLinkModes(std::string modes);

std::string linkModeName(LinkMode link_mode);

#endif
