/*
 Copyright (C) 2023-2024 Fredrik Öhrström (gpl-3.0-or-later)

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
    static bool load(DriverInfo *di, const std::string &name, const char *content);
    static XMQProceed add_detect(XMQDoc *doc, XMQNodePtr detect, DriverInfo *di);
    static XMQProceed add_use(XMQDoc *doc, XMQNodePtr field, DriverDynamic *dd);
    static XMQProceed add_field(XMQDoc *doc, XMQNodePtr field, DriverDynamic *dd);
    static XMQProceed add_match(XMQDoc *doc, XMQNodePtr match, DriverDynamic *dd);
    static XMQProceed add_combinable(XMQDoc *doc, XMQNodePtr match, DriverDynamic *dd);
    static XMQProceed add_combinable_raw(XMQDoc *doc, XMQNodePtr match, DriverDynamic *dd);

    static XMQProceed add_lookup(XMQDoc *doc, XMQNodePtr lookup, DriverDynamic *dd);
    static XMQProceed add_map(XMQDoc *doc, XMQNodePtr map, DriverDynamic *dd);

    static XMQProceed add_mfct_tpl_status(XMQDoc *doc, XMQNodePtr node, DriverInfo *di);
    static XMQProceed add_mfct_tpl_status_map(XMQDoc *doc, XMQNodePtr map, Translate::Rule *rule);

    const std::string &fileName() { return file_name_; }

private:

    std::string file_name_;
    FieldMatcher *tmp_matcher_;
    Translate::Lookup *tmp_lookup_;
    Translate::Rule *tmp_rule_;
};

#endif
