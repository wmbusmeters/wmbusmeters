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

bool DriverDynamic::load(DriverInfo *di, const string &file)
{
    XMQDoc *doc = xmqNewDoc();

    bool ok = xmqParseFile(doc, file.c_str(), "config");

    if (!ok) {
        error("Error loading wmbusmeters driver file %s\n%s",
              file.c_str(),
              xmqDocError(doc));

        return false;
    }

    const char *name = xmqGetString(doc, NULL, "/driver/name");
    if (!is_lowercase_alnum_text(name)) error("(dynamic) Error in %s invalid driver name \"%s\"\n"
                                              "          The driver name must consist of lower case ascii a-z and digits 0-9.\n",
                                              file.c_str(), name);

    const char *default_fields = xmqGetString(doc, NULL, "/driver/default_fields");

    const char *meter_type_s = xmqGetString(doc, NULL, "/driver/meter_type");
    MeterType meter_type = toMeterType(meter_type_s);

    if (meter_type == MeterType::UnknownMeter) error("(dynamic) Error in %s unknown meter type %s\n"
                                                     "Available meter types are:\n%s\n"
                                                     ,
                                                     file.c_str(), meter_type_s, availableMeterTypes());

    verbose("(dynamic) loading %s %s\n", meter_type_s, name);

    di->setName(name);
    di->setDefaultFields(default_fields);
    di->setMeterType(meter_type);
    di->setDynamic(file, doc);

    xmqForeach(doc, NULL, "/driver/detect/mvt", add_detect, di);

    di->setConstructor([](MeterInfo& mi, DriverInfo& di){ return shared_ptr<Meter>(new DriverDynamic(mi, di)); });

    return true;
}

DriverDynamic::DriverDynamic(MeterInfo &mi, DriverInfo &di) : MeterCommonImplementation(mi, di)
{
    string file = di.getDynamicFileName();
    XMQDoc *doc = di.getDynamicDriver();

    verbose("(dynamic) Constructing driver %s from file %s\n",
            di.name().str().c_str(),
            file.c_str());

    xmqForeach(doc, NULL, "/driver/field", add_field, this);
}

XMQProceed DriverDynamic::add_detect(XMQDoc *doc, XMQNode *detect, void *dd)
{
    DriverInfo *di = (DriverInfo*)dd;

    string mvt = xmqGetString(doc, detect, ".");

    auto fields = splitString(mvt, ',');
    if (fields.size() != 3) error("Cannot load driver %s, wrong number of fields in mvt triple.\n", di->name().str().c_str());

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
              c >= 'A' && c < 'Z')) error("(dynamic) %s bad mfct \"%s\" in mvt detect: should be 3 uppercase characters A-Z\n",
                                          di->name().str().c_str(),
                                          mfct.c_str());
        mfct_code = toMfctCode(a, b, c);
    }
    else
    {
        char *eptr;
        mfct_code = strtol(mfct.c_str(), &eptr, 16);
        if (*eptr) error("(dynmic) Error in %s: mfct must be either version must be in range 0..ff\n", di->name().str().c_str());
    }
    if (version > 255 || version < 0)
    {
        error("(dynmic) Error in %s: version must be in range 0..ff\n", di->name().str().c_str());
    }
    if (type > 255 || type < 0)
    {
        error("(dynmic) Error in %s: type must be in range 0..ff\n", di->name().str().c_str());
    }

    string mfct_flag = manufacturerFlag(mfct_code);
    debug("(dynamic) register detection %s %s %2x %02x\n",
          di->name().str().c_str(),
          mfct_flag.c_str(),
          version,
          type);

    di->addDetection(mfct_code, type, version);
    return XMQ_CONTINUE;
}

XMQProceed DriverDynamic::add_field(XMQDoc *doc, XMQNode *field, void *d)
{
    DriverDynamic *dd = (DriverDynamic*)d;
    const char *driver_name = dd->name().c_str();

    const char *name = xmqGetString(doc, field, "name");
    const char *quantity_s = xmqGetString(doc, field, "quantity");
    const char *type_s = xmqGetString(doc, field, "type");
    const char *info_s = xmqGetString(doc, field, "info");
    const char *attributes_s = xmqGetString(doc, field, "attributes");
    const char *vif_scaling_s = xmqGetString(doc, field, "vif_scaling");

    debug("(dynamic) field %s %s %s %s\n", name, quantity_s, vif_scaling_s, attributes_s);

    FieldType type = toFieldType(type_s);
    if (type == FieldType::Unknown) error("(dynamic) Error in %s unknown field type %s\n",  driver_name, type_s);

    Quantity quantity = toQuantity(quantity_s);
    if (quantity == Quantity::Unknown) error("(dynamic) Error in %s unknown quantity %s\n",  driver_name, quantity);

    VifScaling vif_scaling = toVifScaling(vif_scaling_s);
    if (vif_scaling == VifScaling::Unknown) error("(dynamic) Error in %s unknown vif scaling %s\n",  driver_name, vif_scaling_s);

    PrintProperties properties = toPrintProperties(attributes_s);
    if (properties.hasUnknown()) error("(dynamic) Error in %s unknown attributes %s\n",  driver_name, attributes_s);

    FieldMatcher match = FieldMatcher::build();

    xmqForeach(doc, field, "match", add_match, &match);

    dd->addNumericFieldWithExtractor(
            name,
            info_s,
            properties,
            quantity,
            vif_scaling,
            match
            );

    return XMQ_CONTINUE;
}

XMQProceed DriverDynamic::add_match(XMQDoc *doc, XMQNode *match, void *m)
{
    FieldMatcher *fm = (FieldMatcher*)m;

    const char *measurement_type_s = xmqGetString(doc, match, "measurement_type");
    const char *vif_range_s = xmqGetString(doc, match, "vif_range");

    if (measurement_type_s)
    {
        MeasurementType measurement_type = toMeasurementType(measurement_type_s);
        if (measurement_type == MeasurementType::Unknown)
            error("(dynamic) Error unknown measurement type %s\n",  measurement_type_s);
        fm->set(measurement_type);
    }

    if (vif_range_s)
    {
        VIFRange vif_range = toVIFRange(vif_range_s);
        if (vif_range == VIFRange::None)
            error("(dynamic) Error unknown measurement type %s\n",  vif_range_s);
        fm->set(vif_range);
    }

    return XMQ_CONTINUE;
}
