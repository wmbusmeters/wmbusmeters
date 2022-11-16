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

// This is the old style meter list. Drivers are succesively rewritten
// from meter_xyz.cc to driver_xyz.cc only old style drivers are listed here.
// The new driver_xyz.cc file format is selfcontained so eventually this
// macro LIST_OF_METERS will be empty and go away.

#define LIST_OF_METERS \
    X(auto,       0,      AutoMeter, AUTO, Auto) \
    X(unknown,    0,      UnknownMeter, UNKNOWN, Unknown) \
    X(eurisii,    T1_bit, HeatCostAllocationMeter, EURISII, EurisII)   \
    X(evo868,     T1_bit, WaterMeter,       EVO868,      EVO868)       \
    X(fhkvdataiii,T1_bit, HeatCostAllocationMeter,       FHKVDATAIII,   FHKVDataIII)    \
    X(fhkvdataiv, T1_bit, HeatCostAllocationMeter,       FHKVDATAIV,    FHKVDataIV)     \
    X(gransystems,T1_bit, ElectricityMeter, CCx01, CCx01) 		       \
    X(hydrocalm3, T1_bit, HeatMeter,        HYDROCALM3,  HydrocalM3)   \
    X(hydrodigit, T1_bit, WaterMeter,       HYDRODIGIT,  Hydrodigit)   \
    X(mkradio4,   T1_bit, WaterMeter,       MKRADIO4,    MKRadio4)     \
    X(multical302,C1_bit|T1_bit, HeatMeter,        MULTICAL302, Multical302)  \
    X(multical403,C1_bit, HeatMeter,        MULTICAL403, Multical403)  \
    X(multical602,C1_bit, HeatMeter,        MULTICAL602, Multical602)  \
    X(multical803,C1_bit, HeatMeter,        MULTICAL803, Multical803)  \
    X(rfmamb,     T1_bit, TempHygroMeter,   RFMAMB,      RfmAmb)       \
    X(rfmtx1,     T1_bit, WaterMeter,       RFMTX1,      RfmTX1)       \
    X(sontex868,  T1_bit, HeatCostAllocationMeter, SONTEX868, Sontex868) \
    X(lse_08,     S1_bit|C1_bit, HeatCostAllocationMeter, LSE_08, LSE_08) \


enum class MeterDriver {
#define X(mname,linkmode,info,type,cname) type,
LIST_OF_METERS
#undef X
};

struct DriverName
{
    DriverName() {};
    DriverName(string s) : name_(s) {};
    string str() { return name_; }

private:
    string name_;
};

struct MeterMatch
{
    MeterDriver driver;
    int manufacturer;
    int media;
    int version;
};

// Return a list of matching drivers, like: multical21
void detectMeterDrivers(int manufacturer, int media, int version, std::vector<std::string> *drivers);
// When entering the driver, check that the telegram is indeed known to be
// compatible with the driver(type), if not then print a warning.
bool isMeterDriverValid(MeterDriver type, int manufacturer, int media, int version);
// For an unknown telegram, when analyzing check if the media type is reasonable in relation to the driver.
// Ie. do not try to decode a door sensor telegram with a water meter driver.
bool isMeterDriverReasonableForMedia(MeterDriver type, string driver_name, int media);

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
    MeterDriver driver {}; // Requested driver for decoding telegrams from this meter.
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
    vector<Unit> conversions; // Additional units desired in json.
    vector<string> selected_fields; // Usually set to the default fields, but can be override in meter config.

    // If this is a meter that needs to be polled.
    int    poll_interval; // Poll every x seconds.

    MeterInfo()
    {
    }

    string str();
    DriverName driverName();

    MeterInfo(string b, string n, MeterDriver d, string e, vector<string> i, string k, LinkModeSet lms, int baud, vector<string> &s, vector<string> &j, vector<string> &calcfs)
    {
        bus = b;
        name = n;
        driver = d;
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
        driver = MeterDriver::UNKNOWN;
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

    MeterDriver driver_ {}; // Old driver enum, to go away.
    DriverName name_; // auto, unknown, amiplus, lse_07_17, multical21 etc
    vector<DriverName> name_aliases_; // Secondary names that will map to this driver.
    LinkModeSet linkmodes_; // C1, T1, S1 or combinations thereof.
    Translate::Lookup mfct_tpl_status_bits_; // Translate any mfct specific bits in tpl status.
    MeterType type_; // Water, Electricity etc.
    function<shared_ptr<Meter>(MeterInfo&,DriverInfo&di)> constructor_; // Invoke this to create an instance of the driver.
    vector<DriverDetect> detect_;
    vector<string> default_fields_;
    int force_mfct_index_ = -1; // Used for meters not declaring mfct specific data using the dif 0f.

public:
    DriverInfo() {};
    DriverInfo(MeterDriver mt) : driver_(mt) {};
    void setName(std::string n) { name_ = n; }
    void addNameAlias(std::string n) { name_aliases_.push_back(n); }
    void setMeterType(MeterType t) { type_ = t; }
    void setDefaultFields(string f) { default_fields_ = splitString(f, ','); }
    void addLinkMode(LinkMode lm) { linkmodes_.addLinkMode(lm); }
    void forceMfctIndex(int i) { force_mfct_index_ = i; }
    void addMfctTPLStatusBits(Translate::Lookup lookup) { mfct_tpl_status_bits_ = lookup; }
    void setConstructor(function<shared_ptr<Meter>(MeterInfo&,DriverInfo&)> c) { constructor_ = c; }
    void addDetection(uint16_t mfct, uchar type, uchar ver) { detect_.push_back({ mfct, type, ver }); }
    vector<DriverDetect> &detect() { return detect_; }

    MeterDriver driver() { return driver_; }
    DriverName name() { return name_; }
    vector<DriverName>& nameAliases() { return name_aliases_; }
    MeterType type() { return type_; }
    vector<string>& defaultFields() { return default_fields_; }
    LinkModeSet linkModes() { return linkmodes_; }
    Translate::Lookup &mfctTPLStatusBits() { return mfct_tpl_status_bits_; }
    shared_ptr<Meter> construct(MeterInfo& mi) { return constructor_(mi, *this); }
    bool detect(uint16_t mfct, uchar type, uchar version);
    bool isValidMedia(uchar type);
    bool isCloseEnoughMedia(uchar type);
    int forceMfctIndex() { return force_mfct_index_; }
};

bool registerDriver(function<void(DriverInfo&di)> setup);
bool lookupDriverInfo(const string& driver, DriverInfo *di = NULL);
// Return the best driver match for a telegram.
DriverInfo pickMeterDriver(Telegram *t);
// Return true for mbus and S2/C2/T2 drivers.
bool driverNeedsPolling(MeterDriver driver, DriverName& dn);

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
    JSON = 1,  // This field should be printed when using --format=json
    FIELD = 2, // This field should be printed when using --format=field
    IMPORTANT = 4, // The most important field.
    OPTIONAL = 8, // If no data has arrived, then do not include this field in the json output.
    REQUIRED = 16, // If no data has arrived, then print this field anyway with NaN or null.
    DEPRECATED = 32, // This field is about to be removed or changed in a newer driver, which will have a new name.
    STATUS = 64, // This is >the< status field and it should read OK of not error flags are set.
    JOIN_TPL_STATUS = 128, // This text field also includes the tpl status decoding. multiple OK:s collapse to a single OK.
    JOIN_INTO_STATUS = 256, // This text field is injected into the already defined status field. multiple OK:s collapse.
    OFFICIAL = 512 // This field is listed as an official field for the driver.
};

struct PrintProperties
{
    PrintProperties(int x) : props_(x) {}

    bool hasJSON() { return props_ & PrintProperty::JSON; }
    bool hasFIELD() { return props_ & PrintProperty::FIELD; }
    bool hasIMPORTANT() { return props_ & PrintProperty::IMPORTANT; }
    bool hasOPTIONAL() { return props_ & PrintProperty::OPTIONAL; }
    bool hasDEPRECATED() { return props_ & PrintProperty::DEPRECATED; }
    bool hasSTATUS() { return props_ & PrintProperty::STATUS; }
    bool hasJOINTPLSTATUS() { return props_ & PrintProperty::JOIN_TPL_STATUS; }
    bool hasJOININTOSTATUS() { return props_ & PrintProperty::JOIN_INTO_STATUS; }

    private:
    int props_;
};

struct FieldInfo
{
    FieldInfo(int index,
              string vname,
              Quantity xuantity,
              Unit default_unit,
              VifScaling vif_scaling,
              FieldMatcher matcher,
              string help,
              PrintProperties print_properties,
              function<double(Unit)> get_numeric_value_override,
              function<string()> get_string_value_override,
              function<void(Unit,double)> set_numeric_value_override,
              function<void(string)> set_string_value_override,
              Translate::Lookup lookup,
              Formula *formula
        ) :
        index_(index),
        vname_(vname),
        xuantity_(xuantity),
        default_unit_(default_unit),
        vif_scaling_(vif_scaling),
        matcher_(matcher),
        help_(help),
        print_properties_(print_properties),
        get_numeric_value_override_(get_numeric_value_override),
        get_string_value_override_(get_string_value_override),
        set_numeric_value_override_(set_numeric_value_override),
        set_string_value_override_(set_string_value_override),
        lookup_(lookup),
        formula_(formula)
    {}

    int index() { return index_; }
    string vname() { return vname_; }
    Quantity xuantity() { return xuantity_; }
    Unit defaultUnit() { return default_unit_; }
    VifScaling vifScaling() { return vif_scaling_; }
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
    string renderJson(Meter *m, vector<Unit> *additional_conversions);
    string renderJsonText(Meter *m);
    // Render the field name based on the actual field from the telegram.
    // A FieldInfo can be declared to handle any number of storage fields of a certain range.
    // The vname is then a pattern total_at_month_{storagenr-32} that gets translated into
    // total_at_month_2 (for the dventry with storage nr 34.)
    string generateFieldName();
    // Check if the meter object stores a value for this field.
    bool hasValue(Meter *m);

    Translate::Lookup& lookup() { return lookup_; }

    string str();

private:

    int index_; // The field infos for a meter are ordered.
    string vname_; // Value name, like: total current previous target, ie no unit suffix.
    Quantity xuantity_; // Quantity: Energy, Volume
    Unit default_unit_; // Default unit for above quantity: KWH, M3
    VifScaling vif_scaling_;
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
    unique_ptr<Formula> formula_;
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
    // Either the default fields specified in the driver, or override fields in the meter configuration file.
    virtual vector<string> &selectedFields() = 0;
    virtual void setSelectedFields(vector<string> &f) = 0;
    virtual string meterDriver() = 0;
    virtual string name() = 0;
    virtual MeterDriver driver() = 0;
    virtual DriverName  driverName() = 0;

    virtual string datetimeOfUpdateHumanReadable() = 0;
    virtual string datetimeOfUpdateRobot() = 0;
    virtual string unixTimestampOfUpdate() = 0;
    virtual time_t timestampLastUpdate() = 0;
    virtual void setPollInterval(time_t interval) = 0;
    virtual time_t pollInterval() = 0;
    virtual bool usesPolling() = 0;

    virtual void setNumericValue(FieldInfo *fi, Unit u, double v) = 0;
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

    virtual void addConversions(std::vector<Unit> cs) = 0;
    virtual void addExtraCalculatedField(std::string ecf) = 0;
    virtual vector<Unit>& conversions() = 0;
    virtual void addShell(std::string cmdline) = 0;
    virtual vector<string> &shellCmdlines() = 0;
    virtual void poll(shared_ptr<BusManager> bus) = 0;

    virtual FieldInfo *findFieldInfo(string vname, Quantity xuantity) = 0;
    virtual string renderJsonOnlyDefaultUnit(string vname, Quantity xuantity) = 0;

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
    virtual void analyzeEnabled(bool b, OutputFormat f, string force_driver, string key, bool verbose) = 0;
    virtual void analyzeTelegram(AboutTelegram &about, vector<uchar> &input_frame, bool simulated) = 0;

    virtual ~MeterManager() = default;
};

shared_ptr<MeterManager> createMeterManager(bool daemon);

const char *toString(MeterType type);
string toString(MeterDriver driver);
string toString(DriverInfo &driver);
MeterDriver toMeterDriver(const string& driver);
LinkModeSet toMeterLinkModeSet(const string& driver);
LinkModeSet toMeterLinkModeSet(MeterDriver driver);

#define X(mname,linkmode,info,type,cname) shared_ptr<Meter> create##cname(MeterInfo &m);
LIST_OF_METERS
#undef X

struct Configuration;
struct MeterInfo;
shared_ptr<Meter> createMeter(MeterInfo *mi);

#endif
