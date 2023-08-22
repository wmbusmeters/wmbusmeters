/*
 Copyright (C) 2017-2022 Fredrik Öhrström (gpl-3.0-or-later)

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

#ifndef METER_H_
#define METER_H_

#include"dvparser.h"
#include"formula.h"
#include"util.h"
#include"units.h"
#include"translatebits.h"
#include"wmbus.h"

#include<assert.h>
#include<functional>
#include<numeric>
#include<string>
#include<vector>


#define LIST_OF_METER_TYPES \
    X(AutoMeter) \
    X(UnknownMeter) \
    X(DoorWindowDetector) \
    X(ElectricityMeter) \
    X(GasMeter) \
    X(HeatCostAllocationMeter) \
    X(HeatMeter) \
    X(HeatCoolingMeter) \
    X(PulseCounter) \
    X(SmokeDetector) \
    X(TempHygroMeter) \
    X(WaterMeter)  \
    X(PressureSensor)  \

enum class MeterType {
#define X(name) name,
LIST_OF_METER_TYPES
#undef X
};

struct DriverName
{
    DriverName() {};
    DriverName(string s) : name_(s) {};
    const string &str() const { return name_; }
    bool operator==(const DriverName &dn) const { return name_ == dn.name_; }

private:
    string name_;
};

// Return a list of matching drivers, like: multical21
void detectMeterDrivers(int manufacturer, int media, int version, std::vector<std::string> *drivers);
// When entering the driver, check that the telegram is indeed known to be
// compatible with the driver(type), if not then print a warning.
bool isMeterDriverValid(DriverName driver_name, int manufacturer, int media, int version);
// For an unknown telegram, when analyzing check if the media type is reasonable in relation to the driver.
// Ie. do not try to decode a door sensor telegram with a water meter driver.
bool isMeterDriverReasonableForMedia(string driver_name, int media);

struct MeterInfo;
bool isValidKey(const string& key, MeterInfo &mt);

using namespace std;

typedef unsigned char uchar;

struct Address
{
    // Example address: 12345678
    // Or fully qualified: 12345678.M=PII.T=1b.V=01
    // which means manufacturer triplet PII, type/media=0x1b, version=0x01
    string id;
    bool wildcard_used {}; // The id contains a *
    bool mbus_primary {}; // Signals that the id is 0-250
    uint16_t mfct {};
    uchar type {};
    uchar version {};

    bool parse(string &s);
};

struct MeterInfo
{
    string bus;  // The bus used to communicate with this meter. A device like /dev/ttyUSB0 or an alias like BUS1.
                 // A bus can be an mbus or a wmbus dongle.
                 // The bus can be the empty string, which means that it will fallback to the first defined bus.
    string name; // User specified name of this (group of) meters.
    DriverName driver_name; // Will replace MeterDriver.
    string extras; // Extra driver specific settings.
    vector<string> ids; // Match expressions for ids.
    string idsc; // Comma separated ids.
    string key;  // Decryption key.
    LinkModeSet link_modes;
    int bps {};     // For mbus communication you need to know the baud rate.
    vector<string> shells;
    vector<string> extra_constant_fields; // Additional static fields that are added to each message.
    vector<string> extra_calculated_fields; // Additional field calculated using formulas.
    vector<string> selected_fields; // Usually set to the default fields, but can be override in meter config.

    // If this is a meter that needs to be polled.
    int    poll_interval; // Poll every x seconds.

    MeterInfo()
    {
    }

    string str();
    DriverName driverName();

    MeterInfo(string b, string n, string e, vector<string> i, string k, LinkModeSet lms, int baud, vector<string> &s, vector<string> &j, vector<string> &calcfs)
    {
        bus = b;
        name = n;
        extras = e,
        ids = i;
        idsc = toIdsCommaSeparated(ids);
        key = k;
        shells = s;
        extra_constant_fields = j;
        extra_calculated_fields = calcfs;
        link_modes = lms;
        bps = baud;
    }

    void clear()
    {
        bus = "";
        name = "";
        ids.clear();
        idsc = "";
        key = "";
        shells.clear();
        extra_constant_fields.clear();
        extra_calculated_fields.clear();
        link_modes.clear();
        bps = 0;
    }

    bool parse(string name, string driver, string id, string key);
    bool usesPolling();
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Dynamic loading of drivers based on the driver info.
//
////////////////////////////////////////////////////////////////////////////////////////////////////////////

struct DriverDetect
{
    uint16_t mfct;
    uchar    type;
    uchar    version;
};

struct DriverInfo
{
private:

    DriverName name_; // auto, unknown, amiplus, lse_07_17, multical21 etc
    vector<DriverName> name_aliases_; // Secondary names that will map to this driver.
    LinkModeSet linkmodes_; // C1, T1, S1 or combinations thereof.
    Translate::Lookup mfct_tpl_status_bits_; // Translate any mfct specific bits in tpl status.
    MeterType type_; // Water, Electricity etc.
    function<shared_ptr<Meter>(MeterInfo&,DriverInfo&di)> constructor_; // Invoke this to create an instance of the driver.
    vector<DriverDetect> detect_;
    vector<string> default_fields_;
    int force_mfct_index_ = -1; // Used for meters not declaring mfct specific data using the dif 0f.
    bool has_process_content_ = false; // Mark this driver as having mfct specific decoding.

public:
    DriverInfo() {};
    void setName(std::string n) { name_ = n; }
    void addNameAlias(std::string n) { name_aliases_.push_back(n); }
    void setMeterType(MeterType t) { type_ = t; }
    void setDefaultFields(string f) { default_fields_ = splitString(f, ','); }
    void addLinkMode(LinkMode lm) { linkmodes_.addLinkMode(lm); }
    void forceMfctIndex(int i) { force_mfct_index_ = i; }
    void setConstructor(function<shared_ptr<Meter>(MeterInfo&,DriverInfo&)> c) { constructor_ = c; }
    void addDetection(uint16_t mfct, uchar type, uchar ver) { detect_.push_back({ mfct, type, ver }); }
    void usesProcessContent() { has_process_content_ = true; }

    vector<DriverDetect> &detect() { return detect_; }

    DriverName name() { return name_; }
    vector<DriverName>& nameAliases() { return name_aliases_; }
    bool hasDriverName(DriverName dn) {
        if (name_ == dn) return true;
        for (auto &i : name_aliases_) if (i == dn) return true;
        return false;
    }

    MeterType type() { return type_; }
    vector<string>& defaultFields() { return default_fields_; }
    LinkModeSet linkModes() { return linkmodes_; }
    Translate::Lookup &mfctTPLStatusBits() { return mfct_tpl_status_bits_; }
    shared_ptr<Meter> construct(MeterInfo& mi) { return constructor_(mi, *this); }
    bool detect(uint16_t mfct, uchar type, uchar version);
    bool isValidMedia(uchar type);
    bool isCloseEnoughMedia(uchar type);
    int forceMfctIndex() { return force_mfct_index_; }
    bool hasProcessContent() { return has_process_content_; }
};

bool registerDriver(function<void(DriverInfo&di)> setup);
bool lookupDriverInfo(const string& driver, DriverInfo *di = NULL);
// Return the best driver match for a telegram.
DriverInfo pickMeterDriver(Telegram *t);
// Return true for mbus and S2/C2/T2 drivers.
bool driverNeedsPolling(DriverName& dn);

vector<DriverInfo*>& allDrivers();

////////////////////////////////////////////////////////////////////////////////////////////////////////////

enum class VifScaling
{
    None, // No auto scaling.
    Auto, // Scale to normalized VIF unit (ie kwh, m3, m3h etc)
    NoneSigned, // No auto scaling however assume the value is signed.
    AutoSigned // Scale and assume the value is signed.
};

const char* toString(VifScaling s);

enum PrintProperty
{
    REQUIRED = 1, // If no data has arrived, then print this field anyway with NaN or null.
    DEPRECATED = 2, // This field is about to be removed or changed in a newer driver, which will have a new name.
    STATUS = 4, // This is >the< status field and it should read OK of not error flags are set.
    INCLUDE_TPL_STATUS = 8, // This text field also includes the tpl status decoding. multiple OK:s collapse to a single OK.
    INJECT_INTO_STATUS = 16, // This text field is injected into the already defined status field. multiple OK:s collapse.
    HIDE = 32 // This field is only used in calculations, do not print it!
};

struct PrintProperties
{
    PrintProperties(int x) : props_(x) {}

    bool hasREQUIRED() { return props_ & PrintProperty::REQUIRED; }
    bool hasDEPRECATED() { return props_ & PrintProperty::DEPRECATED; }
    bool hasSTATUS() { return props_ & PrintProperty::STATUS; }
    bool hasINCLUDETPLSTATUS() { return props_ & PrintProperty::INCLUDE_TPL_STATUS; }
    bool hasINJECTINTOSTATUS() { return props_ & PrintProperty::INJECT_INTO_STATUS; }
    bool hasHIDE() { return props_ & PrintProperty::HIDE; }

    private:
    int props_;
};

#define DEFAULT_PRINT_PROPERTIES 0

struct FieldInfo
{
    ~FieldInfo();
    FieldInfo(int index,
              string vname,
              Quantity quantity,
              Unit display_unit,
              VifScaling vif_scaling,
              double scale,
              FieldMatcher matcher,
              string help,
              PrintProperties print_properties,
              function<double(Unit)> get_numeric_value_override,
              function<string()> get_string_value_override,
              function<void(Unit,double)> set_numeric_value_override,
              function<void(string)> set_string_value_override,
              Translate::Lookup lookup,
              Formula *formula
        );

    int index() { return index_; }
    string vname() { return vname_; }
    Quantity xuantity() { return xuantity_; }
    Unit displayUnit() { return display_unit_; }
    VifScaling vifScaling() { return vif_scaling_; }
    double scale() { return scale_; }
    FieldMatcher& matcher() { return matcher_; }
    string help() { return help_; }
    PrintProperties printProperties() { return print_properties_; }

    double getNumericValueOverride(Unit u) { if (get_numeric_value_override_) return get_numeric_value_override_(u); else return -12345678; }
    bool hasGetNumericValueOverride() { return get_numeric_value_override_ != NULL; }
    string getStringValueOverride() { if (get_string_value_override_) return get_string_value_override_(); else return "?"; }
    bool hasGetStringValueOverride() { return get_string_value_override_ != NULL; }

    void setNumericValueOverride(Unit u, double v) { if (set_numeric_value_override_) set_numeric_value_override_(u, v); }
    bool hasSetNumericValueOverride() { return set_numeric_value_override_ != NULL; }
    void setStringValueOverride(string v) { if (set_string_value_override_) set_string_value_override_(v); }
    bool hasSetStringValueOverride() { return set_string_value_override_ != NULL; }

    bool extractNumeric(Meter *m, Telegram *t, DVEntry *dve = NULL);
    bool extractString(Meter *m, Telegram *t, DVEntry *dve = NULL);
    bool hasMatcher();
    bool hasFormula();
    bool matches(DVEntry *dve);
    void performExtraction(Meter *m, Telegram *t, DVEntry *dve);

    void performCalculation(Meter *m);

    string renderJsonOnlyDefaultUnit(Meter *m);
    string renderJson(Meter *m, DVEntry *dve);
    string renderJsonText(Meter *m);
    // Render the field name based on the actual field from the telegram.
    // A FieldInfo can be declared to handle any number of storage fields of a certain range.
    // The vname is then a pattern total_at_month_{storage_counter} that gets translated into
    // total_at_month_2 (for the dventry with storage nr 2.)
    string generateFieldNameWithUnit(DVEntry *dve);
    string generateFieldNameNoUnit(DVEntry *dve);
    // Check if the meter object stores a value for this field.
    bool hasValue(Meter *m);

    Translate::Lookup& lookup() { return lookup_; }

    string str();

private:

    int index_; // The field infos for a meter are ordered.
    string vname_; // Value name, like: total current previous target, ie no unit suffix.
    Quantity xuantity_; // Quantity: Energy, Volume
    Unit display_unit_; // Selected display unit for above quantity: KWH, M3
    VifScaling vif_scaling_;
    double scale_; // A hardcoded scale factor. Used only for manufacturer specific values with unknown units for the vifs.
    FieldMatcher matcher_;
    string help_; // Helpful information on this meters use of this value.
    PrintProperties print_properties_;

    // Normally the values are stored inside the meter object using its setNumeric/setString/getNumeric/getString
    // But for special fields we can override this default location with these setters/getters.
    function<double(Unit)> get_numeric_value_override_; // Callback to fetch the value from the meter.
    function<string()> get_string_value_override_; // Callback to fetch the value from the meter.
    function<void(Unit,double)> set_numeric_value_override_; // Call back to set the value in the c++ object
    function<void(string)> set_string_value_override_; // Call back to set the value string in the c++ object

    // Lookup bits to strings.
    Translate::Lookup lookup_;

    // For calculated fields.
    shared_ptr<Formula> formula_;

    // For the generated field name.
    shared_ptr<StringInterpolator> field_name_;

    // If the field name template could not be parsed.
    bool valid_field_name_ {};
};

struct BusManager;

struct Meter
{
    // Meters are instantiated on the fly from a template, when a telegram arrives
    // and no exact meter exists. Index 1 is the first meter created etc.
    virtual int index() = 0;
    virtual void setIndex(int i) = 0;
    // Use this bus to send messages to the meter.
    virtual string bus() = 0;
    // This meter listens to these ids.
    virtual vector<string> &ids() = 0;
    // Comma separated ids.
    virtual string idsc() = 0;
    // This meter can report these fields, like total_m3, temp_c.
    virtual vector<FieldInfo> &fieldInfos() = 0;
    virtual vector<string> &extraConstantFields() = 0;
    // Either the default fields specified in the driver, or override fields in the meter configuration file.
    virtual vector<string> &selectedFields() = 0;
    virtual void setSelectedFields(vector<string> &f) = 0;
    virtual string name() = 0;
    virtual DriverName driverName() = 0;

    virtual string datetimeOfUpdateHumanReadable() = 0;
    virtual string datetimeOfUpdateRobot() = 0;
    virtual string unixTimestampOfUpdate() = 0;
    virtual time_t timestampLastUpdate() = 0;
    virtual void setPollInterval(time_t interval) = 0;
    virtual time_t pollInterval() = 0;
    virtual bool usesPolling() = 0;

    virtual void setNumericValue(string vname, Unit u, double v) = 0;
    virtual void setNumericValue(FieldInfo *fi, DVEntry *dve, Unit u, double v) = 0;
    virtual double getNumericValue(string vname, Unit u) = 0;
    virtual double getNumericValue(FieldInfo *fi, Unit u) = 0;
    virtual void setStringValue(FieldInfo *fi, std::string v) = 0;
    virtual std::string getStringValue(FieldInfo *fi) = 0;
    virtual std::string decodeTPLStatusByte(uchar sts) = 0;

    virtual void onUpdate(std::function<void(Telegram*t,Meter*)> cb) = 0;
    virtual int numUpdates() = 0;

    virtual void printMeter(Telegram *t,
                            string *human_readable,
                            string *fields, char separator,
                            string *json,
                            vector<string> *envs,
                            vector<string> *more_json,
                            vector<string> *selected_fields,
                            bool pretty_print_json) = 0;

    // The handleTelegram expects an input_frame where the DLL crcs have been removed.
    // Returns true of this meter handled this telegram!
    // Sets id_match to true, if there was an id match, even though the telegram could not be properly handled.
    virtual bool handleTelegram(AboutTelegram &about, vector<uchar> input_frame,
                                bool simulated, string *id, bool *id_match, Telegram *out_t = NULL) = 0;
    virtual MeterKeys *meterKeys() = 0;

    virtual void addExtraCalculatedField(std::string ecf) = 0;
    virtual void addShell(std::string cmdline) = 0;
    virtual vector<string> &shellCmdlines() = 0;
    virtual void poll(shared_ptr<BusManager> bus) = 0;

    virtual FieldInfo *findFieldInfo(string vname, Quantity xuantity) = 0;
    virtual string renderJsonOnlyDefaultUnit(string vname, Quantity xuantity) = 0;

    virtual string debugValues() = 0;

    virtual ~Meter() = default;
};

struct MeterManager
{
    virtual void addMeterTemplate(MeterInfo &mi) = 0;
    virtual void addMeter(shared_ptr<Meter> meter) = 0;
    virtual Meter*lastAddedMeter() = 0;
    virtual void removeAllMeters() = 0;
    virtual void forEachMeter(std::function<void(Meter*)> cb) = 0;
    virtual bool handleTelegram(AboutTelegram &about, vector<uchar> data, bool simulated) = 0;
    virtual bool hasAllMetersReceivedATelegram() = 0;
    virtual bool hasMeters() = 0;
    virtual void onTelegram(function<bool(AboutTelegram&,vector<uchar>)> cb) = 0;
    virtual void whenMeterUpdated(std::function<void(Telegram*t,Meter*)> cb) = 0;
    virtual void pollMeters(shared_ptr<BusManager> bus) = 0;
    virtual void analyzeEnabled(bool b, OutputFormat f, string force_driver, string key, bool verbose, int profile) = 0;
    virtual void analyzeTelegram(AboutTelegram &about, vector<uchar> &input_frame, bool simulated) = 0;

    virtual ~MeterManager() = default;
};

shared_ptr<MeterManager> createMeterManager(bool daemon);

const char *toString(MeterType type);
string toString(DriverInfo &driver);
LinkModeSet toMeterLinkModeSet(const string& driver);

struct Configuration;
struct MeterInfo;
shared_ptr<Meter> createMeter(MeterInfo *mi);

#endif
