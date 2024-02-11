/*
 Copyright (C) 2023 Fredrik Öhrström (gpl-3.0-or-later)

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

#ifndef DRIVER_LOADER_H_
#define DRIVER_LOADER_H_

#include "meters_common_implementation.h"

struct DriverDynamic : public virtual MeterCommonImplementation
{
    DriverDynamic(MeterInfo &mi, DriverInfo &di);
    ~DriverDynamic();
    static bool load(DriverInfo *di, const string &name, const char *content);
    static XMQProceed add_detect(XMQDoc *doc, XMQNode *detect, DriverInfo *di);
    static XMQProceed add_use(XMQDoc *doc, XMQNode *field, DriverDynamic *dd);
    static XMQProceed add_field(XMQDoc *doc, XMQNode *field, DriverDynamic *dd);
    static XMQProceed add_match(XMQDoc *doc, XMQNode *match, DriverDynamic *dd);

    const string &fileName() { return file_name_; }

private:

    string file_name_;
    FieldMatcher *tmp_matcher_;
};

#endif
