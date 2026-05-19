/*
 Copyright (C) 2019-2026 Fredrik Öhrström (gpl-3.0-or-later)

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

#include"formula.h"
#include"util.h"

#include<string>
#include<unistd.h>
#include<vector>

using namespace std;

int main(int argc, char **argv)
{
    char buf[4096];
    string input;

    if (argc > 1 && argv[1][0] != 0)
    {
        vector<char> contents;
        loadFile(string(argv[1]), &contents);
        input.assign(contents.begin(), contents.end());
    }
    else
    {
        for (;;) {
            ssize_t len = read(0, buf, sizeof(buf));
            if (len <= 0) break;
            input.append(buf, len);
        }
    }

    // Fuzz the numeric formula parser and evaluator.
    {
        Formula *f = newFormula();
        f->parse(nullptr, input);
        if (f->valid())
            f->calculate(Unit::KWH);
        delete f;
    }

    // Fuzz the string interpolator (parses embedded {...} subformulas).
    {
        StringInterpolator *si = newStringInterpolator();
        si->parse(nullptr, input);
        si->apply(nullptr, nullptr);
        delete si;
    }

    return 0;
}
