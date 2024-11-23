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

#include"meters_common_implementation.h"

#include"driver_dynamic.h"
#include"xmq.h"

#include<string.h>

string check_driver_name(const char *name, string file);
MeterType check_meter_type(const char *meter_type_s, string file);
string check_default_fields(const char *fields, string file);
void check_detection_triplets(DriverInfo *di, string file);

string check_field_name(const char *name, DriverDynamic *dd);
string check_field_info(const char *info, DriverDynamic *dd);
Quantity check_field_quantity(const char *quantity_s, DriverDynamic *dd);
VifScaling check_vif_scaling(const char *vif_scaling_s, DriverDynamic *dd);
DifSignedness check_dif_signedness(const char *dif_signedness_s, DriverDynamic *dd);
PrintProperties check_print_properties(const char *print_properties_s, DriverDynamic *dd);
string get_translation(XMQDoc *doc, XMQNode *node, string name, string lang);
string check_calculate(const char *formula, DriverDynamic *dd);
Unit check_display_unit(const char *display_unit, DriverDynamic *dd);
double check_force_scale(const char *force_scale, DriverDynamic *dd);

bool checked_set_difvifkey(const char *difvifkey_s, FieldMatcher *fm, DriverDynamic *dd);
void checked_set_measurement_type(const char *measurement_type_s, FieldMatcher *fm, DriverDynamic *dd);
void checked_set_vif_range(const char *vif_range_s, FieldMatcher *fm, DriverDynamic *dd);
void checked_set_storagenr_range(const char *storagenr_range_s, FieldMatcher *fm, DriverDynamic *dd);
void checked_set_tariffnr_range(const char *tariffnr_range_s, FieldMatcher *fm, DriverDynamic *dd);
void checked_set_subunitnr_range(const char *subunitnr_range_s, FieldMatcher *fm, DriverDynamic *dd);
Translate::MapType checked_map_type(const char *map_type_s, DriverDynamic *dd);
uint64_t checked_mask_bits(const char *mask_bits_s, DriverDynamic *dd);
uint64_t checked_value(const char *value_s, DriverDynamic *dd);
TestBit checked_test_type(const char *test_s, DriverDynamic *dd);
void checked_add_vif_combinable(const char *vif_range_s, FieldMatcher *fm, DriverDynamic *dd);

const char *line = "-------------------------------------------------------------------------------";

bool DriverDynamic::load(DriverInfo *di, const string &file_name, const char *content)
{
    if (!content)
    {
        if (!endsWith(file_name, ".xmq")) return false;
        if (!checkFileExists(file_name.c_str())) return false;
    }

    string file = file_name;
    XMQDoc *doc = xmqNewDoc();

    bool ok = false;

    if (!content)
    {
        vector<char> buf;
        ok = loadFile(file.c_str(), &buf);
        if (!ok)
        {
            warning("(driver) error cannot load wmbusmeters driver file %s\n", file.c_str());
            return false;
        }
        ok = xmqParseBuffer(doc, buf.data(), buf.data()+buf.size(), NULL, 0);
        di->setDynamicSource(string(buf.begin(), buf.end()));
    }
    else
    {
        file = "builtin";
        ok = xmqParseBuffer(doc, content, content+strlen(content), NULL, 0);
        di->setDynamicSource(content);
    }

    if (!ok) {
        warning("(driver) error loading wmbusmeters driver file %s\n%s\n%s\n",
                file.c_str(),
                xmqDocError(doc),
                line);

        return false;
    }

    try
    {
        string name = check_driver_name(xmqGetString(doc, "/driver/name"), file);
        di->setName(name);

        MeterType meter_type = check_meter_type(xmqGetString(doc, "/driver/meter_type"), file);
        di->setMeterType(meter_type);

        string default_fields = check_default_fields(xmqGetString(doc, "/driver/default_fields"), file);
        di->setDefaultFields(default_fields);

        if (!content)
        {
            verbose("(driver) loading driver %s from file %s\n", name.c_str(), file.c_str());
        }

        di->setDynamic(file, doc);

        xmqForeach(doc, "/driver/detect/mvt", (XMQNodeCallback)add_detect, di);

        check_detection_triplets(di, file);

        di->setConstructor([](MeterInfo& mi, DriverInfo& di){ return shared_ptr<Meter>(new DriverDynamic(mi, di)); });

        return true;
    }
    catch (...)
    {
        xmqFreeDoc(doc);
        di->setDynamic(file, NULL);
        return false;
    }
}

DriverDynamic::DriverDynamic(MeterInfo &mi, DriverInfo &di) :
    MeterCommonImplementation(mi, di), file_name_(di.getDynamicFileName())
{
    XMQDoc *doc = NULL;
    try
    {
        doc = di.getDynamicDriver();
        assert(doc);

        verbose("(driver) constructing driver %s from already loaded file %s\n",
                di.name().str().c_str(),
                fileName().c_str());

        xmqForeach(doc, "/driver/library/use", (XMQNodeCallback)add_use, this);
        xmqForeach(doc, "/driver/fields/field", (XMQNodeCallback)add_field, this);
    }
    catch(...)
    {
        xmqFreeDoc(doc);
    }
}

DriverDynamic::~DriverDynamic()
{
}

XMQProceed DriverDynamic::add_detect(XMQDoc *doc, XMQNode *detect, DriverInfo *di)
{
    string mvt = xmqGetStringRel(doc, ".", detect);

    auto fields = splitString(mvt, ',');
    if (fields.size() != 3)
    {
        warning("(driver) error in %s, wrong number of fields in mvt triple: mvt = %s\n"
                "%s\n"
                "There should be three fields, for example: mvt = AAA,07,05\n"
                "%s\n",
                di->getDynamicFileName().c_str(),
                mvt.c_str(),
                line,
                line);
        return XMQ_CONTINUE;
    }

    string mfct = fields[0];
    int mfct_code = 0;
    long version = strtol(fields[1].c_str(), NULL, 16);
    long type = strtol(fields[2].c_str(), NULL, 16);

    if (mfct.length() == 3)
    {
        char a = mfct[0];
        char b = mfct[1];
        char c = mfct[2];
        if (!(a >= 'A' && a <= 'Z' &&
              b >= 'A' && b <= 'Z' &&
              c >= 'A' && c <= 'Z'))
        {
            warning("(driver) error in %s, bad manufacturer in mvt triplet: %s\n"
                    "%s\n"
                    "Use 3 uppercase characters A-Z or 4 lowercase hex chars.\n"
                    "%s\n",
                    di->getDynamicFileName().c_str(),
                    mfct.c_str(),
                    line,
                    line);
            return XMQ_CONTINUE;
        }
        mfct_code = toMfctCode(a, b, c);
    }
    else
    {
        char *eptr;
        mfct_code = strtol(mfct.c_str(), &eptr, 16);
        if (*eptr)
        {
            warning("(driver) error in %s, bad manufacturer in mvt triplet: %s\n"
                    "%s\n"
                    "Use 3 uppercase characters A-Z or 4 lowercase hex chars.\n"
                    "%s\n",
                    di->getDynamicFileName().c_str(),
                    mfct.c_str(),
                    line,
                    line);
            return XMQ_CONTINUE;
        }
    }

    if (version > 255 || version < 0)
    {
        warning("(driver) error in %s, bad version in mvt triplet: %02x\n"
                "%s\n"
                "The version must be a hex value from 00 to ff.\n"
                "%s\n",
                di->getDynamicFileName().c_str(),
                version,
                line,
                line);
        return XMQ_CONTINUE;
    }

    if (type > 255 || type < 0)
    {
        warning("(driver) error in %s, bad type in mvt triplet: %02x\n"
                "%s\n"
                "The type must be a hex value from 00 to ff.\n"
                "%s\n",
                di->getDynamicFileName().c_str(),
                type,
                line,
                line);
        return XMQ_CONTINUE;
    }

    string mfct_flag = manufacturerFlag(mfct_code);
    debug("(driver) register detection %s %s %2x %02x\n",
          di->getDynamicFileName().c_str(),
          mfct_flag.c_str(),
          version,
          type);

    di->addDetection(mfct_code, type, version);
    return XMQ_CONTINUE;
}

XMQProceed DriverDynamic::add_use(XMQDoc *doc, XMQNode *field, DriverDynamic *dd)
{
    string name = xmqGetStringRel(doc, ".", field);
    bool ok = dd->addOptionalLibraryFields(name);
    if (!ok)
    {
        warning("(driver) error in %s, unknown library field: %s \n",
                dd->fileName().c_str(),
                name.c_str());
    }

    return XMQ_CONTINUE;
}

XMQProceed DriverDynamic::add_field(XMQDoc *doc, XMQNode *field, DriverDynamic *dd)
{
    // The field name must be supplied without a unit ie total (not total_m3) since units are managed by wmbusmeters.
    string name = check_field_name(xmqGetStringRel(doc, "name", field), dd);

    // The quantity ie Volume, gives the default unit (m3) for the field. The unit can be overriden with display_unit.
    Quantity quantity = check_field_quantity(xmqGetStringRel(doc, "quantity", field), dd);

    // Text fields are either version strings or lookups from status bits.
    // All other fields are numeric, ie they have a unit. This also includes date and datetime.
    bool is_numeric = quantity != Quantity::Text;

    // The vif scaling is by default Auto but can be overriden for pesky fields.
    VifScaling vif_scaling = check_vif_scaling(xmqGetStringRel(doc, "vif_scaling", field), dd);

    // The dif signedness is by default Signed but can be overriden for pesky fields.
    DifSignedness dif_signedness = check_dif_signedness(xmqGetStringRel(doc, "dif_signedness", field), dd);

    // The properties are by default empty but can be specified for specific fields.
    PrintProperties properties = check_print_properties(xmqGetStringRel(doc, "attributes", field), dd);

    // The info fields explains what the value is for. Ie. is storage 1 the previous day or month value etc.
    string info = check_field_info(xmqGetStringRel(doc, "info", field), dd);

    // The calculate formula is optional.
    string calculate = check_calculate(xmqGetStringRel(doc, "calculate", field), dd);

    // The display unit is usually based on the quantity. But you can override it.
    Unit display_unit = check_display_unit(xmqGetStringRel(doc, "display_unit", field), dd);

    // A field can force a scale factor. Defaults to 1.0 but you can override
    // with 1.123 or 1/32 or 0.33333 or 3.14/2.5
    double force_scale = check_force_scale(xmqGetStringRel(doc, "force_scale", field), dd);

    // Now find all matchers.
    FieldMatcher match = FieldMatcher::build();
    dd->tmp_matcher_ = &match;
    int num_matches = xmqForeachRel(doc, "match", (XMQNodeCallback)add_match, dd, field);
    // Check if there were any matches at all, if not, then disable the matcher.
    match.active = num_matches > 0;

    // Now find all matchers.
    Translate::Lookup lookup = Translate::Lookup();
    /*
        .add(Translate::Rule("ERROR_FLAGS", Translate::Type::BitToString)
        .set(MaskBits(0x000f))
        .set(DefaultMessage("OK"))
        .add(Translate::Map(0x01 ,"DRY", TestBit::Set))
        .add(Translate::Map(0x02 ,"REVERSE", TestBit::Set))
        .add(Translate::Map(0x04 ,"LEAK", TestBit::Set))
        .add(Translate::Map(0x08 ,"BURST", TestBit::Set))
        ));
    */
    dd->tmp_lookup_ = &lookup;
    int num_lookups = xmqForeachRel(doc, "lookup", (XMQNodeCallback)add_lookup, dd, field);

    if (is_numeric)
    {
        if (calculate == "")
        {
            dd->addNumericFieldWithExtractor(
                name,
                info,
                properties,
                quantity,
                vif_scaling,
                dif_signedness,
                match,
                display_unit,
                force_scale
                );
        }
        else
        {
            if (!match.active)
            {
                dd->addNumericFieldWithCalculator(
                    name,
                    info,
                    properties,
                    quantity,
                    calculate,
                    display_unit
                    );
            }
            else
            {
                dd->addNumericFieldWithCalculatorAndMatcher(
                    name,
                    info,
                    properties,
                    quantity,
                    calculate,
                    match,
                    display_unit
                    );
            }
        }
    }
    else
    {
        if (num_lookups > 0)
        {
            dd->addStringFieldWithExtractorAndLookup(
                name,
                info,
                properties,
                match,
                lookup
                );
        }
        else
        {
            dd->addStringFieldWithExtractor(
                name,
                info,
                properties,
                match
                );
        }
    }
    return XMQ_CONTINUE;
}

XMQProceed DriverDynamic::add_match(XMQDoc *doc, XMQNode *match, DriverDynamic *dd)
{
    FieldMatcher *fm = dd->tmp_matcher_;

    if (checked_set_difvifkey(xmqGetStringRel(doc, "difvifkey", match), fm, dd)) return XMQ_CONTINUE;

    checked_set_measurement_type(xmqGetStringRel(doc, "measurement_type", match), fm, dd);

    checked_set_vif_range(xmqGetStringRel(doc, "vif_range", match), fm, dd);

    checked_set_storagenr_range(xmqGetStringRel(doc, "storage_nr", match), fm, dd);
    checked_set_tariffnr_range(xmqGetStringRel(doc, "tariff_nr", match), fm, dd);
    checked_set_subunitnr_range(xmqGetStringRel(doc, "subunit_nr", match), fm, dd);

    xmqForeachRel(doc, "add_combinable", (XMQNodeCallback)add_combinable, dd, match);

    return XMQ_CONTINUE;
}

XMQProceed DriverDynamic::add_combinable(XMQDoc *doc, XMQNode *match, DriverDynamic *dd)
{
    FieldMatcher *fm = dd->tmp_matcher_;

    checked_add_vif_combinable(xmqGetStringRel(doc, ".", match), fm, dd);

    return XMQ_CONTINUE;
}

/**
   add_map:
   Add a mapping from a value (bits,index,decimal) to a string name.

   map {
       name  = SURGE
       info  = 'Unexpected increase in pressure in relation to average pressure.'
       value = 0x02
       test  = set
   }
*/
XMQProceed DriverDynamic::add_map(XMQDoc *doc, XMQNode *map, DriverDynamic *dd)
{
    const char *name = xmqGetStringRel(doc, "name", map);
    uint64_t value = checked_value(xmqGetStringRel(doc, "value", map), dd);
    TestBit test_type = checked_test_type(xmqGetStringRel(doc, "test", map), dd);

    dd->tmp_rule_->add(Translate::Map(value, name, test_type));

    return XMQ_CONTINUE;
}

/**
    add_lookup:
    Add a lookup from bits,index or decimal to a sequence of string tokens.
    Or fallback to the name (ERROR_FLAGS_8) suffixed by the untranslateable bits.

    lookup {
        name            = ERROR_FLAGS
        map_type        = BitToString
        mask_bits       = 0xffff
        default_message = OK
        map { } map {}
    }
*/
XMQProceed DriverDynamic::add_lookup(XMQDoc *doc, XMQNode *lookup, DriverDynamic *dd)
{
    const char *name = xmqGetStringRel(doc, "name", lookup);
    Translate::MapType map_type = checked_map_type(xmqGetStringRel(doc, "map_type", lookup), dd);
    uint64_t mask_bits = checked_mask_bits(xmqGetStringRel(doc, "mask_bits", lookup), dd);
    const char *default_message = xmqGetStringRel(doc, "default_message", lookup);

    if (default_message == NULL) default_message = "";

    Translate::Rule rule = Translate::Rule(name, map_type);
    dd->tmp_rule_ = &rule;

    rule.set(MaskBits(mask_bits));
    rule.set(DefaultMessage(default_message));

    xmqForeachRel(doc, "map", (XMQNodeCallback)add_map, dd, lookup);

    dd->tmp_lookup_->add(rule);

    return XMQ_CONTINUE;
}

string check_driver_name(const char *name, string file)
{
    if (!name)
    {
        warning("(driver) error in %s, cannot find: driver/name\n"
                "%s\n"
                "A driver file looks like this: driver { name = abc123 ... }\n"
                "%s\n",
                file.c_str(),
                line,
                line);
        throw 1;
    }

    if (!is_lowercase_alnum_text(name))
    {
        warning("(driver) error in %s, bad driver name: %s\n"
                "%s\n"
                "The driver name must consist of lower case ascii a-z and digits 0-9.\n"
                "%s\n",
                file.c_str(),
                name,
                line,
                line);
        throw 1;
    }

    return name;
}

MeterType check_meter_type(const char *meter_type_s, string file)
{
    if (!meter_type_s)
    {
        warning("(driver) error in %s, cannot find: driver/meter_type\n"
                "%s\n"
                "Remember to add: meter_type = ...\n"
                "Available meter types are:\n%s\n"
                "%s\n",
                file.c_str(),
                line,
                availableMeterTypes(),
                line);
        throw 1;
    }

    MeterType meter_type = toMeterType(meter_type_s);

    if (meter_type == MeterType::UnknownMeter)
    {
        warning("(driver) error in %s, unknown meter type: %s\n"
                "%s\n"
                "Available meter types are:\n%s\n"
                "%s\n",
                file.c_str(),
                meter_type_s,
                line,
                availableMeterTypes(),
                line);
        throw 1;
    }

    return meter_type;
}

string check_default_fields(const char *default_fields, string file)
{
    if (!default_fields)
    {
        warning("(driver) error in %s, cannot find: driver/default_fields\n"
                "%s\n"
                "Remember to add for example: default_fields = name,id,total_m3,timestamp\n"
                "Where you change total_m3 to your meters most important field.\n"
                "%s\n",
                file.c_str(),
                line,
                line);
        throw 1;
    }

    return default_fields;
}

void check_detection_triplets(DriverInfo *di, string file)
{
    if (di->detect().size() == 0)
    {
        warning("(driver) error in %s, cannot find any detection triplets: driver/detect/mvt\n"
                "%s\n"
                "Remember to add: detect { mvt = AAA,05,07 mvt = AAA,06,07 ... }\n"
                "The triplets consists of MANUFACTURER,VERSION,TYPE\n"
                "You can see these values when listening to all meters.\n"
                "The manufacturer can be given as three uppercase characters A-Z\n"
                "or as 4 lower case hex digits.\n"
                "%s\n",
                file.c_str(),
                line,
                line);
        throw 1;
    }
}

string check_field_name(const char *name, DriverDynamic *dd)
{
    if (!name)
    {
        warning("(driver) error in %s, cannot find: driver/fields/field/name\n"
                "%s\n"
                "Remember to add for example: field { name = total ... }\n"
                "%s\n",
                dd->fileName().c_str(),
                line,
                line);
        throw 1;
    }

    string vname;
    Unit u;
    if (extractUnit(string(name), &vname, &u))
    {
        warning("(driver) error in %s, bad field name %s (field names should not have units)\n"
                "%s\n"
                "The field name should not have a unit since units are added automatically.\n"
                "Either indirectly based on the quantity or directly based on the display_unit.\n"
                "%s\n",
                dd->fileName().c_str(),
                name,
                line,
                line);
        throw 1;
    }

    return name;
}

string check_field_info(const char *info, DriverDynamic *dd)
{
    if (!info) return "";

    return info;
}

Quantity check_field_quantity(const char *quantity_s, DriverDynamic *dd)
{
    if (!quantity_s)
    {
        warning("(driver) error in %s, cannot find: driver/fields/field/quantity\n"
                "%s\n"
                "Remember to add for example: field { quantity = Volume ... }\n"
                "Available quantities:\n%s\n"
                "%s\n",
                dd->fileName().c_str(),
                line,
                availableQuantities(),
                line);
        throw 1;
    }

    Quantity quantity = toQuantity(quantity_s);

    if (quantity == Quantity::Unknown)
    {
        warning("(driver) error in %s, bad quantity: %s\n"
                "%s\n"
                "Available quantities:\n"
                "%s\n"
                "%s\n",
                dd->fileName().c_str(),
                quantity_s,
                line,
                availableQuantities(),
                line);
        throw 1;
    }

    return quantity;
}

VifScaling check_vif_scaling(const char *vif_scaling_s, DriverDynamic *dd)
{
    if (!vif_scaling_s)
    {
        return VifScaling::Auto;
    }

    VifScaling vif_scaling = toVifScaling(vif_scaling_s);

    if (vif_scaling == VifScaling::Unknown)
    {
        warning("(driver) error in %s, bad vif scaling: %s\n",
                "%s\n"
                "Available vif scalings:\n"
                "Auto\n"
                "None\n"
                "%s\n",
                dd->fileName().c_str(),
                vif_scaling_s,
                line,
                line);
        throw 1;
    }

    return vif_scaling;
}

DifSignedness check_dif_signedness(const char *dif_signedness_s, DriverDynamic *dd)
{
    if (!dif_signedness_s)
    {
        return DifSignedness::Signed;
    }

    DifSignedness dif_signedness = toDifSignedness(dif_signedness_s);

    if (dif_signedness == DifSignedness::Unknown)
    {
        warning("(driver) error in %s, bad dif signedness: %s\n",
                "%s\n"
                "Available dif signedness:\n"
                "Signed\n"
                "Unsigned\n"
                "%s\n",
                dd->fileName().c_str(),
                dif_signedness_s,
                line,
                line);
        throw 1;
    }

    return dif_signedness;
}

PrintProperties check_print_properties(const char *print_properties_s, DriverDynamic *dd)
{
    if (!print_properties_s)
    {
        return PrintProperties(0);
    }

    PrintProperties print_properties = toPrintProperties(print_properties_s);
    if (print_properties.hasUnknown())
    {
        warning("(driver) error in %s, unknown attributes: %s\n",
                dd->fileName().c_str(),
                print_properties_s);
        throw 1;
    }

    return print_properties;
}

string get_translation(XMQDoc *doc, XMQNode *node, string name, string lang)
{
    string xpath = name+"/"+lang;
    const char *txt = xmqGetStringRel(doc, xpath.c_str(), node);
    if (!txt)
    {
        xpath = name+"/en";
        txt = xmqGetStringRel(doc, xpath.c_str(), node);
        if (!txt)
        {
            txt = "";
        }
    }

    return txt;
}

string check_calculate(const char *formula, DriverDynamic *dd)
{
    if (!formula) return "";

    return formula;
}

Unit check_display_unit(const char *display_unit_s, DriverDynamic *dd)
{
    if (!display_unit_s)
    {
        return Unit::Unknown;
    }

    Unit u = toUnit(display_unit_s);
    if (u == Unit::Unknown)
    {
        warning("(driver) error in %s, unknown display unit: %s\n"
                "Available units:\n"
                "%s\n",
                dd->fileName().c_str(),
                display_unit_s,
                availableUnits());
        throw 1;
    }

    return u;
}

double check_force_scale(const char *force_scale, DriverDynamic *dd)
{
    if (force_scale == 0) return 1.0;

    const char *start = force_scale;
    const char *stop = force_scale+strlen(force_scale);
    while (start < stop && (*start == ' ' || *start == '\n')) start++;
    while (start < stop && (*stop == ' ' || *stop == '\n')) stop--;

    const char *slash = strchr(force_scale, '/');
    if (slash == NULL)
    {
        char *end;
        double d = strtod(start, &end);
        if (end != stop)
        {
            warning("(driver) error in %s, unparseable force_scale: %s\n"
                    "You can force scales such as:\n"
                    "3.14\n"
                    "2/3\n"
                    "12.5\n"
                    "12.5/5.3\n",
                    dd->fileName().c_str(),
                    force_scale);
            throw 1;
        }
        return d;
    }

    const char *numerator_stop = slash;
    const char *denominator_start = slash+1;
    while (start < numerator_stop && (*numerator_stop == ' ' || *numerator_stop == '\n')) numerator_stop--;
    while (denominator_start < stop && (*denominator_start == ' ' || *denominator_start == '\n')) denominator_start++;

    char *end;
    double num = strtod(start, &end);
    if (end != numerator_stop)
    {
        warning("(driver) error in %s, unparseable numerator in force_scale: %s\n"
                "You can force scales such as:\n"
                "3.14\n"
                "2/3\n"
                "12.5\n"
                "12.5/5.3\n",
                dd->fileName().c_str(),
                force_scale);
        throw 1;
    }

    double denom = strtod(denominator_start, &end);
    if (end != stop)
    {
        warning("(driver) error in %s, unparseable denominator in force_scale: %s\n"
                "You can force scales such as:\n"
                "3.14\n"
                "2/3\n"
                "12.5\n"
                "12.5/5.3\n",
                dd->fileName().c_str(),
                force_scale);
        throw 1;
    }

    double d = num / denom;
    return d;
}

bool checked_set_difvifkey(const char *difvifkey_s, FieldMatcher *fm, DriverDynamic *dd)
{
    if (!difvifkey_s) return false;

    bool invalid_hex = false;
    bool hex = isHexStringStrict(difvifkey_s, &invalid_hex);

    if (!hex || invalid_hex)
    {
        warning("(driver) error in %s, bad divfikey: %s\n"
                "%s\n"
                "Should be all hex.\n"
                "%s\n",
                dd->fileName().c_str(),
                difvifkey_s,
                line,
                line);
        throw 1;
    }

    fm->set(DifVifKey(difvifkey_s));

    return true;
}

void checked_set_measurement_type(const char *measurement_type_s, FieldMatcher *fm, DriverDynamic *dd)
{
    if (!measurement_type_s)
    {
        warning("(driver) error in %s, cannot find: driver/fields/field/match/measurement_type\n"
                "%s\n"
                "Remember to add for example: match { measurement_type = Instantaneous ... }\n"
                "Available measurement types:\n"
                "Instantaneous\n"
                "Minimum\n"
                "Maximum\n"
                "AtError\n"
                "Any\n"
                "%s\n",
                dd->fileName().c_str(),
                line,
                line);
        throw 1;
    }

    MeasurementType measurement_type = toMeasurementType(measurement_type_s);

    if (measurement_type == MeasurementType::Unknown)
    {
        warning("(driver) error in %s, bad measurement_type: %s\n"
                "%s\n"
                "Available measurement types:\n"
                "Instantaneous\n"
                "Minimum\n"
                "Maximum\n"
                "AtError\n"
                "Any\n"
                "%s\n",
                dd->fileName().c_str(),
                measurement_type_s,
                line,
                line);
        throw 1;
    }

    fm->set(measurement_type);
}

void checked_set_vif_range(const char *vif_range_s, FieldMatcher *fm, DriverDynamic *dd)
{
    if (!vif_range_s)
    {
        warning("(driver) error in %s, cannot find: driver/fields/field/match/vif_range\n"
                "%s\n"
                "Remember to add for example: match { ... vif_range = ReturnTemperature ... }\n"
                "Available vif ranges:\n"
                "%s\n"
                "%s\n",
                dd->fileName().c_str(),
                line,
                availableVIFRanges().c_str(),
                line);
        throw 1;
    }

    VIFRange vif_range = toVIFRange(vif_range_s);

    if (vif_range == VIFRange::None)
    {
        warning("(driver) error in %s, bad vif_range: %s\n"
                "%s\n"
                "Available vif ranges:\n"
                "%s\n"
                "%s\n",
                dd->fileName().c_str(),
                vif_range_s,
                line,
                availableVIFRanges().c_str(),
                line);
        throw 1;
    }

    fm->set(vif_range);
}

void checked_set_storagenr_range(const char *storagenr_range_s, FieldMatcher *fm, DriverDynamic *dd)
{
    if (!storagenr_range_s) return;

    auto fields = splitString(storagenr_range_s, ',');
    bool ok = isNumber(fields[0]);
    if (fields.size() > 1)
    {
        ok &= isNumber(fields[1]);
    }
    if (!ok || fields.size() > 2)
    {
        warning("(driver) error in %s, bad storagenr_range: %s\n"
                "%s\n",
                dd->fileName().c_str(),
                storagenr_range_s,
                line);
        throw 1;
    }

    if (fields.size() == 1)
    {
        fm->set(StorageNr(atoi(fields[0].c_str())));
    }
    else
    {
        fm->set(StorageNr(atoi(fields[0].c_str())),
                StorageNr(atoi(fields[1].c_str())));
    }
}

void checked_set_tariffnr_range(const char *tariffnr_range_s, FieldMatcher *fm, DriverDynamic *dd)
{
    if (!tariffnr_range_s) return;

    auto fields = splitString(tariffnr_range_s, ',');
    bool ok = isNumber(fields[0]);
    if (fields.size() > 1)
    {
        ok &= isNumber(fields[1]);
    }
    if (!ok || fields.size() > 2)
    {
        warning("(driver) error in %s, bad tariffnr_range: %s\n"
                "%s\n",
                dd->fileName().c_str(),
                tariffnr_range_s,
                line);
        throw 1;
    }

    if (fields.size() == 1)
    {
        fm->set(TariffNr(atoi(fields[0].c_str())));
    }
    else
    {
        fm->set(TariffNr(atoi(fields[0].c_str())),
                TariffNr(atoi(fields[1].c_str())));
    }
}

void checked_set_subunitnr_range(const char *subunitnr_range_s, FieldMatcher *fm, DriverDynamic *dd)
{
    if (!subunitnr_range_s) return;

    auto fields = splitString(subunitnr_range_s, ',');
    bool ok = isNumber(fields[0]);
    if (fields.size() > 1)
    {
        ok &= isNumber(fields[1]);
    }
    if (!ok || fields.size() > 2)
    {
        warning("(driver) error in %s, bad subunitnr_range: %s\n"
                "%s\n",
                dd->fileName().c_str(),
                subunitnr_range_s,
                line);
        throw 1;
    }

    if (fields.size() == 1)
    {
        fm->set(SubUnitNr(atoi(fields[0].c_str())));
    }
    else
    {
        fm->set(SubUnitNr(atoi(fields[0].c_str())),
                SubUnitNr(atoi(fields[1].c_str())));
    }
}


void checked_add_vif_combinable(const char *vif_combinable_s, FieldMatcher *fm, DriverDynamic *dd)
{
    if (!vif_combinable_s) return;

    VIFCombinable vif_combinable = toVIFCombinable(vif_combinable_s);

    if (vif_combinable == VIFCombinable::None)
    {
        warning("(driver) error in %s, bad vif_combinable: %s\n"
                "%s\n"
                "Available vif combinables:\n"
                "%s\n"
                "%s\n",
                dd->fileName().c_str(),
                vif_combinable_s,
                line,
                availableVIFCombinables().c_str(),
                line);
        throw 1;
    }

    fm->add(vif_combinable);
}

Translate::MapType checked_map_type(const char *map_type_s, DriverDynamic *dd)
{
    if (!map_type_s)
    {
        warning("(driver) error in %s, cannot find: driver/fields/field/lookup/map_type\n"
                "%s\n"
                "Remember to add for example: lookup { map_type = BitToString ... }\n"
                "Available map types:\n"
                "BitToString\n"
                "IndexToString\n"
                "DecimalsToString\n"
                "%s\n",
                dd->fileName().c_str(),
                line,
                line);
        throw 1;
    }

    Translate::MapType map_type = toMapType(map_type_s);

    if (map_type == Translate::MapType::Unknown)
    {
        warning("(driver) error in %s, bad map_type: %s\n"
                "%s\n"
                "Available map types:\n"
                "BitToString\n"
                "IndexToString\n"
                "DecimalToString\n"
                "%s\n",
                dd->fileName().c_str(),
                map_type_s,
                line,
                line);
        throw 1;
    }

    return map_type;
}


uint64_t checked_mask_bits(const char *mask_bits_s, DriverDynamic *dd)
{
    if (!mask_bits_s)
    {
        warning("(driver) error in %s, cannot find: driver/fields/field/lookup/mask_bitse\n"
                "%s\n"
                "Remember to add for example: lookup { mask_bits = 0x00ff ... }\n"
                "%s\n",
                dd->fileName().c_str(),
                line,
                line);
        throw 1;
    }

    uint64_t mask = strtol(mask_bits_s, NULL, 16);

    return mask;
}

uint64_t checked_value(const char *value_s, DriverDynamic *dd)
{
    if (!value_s)
    {
        warning("(driver) error in %s, cannot find: driver/fields/field/lookup/map/value\n"
                "%s\n"
                "Remember to add for example: lookup { map { ... value = 0x01 ... }}\n"
                "%s\n",
                dd->fileName().c_str(),
                line,
                line);
        throw 1;
    }

    uint64_t value = strtol(value_s, NULL, 16);

    return value;
}

TestBit checked_test_type(const char *test_s, DriverDynamic *dd)
{
    if (!test_s)
    {
        warning("(driver) error in %s, cannot find: driver/fields/field/lookup/map/test\n"
                "%s\n"
                "Remember to add for example: lookup { map { test = Set }  }\n"
                "Available test types:\n"
                "Set\n"
                "NotSet\n"
                "%s\n",
                dd->fileName().c_str(),
                line,
                line);
        throw 1;
    }

    TestBit test_type = toTestBit(test_s);

    if (test_type == TestBit::Unknown)
    {
        warning("(driver) error in %s, bad test: %s\n"
                "%s\n"
                "Available test types:\n"
                "Set\n"
                "NotSet\n"
                "%s\n",
                dd->fileName().c_str(),
                test_s,
                line,
                line);
        throw 1;
    }

    return test_type;
}
