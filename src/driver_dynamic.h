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

struct DriverDynamic : public MeterCommonImplementation
{
    DriverDynamic(MeterInfo &mi, DriverInfo &di);
    ~DriverDynamic();
    static bool load(DriverInfo *di, const std::string &name, const char *content);
    static XMQProceed add_detect(XMQDoc *doc, XMQNode *detect, DriverInfo *di);
    static XMQProceed add_compact_frame_format(XMQDoc *doc, XMQNode *node, DriverInfo *di);
    static XMQProceed add_use(XMQDoc *doc, XMQNode *field, DriverDynamic *dd);
    static XMQProceed add_field(XMQDoc *doc, XMQNode *field, DriverDynamic *dd);
    static XMQProceed add_match(XMQDoc *doc, XMQNode *match, DriverDynamic *dd);
    static XMQProceed add_combinable(XMQDoc *doc, XMQNode *match, DriverDynamic *dd);
    static XMQProceed add_combinable_raw(XMQDoc *doc, XMQNode *match, DriverDynamic *dd);

    static XMQProceed add_lookup(XMQDoc *doc, XMQNode *lookup, DriverDynamic *dd);
    static XMQProceed add_map(XMQDoc *doc, XMQNode *map, DriverDynamic *dd);
    // Like add_map, but skips the entry if tmp_rule_ already has a map{} for the same bit/value,
    // used to inherit a template's map{} entries without overriding the field's own.
    static XMQProceed add_inherited_map(XMQDoc *doc, XMQNode *map, DriverDynamic *dd);

    // Reusable field templates, see driver/templates/template_field.
    static XMQProceed add_template_field(XMQDoc *doc, XMQNode *template_field, DriverDynamic *dd);

    static XMQProceed add_mfct_tpl_status(XMQDoc *doc, XMQNode *node, DriverInfo *di);
    static XMQProceed add_mfct_tpl_status_map(XMQDoc *doc, XMQNode *map, Translate::Rule *rule);
    static XMQProceed add_default_key(XMQDoc *doc, XMQNode *node, DriverInfo *di);

    const std::string &fileName() { return file_name_; }

private:

    std::string file_name_;
    FieldMatcher *tmp_matcher_;
    Translate::Lookup *tmp_lookup_;
    Translate::Rule *tmp_rule_;
    // Named field templates (driver/templates/template_field), keyed by their name.
    std::map<std::string, XMQNode*> templates_;
    // While parsing a field's lookup{}, the corresponding template's lookup{} node to
    // fall back to for any property (or map{} entries) the field itself did not declare.
    // NULL when the field does not use a template.
    XMQNode *tmp_template_lookup_;
};

#endif
