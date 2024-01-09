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

#include"meters_common_implementation.h"

#include"driver_dynamic.h"
#include"xmq.h"

string check_driver_name(const char *name, string file);
MeterType check_meter_type(const char *meter_type_s, string file);
string check_default_fields(const char *fields, string file);
void check_detection_triplets(DriverInfo *di, string file);

string check_field_name(const char *name, DriverDynamic *dd);
Quantity check_field_quantity(const char *quantity_s, DriverDynamic *dd);
VifScaling check_vif_scaling(const char *vif_scaling_s, DriverDynamic *dd);
PrintProperties check_print_properties(const char *print_properties_s, DriverDynamic *dd);
string get_translation(XMQDoc *doc, XMQNode *node, string name, string lang);
string check_calculate(const char *formula, DriverDynamic *dd);
Unit check_display_unit(const char *display_unit, DriverDynamic *dd);

void check_set_measurement_type(const char *measurement_type_s, FieldMatcher *fm, DriverDynamic *dd);
void check_set_vif_range(const char *vif_range_s, FieldMatcher *fm, DriverDynamic *dd);

const char *line = "-------------------------------------------------------------------------------";

bool DriverDynamic::load(DriverInfo *di, const string &file)
{
    if (!endsWith(file, ".xmq")) return false;
    if (!checkFileExists(file.c_str())) return false;

    XMQDoc *doc = xmqNewDoc();

    bool ok = xmqParseFile(doc, file.c_str(), NULL);

    if (!ok) {
        warning("(driver) error loading wmbusmeters driver file %s\n%s\n%s\n",
                file.c_str(),
                xmqDocError(doc),
                line);

        return false;
    }

    try
    {
        string name = check_driver_name(xmqGetString(doc, NULL, "/driver/name"), file);
        di->setName(name);

        MeterType meter_type = check_meter_type(xmqGetString(doc, NULL, "/driver/meter_type"), file);
        di->setMeterType(meter_type);

        string default_fields = check_default_fields(xmqGetString(doc, NULL, "/driver/default_fields"), file);
        di->setDefaultFields(default_fields);

        verbose("(driver) loading driver %s\n", name.c_str());

        di->setDynamic(file, doc);

        xmqForeach(doc, NULL, "/driver/detect/mvt", (XMQNodeCallback)add_detect, di);

        check_detection_triplets(di, file);

        di->setConstructor([](MeterInfo& mi, DriverInfo& di){ return shared_ptr<Meter>(new DriverDynamic(mi, di)); });

        return true;
    }
    catch (...)
    {
        return false;
    }
}

DriverDynamic::DriverDynamic(MeterInfo &mi, DriverInfo &di) :
    MeterCommonImplementation(mi, di), file_name_(di.getDynamicFileName())
{
    XMQDoc *doc = di.getDynamicDriver();

    verbose("(driver) constructing driver %s from file %s\n",
            di.name().str().c_str(),
            fileName().c_str());

    try
    {
        xmqForeach(doc, NULL, "/driver/field", (XMQNodeCallback)add_field, this);
    }
    catch (...)
    {
    }
}

XMQProceed DriverDynamic::add_detect(XMQDoc *doc, XMQNode *detect, DriverInfo *di)
{
    string mvt = xmqGetString(doc, detect, ".");

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
        throw 1;
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
        if (!(a >= 'A' && a < 'Z' &&
              b >= 'A' && b < 'Z' &&
              c >= 'A' && c < 'Z'))
        {
            warning("(driver) error in %s, bad manufacturer in mvt triplet: %s\n"
                    "%s\n"
                    "Use 3 uppercase characters A-Z or 4 lowercase hex chars.\n"
                    "%s\n",
                    di->getDynamicFileName().c_str(),
                    mfct.c_str(),
                    line,
                    line);
            throw 1;
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
            throw 1;
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
        throw 1;
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
        throw 1;
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

XMQProceed DriverDynamic::add_field(XMQDoc *doc, XMQNode *field, DriverDynamic *dd)
{
    // The field name must be supplied without a unit ie total (not total_m3) since units are managed by wmbusmeters.
    string name = check_field_name(xmqGetString(doc, field, "name"), dd);

    // The quantity ie Volume, gives the default unit (m3) for the field. The unit can be overriden with display_unit.
    Quantity quantity = check_field_quantity(xmqGetString(doc, field, "quantity"), dd);

    // Text fields are either version strings or lookups from status bits.
    // All other fields are numeric, ie they have a unit. This also includes date and datetime.
    bool is_numeric = quantity != Quantity::Text;

    // The vif scaling is by default Auto but can be overriden for pesky fields.
    VifScaling vif_scaling = check_vif_scaling(xmqGetString(doc, field, "vif_scaling"), dd);

    // The properties are by default empty but can be specified for specific fields.
    PrintProperties properties = check_print_properties(xmqGetString(doc, field, "attributes"), dd);

    // The about fields explains what the value is for. Ie. is storage 1 the previous day or month value etc.
    string info = get_translation(doc, field, "about", language());

    // The calculate formula is optional.
    string calculate = check_calculate(xmqGetString(doc, field, "calculate"), dd);

    // The display unit is usually based on the quantity. But you can override it.
    Unit display_unit = check_display_unit(xmqGetString(doc, field, "display_unit"), dd);

    // Now find all matchers.
    FieldMatcher match = FieldMatcher::build();
    dd->tmp_matcher_ = &match;
    int num_matches = xmqForeach(doc, field, "match", (XMQNodeCallback)add_match, dd);
    // Check if there were any matches at all, if not, then disable the matcher.
    match.active = num_matches > 0;

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
                match,
                display_unit
                );
        }
        else
        {
            if (match.active)
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
        dd->addStringFieldWithExtractor(
            name,
            info,
            properties,
            match
            );
    }
    return XMQ_CONTINUE;
}

XMQProceed DriverDynamic::add_match(XMQDoc *doc, XMQNode *match, DriverDynamic *dd)
{
    FieldMatcher *fm = dd->tmp_matcher_;

    check_set_measurement_type(xmqGetString(doc, match, "measurement_type"), fm, dd);

    check_set_vif_range(xmqGetString(doc, match, "vif_range"), fm, dd);

    const char *vif_range_s = xmqGetString(doc, match, "vif_range");

    if (vif_range_s)
    {
        VIFRange vif_range = toVIFRange(vif_range_s);
        if (vif_range == VIFRange::None)
        {
            warning("(driver) error unknown measurement type %s\n",  vif_range_s);
            throw 1;
        }
        fm->set(vif_range);
    }

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
        warning("(driver) error in %s, cannot find: driver/field/name\n"
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

Quantity check_field_quantity(const char *quantity_s, DriverDynamic *dd)
{
    if (!quantity_s)
    {
        warning("(driver) error in %s, cannot find: driver/field/quantity\n"
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
                "%s\n"
                "%s\n",
                dd->fileName().c_str(),
                vif_scaling_s,
                line,
                "???",
                line);
        throw 1;
    }

    return vif_scaling;
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
    const char *txt = xmqGetString(doc, node, xpath.c_str());
    if (!txt)
    {
        xpath = name+"/en";
        txt = xmqGetString(doc, node, xpath.c_str());
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

void check_set_measurement_type(const char *measurement_type_s, FieldMatcher *fm, DriverDynamic *dd)
{
    if (!measurement_type_s)
    {
        warning("(driver) error in %s, cannot find: driver/field/match/measurement_type\n"
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

void check_set_vif_range(const char *vif_range_s, FieldMatcher *fm, DriverDynamic *dd)
{
    if (!vif_range_s)
    {
        warning("(driver) error in %s, cannot find: driver/field/match/vif_range\n"
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
