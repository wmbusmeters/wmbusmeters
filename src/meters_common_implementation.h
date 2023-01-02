/*
 Copyright (C) 2018-2022 Fredrik Öhrström (gpl-3.0-or-later)

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

#ifndef METERS_COMMON_IMPLEMENTATION_H_
#define METERS_COMMON_IMPLEMENTATION_H_

#include"dvparser.h"
#include"meters.h"
#include"threads.h"
#include"units.h"

#include<map>
#include<set>


// Values in a meter are stored based on vname + Quantity.
// I.e. you can have a total_m3 and a total_kwh even though they share the same "total" vname
// since they have two different quantities (Volume and Energy).
// The field total_l refers to the same field storage in the meter as total_m3.
// If a wacko meter sends different values, one m3 and one l. then you
// have to name the fields using different vnames.
struct NumericField
{
    Unit unit {};
    double value {};
    FieldInfo *field_info {};

    NumericField() {}
    NumericField(Unit u, double v, FieldInfo *f) : unit(u), value(v), field_info(f) {}
};

struct StringField
{
    std::string value;
    FieldInfo *field_info {};

    StringField() {}
    StringField(std::string v, FieldInfo *f) : value(v), field_info(f) {}
};

struct MeterCommonImplementation : public virtual Meter
{
    int index();
    void setIndex(int i);
    string bus();
    vector<string>& ids();
    string idsc();
    vector<FieldInfo> &fieldInfos();
    vector<string> &extraConstantFields();
    string name();
    DriverName driverName();

    ELLSecurityMode expectedELLSecurityMode();
    TPLSecurityMode expectedTPLSecurityMode();

    string datetimeOfUpdateHumanReadable();
    string datetimeOfUpdateRobot();
    string unixTimestampOfUpdate();
    time_t timestampLastUpdate();
    void setPollInterval(time_t interval);
    time_t pollInterval();
    bool usesPolling();
    void addExtraCalculatedField(std::string ef);

    void onUpdate(function<void(Telegram*,Meter*)> cb);
    int numUpdates();

    static bool isTelegramForMeter(Telegram *t, Meter *meter, MeterInfo *mi);
    MeterKeys *meterKeys();

    MeterCommonImplementation(MeterInfo &mi, string driver);
    MeterCommonImplementation(MeterInfo &mi, DriverInfo &di);

    ~MeterCommonImplementation() = default;

protected:

    void triggerUpdate(Telegram *t);
    void setExpectedELLSecurityMode(ELLSecurityMode dsm);
    void setExpectedTPLSecurityMode(TPLSecurityMode tsm);
    void addShell(std::string cmdline);
    void addExtraConstantField(std::string ecf);
    std::vector<std::string> &shellCmdlines();
    std::vector<std::string> &meterExtraConstantFields();
    void setMeterType(MeterType mt);
    void addLinkMode(LinkMode lm);
    void setMfctTPLStatusBits(Translate::Lookup &lookup);


    void addNumericFieldWithExtractor(
        string vname,           // Name of value without unit, eg "total" "total_month{storagenr}"
        string help,            // Information about this field.
        PrintProperties print_properties, // Should this be printed by default in fields,json and hr.
        Quantity vquantity,     // Value belongs to this quantity, this quantity determines the default unit.
        VifScaling vif_scaling, // How should any Vif value be scaled.
        FieldMatcher matcher,
        Unit display_unit = Unit::Unknown); // If specified use this unit for the json field instead instead of the default unit.

    void addNumericFieldWithCalculator(
        string vname,           // Name of value without unit, eg "total" "total_month{storagenr}"
        string help,            // Information about this field.
        PrintProperties print_properties, // Should this be printed by default in fields,json and hr.
        Quantity vquantity,     // Value belongs to this quantity, this quantity determines the default unit.
        string formula,         // The formula can reference the other fields and + them together.
        Unit display_unit = Unit::Unknown); // If specified use this unit for the json field instead instead of the default unit.

    void addNumericFieldWithCalculatorAndMatcher(
        string vname,           // Name of value without unit, eg "total" "total_month{storagenr}"
        string help,            // Information about this field.
        PrintProperties print_properties, // Should this be printed by default in fields,json and hr.
        Quantity vquantity,     // Value belongs to this quantity, this quantity determines the default unit.
        string formula,         // The formula can reference the other fields and + them together.
        FieldMatcher matcher,   // We can generate a calculated field per match.
        Unit display_unit = Unit::Unknown); // If specified use this unit for the json field instead instead of the default unit.

    void addNumericField(
        string vname,          // Name of value without unit, eg total
        Quantity vquantity,    // Value belongs to this quantity.
        PrintProperties print_properties, // Should this be printed by default in fields,json and hr.
        string help,
        Unit display_unit = Unit::Unknown);  // If specified use this unit for the json field instead instead of the default unit.

#define SET_STRING_FUNC(varname) {[=](string s){varname = s;}}
#define GET_STRING_FUNC(varname) {[=](){return varname; }}

    void addStringFieldWithExtractor(
        string vname,
        string help,
        PrintProperties print_properties,
        FieldMatcher matcher);

    void addStringFieldWithExtractorAndLookup(
        string vname,
        string help,
        PrintProperties print_properties,
        FieldMatcher matcher,
        Translate::Lookup lookup); // Translate the bits/indexes.

    // Used only for status field from tpl_status only.
    void addStringField(
        string vname,
        string help,
        PrintProperties print_properties);

    // The default implementation of poll does nothing.
    // Override for mbus meters that need to be queried and likewise for C2/T2 wmbus-meters.
    void poll(shared_ptr<BusManager> bus);
    bool handleTelegram(AboutTelegram &about, vector<uchar> frame,
                        bool simulated, string *id, bool *id_match, Telegram *out_analyzed = NULL);
    void printMeter(Telegram *t,
                    string *human_readable,
                    string *fields, char separator,
                    string *json,
                    vector<string> *envs,
                    vector<string> *more_json, // Add this json "key"="value" strings.
                    vector<string> *selected_fields, // Only print these fields.
                    bool pretty_print); // Insert newlines and indentation.
    // Json fields include all values except timestamp_ut, timestamp_utc, timestamp_lt
    // since Json is assumed to be decoded by a program and the current timestamp which is the
    // same as timestamp_utc, can always be decoded/recoded into local time or a unix timestamp.

    FieldInfo *findFieldInfo(string vname, Quantity xuantity);
    string renderJsonOnlyDefaultUnit(string vname, Quantity xuantity);
    string debugValues();

    void processFieldExtractors(Telegram *t);
    void processFieldCalculators();

    virtual void processContent(Telegram *t);

    void setNumericValue(string vname, Unit u, double v);
    void setNumericValue(FieldInfo *fi, DVEntry *dve, Unit u, double v);
    double getNumericValue(string vname, Unit u);
    double getNumericValue(FieldInfo *fi, Unit u);
    void setStringValue(string vname, std::string v);
    void setStringValue(FieldInfo *fi, std::string v);
    std::string getStringValue(FieldInfo *fi);

    // Check if the meter has received a value for this field.
    bool hasValue(FieldInfo *fi);
    bool hasNumericValue(FieldInfo *fi);
    bool hasStringValue(FieldInfo *fi);

    std::string decodeTPLStatusByte(uchar sts);

    void addOptionalCommonFields(string fields);
    void addOptionalFlowRelatedFields(string fields);
    void addHCARelatedFields(string fields);

    vector<string> &selectedFields() { return selected_fields_; }
    void setSelectedFields(vector<string> &f) { selected_fields_ = f; }

    void forceMfctIndex(int i) { force_mfct_index_  = i; }

private:

    int index_ {};
    MeterType type_ {};
    DriverName driver_name_;
    string bus_ {};
    MeterKeys meter_keys_ {};
    ELLSecurityMode expected_ell_sec_mode_ {};
    TPLSecurityMode expected_tpl_sec_mode_ {};
    string name_;
    vector<string> ids_;
    string idsc_;
    vector<function<void(Telegram*,Meter*)>> on_update_;
    int num_updates_ {};
    time_t datetime_of_update_ {};
    LinkModeSet link_modes_ {};
    vector<string> shell_cmdlines_;
    vector<string> extra_constant_fields_;
    time_t poll_interval_ {};
    Translate::Lookup mfct_tpl_status_bits_ = NoLookup;
    int force_mfct_index_ = -1;

protected:

    vector<FieldInfo> field_infos_;
    vector<string> field_names_;
    // Defaults to a setting specified in the driver. Can be overridden in the meter file.
    // There is also a global selected_fields that can be set on the command line or in the conf file.
    vector<string> selected_fields_;
    // Map difvif key to hex values from telegrams.
    std::map<std::string,std::pair<int,std::string>> hex_values_;
    // Map field name (total_volume) to numeric value.
    std::map<pair<std::string,Quantity>,NumericField> numeric_values_;
    // Map field name (at_date) to string value.
    std::map<std::string,StringField> string_values_;
    // Used to block next poll, until this poll has received a respones.
    Semaphore waiting_for_poll_response_sem_;
};

#endif
