/*
 Copyright (C) 2026 Fredrik Öhrström (gpl-3.0-or-later)

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

// This source is generated from drivers/library.xmq
// Run "cd drivers; make library" to regenerate this file.

#include"meters_common_implementation.h"
#include"util.h"

using namespace std;

namespace
{

bool checkIf(set<string> &fields, const char *s)
{
    if (fields.count(s) > 0)
    {
        fields.erase(s);
        return true;
    }

    return false;
}

bool checkFieldsEmpty(set<string> &fields, string driver_name)
{
    if (fields.size() > 0)
    {
        string info;
        for (auto &s : fields) { info += s+" "; }

        warning("(meter) when adding common fields to driver %s, these fields were not found: %s\n",
                driver_name.c_str(),
                info.c_str());
        return false;
    }
    return true;
}

} // end anonymous namespace

bool MeterCommonImplementation::addOptionalLibraryFields(string field_names)
{
    // Old C++ driver passes fields like: "target_date,target_volume_m3"
    // New xmq driver passes single field like: "target_date|Special help for target date."
    // or just: "target_date"

    set<string> fields = splitStringIntoSet(field_names, ',');
    string help;
    vector<string> helps = splitString(field_names, '|');
    if (helps.size() == 2)
    {
        fields.clear();
        fields.insert(helps[0]);
        help = " "+helps[1];
    }
    if (helps.size() > 2)
    {
        error(EXIT_DRIVER_ERROR, "Bad library field, only zero or one pipe | symbol is allowed: %s", field_names.c_str());
    }

    if (checkIf(fields,"target_hca"))
    {
        addNumericFieldWithExtractor(
            "target",
            "The heat cost allocation recorded by this meter at the target date."+help,
            DEFAULT_PRINT_PROPERTIES,
            Quantity::HCA,
            VifScaling::Auto,
            DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::HeatCostAllocation)
            .set(StorageNr(1))            );
        markLastFieldAsLibrary();
    }

    if (checkIf(fields,"consumption_hca"))
    {
        addNumericFieldWithExtractor(
            "consumption",
            "The current heat cost allocation for this meter."+help,
            DEFAULT_PRINT_PROPERTIES,
            Quantity::HCA,
            VifScaling::Auto,
            DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::HeatCostAllocation)            );
        lastAddedField()->setChange(Change::Increasing);
        markLastFieldAsLibrary();
    }

    if (checkIf(fields,"access_counter"))
    {
        addNumericFieldWithExtractor(
            "access",
            "Meter access counter."+help,
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Dimensionless,
            VifScaling::None,
            DifSignedness::Unsigned,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::AccessNumber)            );
        markLastFieldAsLibrary();
    }

    if (checkIf(fields,"volume_flow_m3h"))
    {
        addNumericFieldWithExtractor(
            "volume_flow",
            "Media volume flow."+help,
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Flow,
            VifScaling::Auto,
            DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::VolumeFlow)            );
        markLastFieldAsLibrary();
    }

    if (checkIf(fields,"flow_return_temperature_difference_c"))
    {
        addNumericFieldWithExtractor(
            "flow_return_temperature_difference",
            "The difference between flow and return media temperatures."+help,
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Temperature,
            VifScaling::Auto,
            DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::TemperatureDifference)            );
        markLastFieldAsLibrary();
    }

    if (checkIf(fields,"return_temperature_c"))
    {
        addNumericFieldWithExtractor(
            "return_temperature",
            "Return media temperature."+help,
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Temperature,
            VifScaling::Auto,
            DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::ReturnTemperature)            );
        markLastFieldAsLibrary();
    }

    if (checkIf(fields,"external_temperature_c"))
    {
        addNumericFieldWithExtractor(
            "external_temperature",
            "Temperature outside of meter."+help,
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Temperature,
            VifScaling::Auto,
            DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::ExternalTemperature)            );
        markLastFieldAsLibrary();
    }

    if (checkIf(fields,"flow_temperature_c"))
    {
        addNumericFieldWithExtractor(
            "flow_temperature",
            "Forward media temperature."+help,
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Temperature,
            VifScaling::Auto,
            DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::FlowTemperature)            );
        markLastFieldAsLibrary();
    }

    if (checkIf(fields,"total_backward_m3"))
    {
        addNumericFieldWithExtractor(
            "total_backward",
            "The total {media} volume flowing backward."+help,
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Volume,
            VifScaling::Auto,
            DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::Volume)
            .add(VIFCombinable::BackwardFlow)            );
        lastAddedField()->setChange(Change::Increasing);
        markLastFieldAsLibrary();
    }

    if (checkIf(fields,"total_forward_m3"))
    {
        addNumericFieldWithExtractor(
            "total_forward",
            "The total {media} volume flowing forward."+help,
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Volume,
            VifScaling::Auto,
            DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::Volume)
            .add(VIFCombinable::ForwardFlow)            );
        lastAddedField()->setChange(Change::Increasing);
        markLastFieldAsLibrary();
    }

    if (checkIf(fields,"target_date"))
    {
        addNumericFieldWithExtractor(
            "target",
            "The target date. Usually the end of the previous billing period."+help,
            DEFAULT_PRINT_PROPERTIES,
            Quantity::PointInTime,
            VifScaling::Auto,
            DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::Date)
            .set(StorageNr(1)),
            Unit::DateLT
            );
        markLastFieldAsLibrary();
    }

    if (checkIf(fields,"meter_datetime_at_error"))
    {
        addStringFieldWithExtractor(
            "meter_datetime_at_error",
            "Date and time when the meter was in error."+help,
            DEFAULT_PRINT_PROPERTIES,
            FieldMatcher::build()
            .set(MeasurementType::AtError)
            .set(VIFRange::DateTime)            );
        markLastFieldAsLibrary();
    }

    if (checkIf(fields,"meter_datetime"))
    {
        addStringFieldWithExtractor(
            "meter_datetime",
            "Date and time when the meter sent the telegram."+help,
            DEFAULT_PRINT_PROPERTIES,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::DateTime)            );
        markLastFieldAsLibrary();
    }

    if (checkIf(fields,"meter_date_at_error"))
    {
        addStringFieldWithExtractor(
            "meter_date_at_error",
            "Date when the meter was in error."+help,
            DEFAULT_PRINT_PROPERTIES,
            FieldMatcher::build()
            .set(MeasurementType::AtError)
            .set(VIFRange::Date)            );
        markLastFieldAsLibrary();
    }

    if (checkIf(fields,"meter_date"))
    {
        addStringFieldWithExtractor(
            "meter_date",
            "Date when the meter sent the telegram."+help,
            DEFAULT_PRINT_PROPERTIES,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::Date)            );
        markLastFieldAsLibrary();
    }

    if (checkIf(fields,"on_time_at_error_h"))
    {
        addNumericFieldWithExtractor(
            "on_time_at_error",
            "How long the meter has been in an error state while powered up."+help,
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Time,
            VifScaling::Auto,
            DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::AtError)
            .set(VIFRange::OnTime)            );
        markLastFieldAsLibrary();
    }

    if (checkIf(fields,"on_time_h"))
    {
        addNumericFieldWithExtractor(
            "on_time",
            "How long the meter has been powered up."+help,
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Time,
            VifScaling::Auto,
            DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::OnTime)            );
        markLastFieldAsLibrary();
    }

    if (checkIf(fields,"operating_time_h"))
    {
        addNumericFieldWithExtractor(
            "operating_time",
            "How long the meter has been collecting data."+help,
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Time,
            VifScaling::Auto,
            DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::OperatingTime)            );
        markLastFieldAsLibrary();
    }

    if (checkIf(fields,"location"))
    {
        addStringFieldWithExtractor(
            "location",
            "Meter installed at this customer location."+help,
            DEFAULT_PRINT_PROPERTIES,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::Location)            );
        markLastFieldAsLibrary();
    }

    if (checkIf(fields,"customer"))
    {
        addStringFieldWithExtractor(
            "customer",
            "Customer name."+help,
            DEFAULT_PRINT_PROPERTIES,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::Customer)            );
        markLastFieldAsLibrary();
    }

    if (checkIf(fields,"parameter_set"))
    {
        addStringFieldWithExtractor(
            "parameter_set",
            "Parameter set for this meter."+help,
            DEFAULT_PRINT_PROPERTIES,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::ParameterSet)            );
        markLastFieldAsLibrary();
    }

    if (checkIf(fields,"firmware_version"))
    {
        addStringFieldWithExtractor(
            "firmware_version",
            "Meter firmware version."+help,
            DEFAULT_PRINT_PROPERTIES,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::FirmwareVersion)            );
        markLastFieldAsLibrary();
    }

    if (checkIf(fields,"model_version"))
    {
        addStringFieldWithExtractor(
            "model_version",
            "Meter model version."+help,
            DEFAULT_PRINT_PROPERTIES,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::ModelVersion)            );
        markLastFieldAsLibrary();
    }

    if (checkIf(fields,"manufacturer"))
    {
        addStringFieldWithExtractor(
            "manufacturer",
            "Meter manufacturer."+help,
            DEFAULT_PRINT_PROPERTIES,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::Manufacturer)            );
        markLastFieldAsLibrary();
    }

    if (checkIf(fields,"software_version"))
    {
        addStringFieldWithExtractor(
            "software_version",
            "Software version."+help,
            DEFAULT_PRINT_PROPERTIES,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::SoftwareVersion)            );
        markLastFieldAsLibrary();
    }

    if (checkIf(fields,"enhanced_id"))
    {
        addStringFieldWithExtractor(
            "enhanced_id",
            "Enhanced identification number."+help,
            DEFAULT_PRINT_PROPERTIES,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::EnhancedIdentification)            );
        markLastFieldAsLibrary();
    }

    if (checkIf(fields,"fabrication_no"))
    {
        addStringFieldWithExtractor(
            "fabrication_no",
            "Fabrication number."+help,
            DEFAULT_PRINT_PROPERTIES,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::FabricationNo)            );
        markLastFieldAsLibrary();
    }

    if (checkIf(fields,"battery_y"))
    {
        addNumericFieldWithExtractor(
            "battery",
            "Remaining battery life in years."+help,
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Time,
            VifScaling::Auto,
            DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::RemainingBattery),
            Unit::Year
            );
        markLastFieldAsLibrary();
    }

    if (checkIf(fields,"battery_v"))
    {
        addNumericFieldWithExtractor(
            "battery",
            "Battery voltage."+help,
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Voltage,
            VifScaling::Auto,
            DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::Voltage)            );
        markLastFieldAsLibrary();
    }

    if (checkIf(fields,"actuality_duration_h"))
    {
        addNumericFieldWithExtractor(
            "actuality_duration",
            "Lapsed time between measurement and transmission."+help,
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Time,
            VifScaling::Auto,
            DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::ActualityDuration)            );
        markLastFieldAsLibrary();
    }

    if (checkIf(fields,"actuality_duration_s"))
    {
        addNumericFieldWithExtractor(
            "actuality_duration",
            "Lapsed time between measurement and transmission."+help,
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Time,
            VifScaling::Auto,
            DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::ActualityDuration),
            Unit::Second
            );
        markLastFieldAsLibrary();
    }

    if (checkIf(fields,"status") || checkIf(fields,"status-tpl-only"))
    {
        addStringField(
            "status",
            "Status and error flags."+help,
            STATUS | INCLUDE_TPL_STATUS);
        markLastFieldAsLibrary();
    }

    if (checkIf(fields,"target_kwh"))
    {
        addNumericFieldWithExtractor(
            "target",
            "The {water} energy recorded at the last billing date."+help,
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Energy,
            VifScaling::Auto,
            DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::AnyEnergyVIF)
            .set(StorageNr(1))            );
        markLastFieldAsLibrary();
    }

    if (checkIf(fields,"target_m3"))
    {
        addNumericFieldWithExtractor(
            "target",
            "The {water} volume recorded at the last billing date."+help,
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Volume,
            VifScaling::Auto,
            DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::Volume)
            .set(StorageNr(1))            );
        markLastFieldAsLibrary();
    }

    if (checkIf(fields,"total_kwh"))
    {
        addNumericFieldWithExtractor(
            "total",
            "The total {media} energy recorded."+help,
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Energy,
            VifScaling::Auto,
            DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::AnyEnergyVIF)            );
        lastAddedField()->setChange(Change::Increasing);
        markLastFieldAsLibrary();
    }

    if (checkIf(fields,"total_m3"))
    {
        addNumericFieldWithExtractor(
            "total",
            "The total {media} volume recorded."+help,
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Volume,
            VifScaling::Auto,
            DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::Volume)            );
        lastAddedField()->setChange(Change::Increasing);
        markLastFieldAsLibrary();
    }

    if (!checkFieldsEmpty(fields, name()))
    {
        return false;
    }
    return true;
}
