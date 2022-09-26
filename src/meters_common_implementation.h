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
    string name();
    MeterDriver driver();
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

    void onUpdate(function<void(Telegram*,Meter*)> cb);
    int numUpdates();

    static bool isTelegramForMeter(Telegram *t, Meter *meter, MeterInfo *mi);
    MeterKeys *meterKeys();

    MeterCommonImplementation(MeterInfo &mi, string driver);
    MeterCommonImplementation(MeterInfo &mi, DriverInfo &di);

    ~MeterCommonImplementation() = default;

    string meterDriver() { return driver_; }

protected:

    void triggerUpdate(Telegram *t);
    void setExpectedELLSecurityMode(ELLSecurityMode dsm);
    void setExpectedTPLSecurityMode(TPLSecurityMode tsm);
    void addConversions(std::vector<Unit> cs);
    std::vector<Unit>& conversions() { return conversions_; }
    void addShell(std::string cmdline);
    void addExtraConstantField(std::string ecf);
    std::vector<std::string> &shellCmdlines();
    std::vector<std::string> &meterExtraConstantFields();
    void setMeterType(MeterType mt);
    void addLinkMode(LinkMode lm);
    void addMfctTPLStatusBits(Translate::Lookup lookup);
    void setDefaultFields(string f);

    // Print with the default unit for this quantity.
    void addPrint(string vname, Quantity vquantity,
                  function<double(Unit)> getValueFunc, string help, PrintProperties pprops);
    // Print with exactly this unit for this quantity.
    void addPrint(string vname, Quantity vquantity, Unit unit,
                  function<double(Unit)> getValueFunc, string help, PrintProperties pprops);
    // Print the dimensionless Text quantity, no unit is needed.
    void addPrint(string vname, Quantity vquantity,
                  function<std::string()> getValueFunc, string help, PrintProperties pprops);

#define SET_FUNC(varname,to_unit) {[=](Unit from_unit, double d){varname = convert(d, from_unit, to_unit);}}
#define GET_FUNC(varname,from_unit) {[=](Unit to_unit){return convert(varname, from_unit, to_unit);}}
#define LOOKUP_FIELD(DVKEY) NoDifVifKey,VifScaling::Auto,MeasurementType::Instantaneous,VIFRange::Any,AnyStorageNr,AnyTariffNr,IndexNr(1)
#define FIND_FIELD(TYPE,INFO) NoDifVifKey,VifScaling::Auto,TYPE,INFO,StorageNr(0),TariffNr(0),IndexNr(1)
#define FIND_FIELD_S(TYPE,INFO,STORAGE) NoDifVifKey,VifScaling::Auto,TYPE,INFO,STORAGE,TariffNr(0),IndexNr(1)
#define FIND_FIELD_ST(TYPE,INFO,STORAGE,TARIFF) NoDifVifKey,VifScaling::Auto,,TYPE,INFO,STORAGE,TARIFF,IndexNr(1)
#define FIND_FIELD_STI(TYPE,INFO,STORAGE,TARIFF,INDEX) NoDifVifKey,VifScaling::Auto,TYPE,INFO,STORAGE,TARIFF,INDEX

#define FIND_SFIELD(TYPE,INFO) NoDifVifKey,TYPE,INFO,StorageNr(0),TariffNr(0),IndexNr(1)
#define FIND_SFIELD_S(TYPE,INFO,STORAGE) NoDifVifKey,TYPE,INFO,STORAGE,TariffNr(0),IndexNr(1)
#define FIND_SFIELD_ST(TYPE,INFO,STORAGE,TARIFF) NoDifVifKey,TYPE,INFO,STORAGE,TARIFF,IndexNr(1)
#define FIND_SFIELD_STI(TYPE,INFO,STORAGE,TARIFF,INDEX) NoDifVifKey,TYPE,INFO,STORAGE,TARIFF,INDEX

    void addNumericFieldWithExtractor(
        string vname,          // Name of value without unit, eg total
        Quantity vquantity,    // Value belongs to this quantity.
        DifVifKey dif_vif_key, // You can hardocde a dif vif header here or use NoDifVifKey
        VifScaling vif_scaling,
        MeasurementType mt,    // If not using a hardcoded key, search for mt,vi,s,t and i instead.
        VIFRange vi,
        StorageNr s,
        TariffNr t,
        IndexNr i,
        PrintProperties print_properties, // Should this be printed by default in fields,json and hr.
        string help,
        function<void(Unit,double)> setValueFunc, // Use the SET macro above.
        function<double(Unit)> getValueFunc); // Use the GET macro above.

    void addNumericFieldWithExtractor(
        string vname,           // Name of value without unit, eg "total" "total_month{storagenr}"
        string help,            // Information about this field.
        PrintProperties print_properties, // Should this be printed by default in fields,json and hr.
        Quantity vquantity,     // Value belongs to this quantity, this quantity determines the default unit.
        VifScaling vif_scaling, // How should any Vif value be scaled.
        FieldMatcher matcher,
        Unit use_unit = Unit::Unknown); // If specified use this unit instead.

    void addNumericField(
        string vname,          // Name of value without unit, eg total
        Quantity vquantity,    // Value belongs to this quantity.
        PrintProperties print_properties, // Should this be printed by default in fields,json and hr.
        string help,
        function<void(Unit,double)> setValueFunc, // Use the SET macro above.
        function<double(Unit)> getValueFunc); // Use the GET macro above.

#define SET_STRING_FUNC(varname) {[=](string s){varname = s;}}
#define GET_STRING_FUNC(varname) {[=](){return varname; }}

    void addStringFieldWithExtractor(
        string vname,          // Name of value without unit, eg total
        Quantity vquantity,    // Value belongs to this quantity.
        DifVifKey dif_vif_key, // You can hardocde a dif vif header here or use NoDifVifKey
        MeasurementType mt,    // If not using a hardcoded key, search for mt,vi,s,t and i instead.
        VIFRange vi,
        StorageNr s,
        TariffNr t,
        IndexNr i,
        PrintProperties print_properties, // Should this be printed by default in fields,json and hr.
        string help,
        function<void(string)> setValueFunc, // Use the SET_STRING macro above.
        function<string()> getValueFunc); // Use the GET_STRING macro above.

    void addStringFieldWithExtractor(
        string vname,
        string help,
        PrintProperties print_properties,
        FieldMatcher matcher);

    void addStringFieldWithExtractorAndLookup(
        string vname,          // Name of value without unit, eg total
        Quantity vquantity,    // Value belongs to this quantity.
        DifVifKey dif_vif_key, // You can hardocde a dif vif header here or use NoDifVifKey
        MeasurementType mt,    // If not using a hardcoded key, search for mt,vi,s,t and i instead.
        VIFRange vi,
        StorageNr s,
        TariffNr t,
        IndexNr i,
        PrintProperties print_properties, // Should this be printed by default in fields,json and hr.
        string help,
        function<void(string)> setValueFunc, // Use the SET_STRING macro above.
        function<string()> getValueFunc, // Use the GET_STRING macro above.
        Translate::Lookup lookup); // Translate the bits/indexes.

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
    // Json fields cannot be modified expect by adding conversions.
    // Json fields include all values except timestamp_ut, timestamp_utc, timestamp_lt
    // since Json is assumed to be decoded by a program and the current timestamp which is the
    // same as timestamp_utc, can always be decoded/recoded into local time or a unix timestamp.

    FieldInfo *findFieldInfo(string vname);
    string renderJsonOnlyDefaultUnit(string vname);

    void processFieldExtractors(Telegram *t);
    virtual void processContent(Telegram *t);

    void setNumericValue(FieldInfo *fi, Unit u, double v);
    double getNumericValue(FieldInfo *fi, Unit u);
    void setStringValue(FieldInfo *fi, std::string v);
    std::string getStringValue(FieldInfo *fi);

    // Check if the meter has received a value for this field.
    bool hasValue(FieldInfo *fi);
    bool hasNumericValue(FieldInfo *fi);
    bool hasStringValue(FieldInfo *fi);

    std::string decodeTPLStatusByte(uchar sts);

    void addOptionalCommonFields();
    void addOptionalFlowRelatedFields();

    vector<string> &selectedFields() { return selected_fields_; }
    void setSelectedFields(vector<string> &f) { selected_fields_ = f; }

private:

    int index_ {};
    MeterType type_ {};
    string driver_ {};
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

protected:
    vector<Unit> conversions_;
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
