/*
 Copyright (C) 2017-2026 Fredrik Öhrström (gpl-3.0-or-later)

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
#include"xmq.h"
#include"wmbus.h"

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
    X(HeatCoolingMeter) \
    X(HeatCostAllocationMeter) \
    X(HeatMeter) \
    X(PressureSensor)  \
    X(PulseCounter) \
    X(Repeater) \
    X(SmokeDetector) \
    X(TempHygroMeter) \
    X(WaterMeter)  \

enum class MeterType {
#define X(name) name,
LIST_OF_METER_TYPES
#undef X
};

enum class ReadableString {
    Unknown,
    Normal,
    Reversed
};

struct DriverName
{
    DriverName() {};
    DriverName(std::string s) : name_(s) {};
    const std::string &str() const { return name_; }
    bool operator==(const DriverName &dn) const { return name_ == dn.name_; }

private:
    std::string name_;
};

// Return a list of matching drivers, like: multical21
void detectMeterDrivers(int manufacturer, int media, int version, std::vector<std::string> *drivers);
// For an unknown telegram, when analyzing check if the media type is reasonable in relation to the driver.
// Ie. do not try to decode a door sensor telegram with a water meter driver.
bool isMeterDriverReasonableForMedia(std::string driver_name, int media);

struct MeterInfo;
bool isValidKey(const std::string& key, MeterInfo &mt);


struct MeterInfo
{
    std::string bus;  // The bus used to communicate with this meter. A device like /dev/ttyUSB0 or an alias like BUS1.
                 // A bus can be an mbus or a wmbus dongle.
                 // The bus can be the empty string, which means that it will fallback to the first defined bus.
    std::string name; // User specified name of this (group of) meters.
    DriverName driver_name; // Will replace MeterDriver.
    std::string extras; // Extra driver specific settings.
    std::vector<AddressExpression> address_expressions; // Match expressions for ids.
    IdentityMode identity_mode {}; // How to group telegram content into objects with state. Default is by id.
    std::string key;  // Decryption key.
    LinkModeSet link_modes;
    int bps {};     // For mbus communication you need to know the baud rate.
    std::vector<std::string> shells;
    std::vector<std::string> new_meter_shells;
    std::vector<std::string> extra_constant_fields; // Additional static fields that are added to each message.
    std::vector<std::string> extra_calculated_fields; // Additional field calculated using formulas.
    std::vector<std::string> selected_fields; // Usually set to the default fields, but can be override in meter config.

    // If this is a meter that needs to be polled.
    int    poll_interval; // Poll every x seconds.

    MeterInfo()
    {
    }

    std::string str();
    DriverName driverName();

    MeterInfo(std::string b, std::string n, std::string e, std::vector<AddressExpression> aes, std::string k, LinkModeSet lms, int baud, std::vector<std::string> &s, std::vector<std::string> &ms, std::vector<std::string> &j, std::vector<std::string> &calcfs)
    {
        bus = b;
        name = n;
        extras = e,
        address_expressions = aes;
        key = k;
        shells = s;
        new_meter_shells = ms;
        extra_constant_fields = j;
        extra_calculated_fields = calcfs;
        link_modes = lms;
        bps = baud;
    }

    void clear()
    {
        bus = "";
        name = "";
        address_expressions.clear();
        key = "";
        shells.clear();
        new_meter_shells.clear();
        extra_constant_fields.clear();
        extra_calculated_fields.clear();
        link_modes.clear();
        bps = 0;
    }

    bool parse(std::string name, std::string driver, std::string id, std::string key);
    bool usesPolling();
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Dynamic loading of drivers based on the driver info.
//
////////////////////////////////////////////////////////////////////////////////////////////////////////////

struct MVT
{
    uint16_t mfct;
    uchar version;
    uchar type;
};

struct DriverInfo
{
private:

    DriverName name_; // auto, unknown, amiplus, lse_07_17, multical21 etc
    std::vector<DriverName> name_aliases_; // Secondary names that will map to this driver.
    LinkModeSet linkmodes_; // C1, T1, S1 or combinations thereof.
    Translate::Lookup mfct_tpl_status_bits_; // Translate any mfct specific bits in tpl status.
    MeterType type_; // Water, Electricity etc.
    std::function<std::shared_ptr<Meter>(MeterInfo&,DriverInfo&di)> constructor_; // Invoke this to create an instance of the driver.
    std::vector<MVT> mvts_;
    std::vector<std::string> default_fields_;
    int force_mfct_index_ = -1; // Used for meters not declaring mfct specific data using the dif 0f.
    bool has_process_content_ = false; // Mark this driver as having mfct specific decoding.
    std::string media_type_; // Override the media string derived from dll_type (for non-standard type bytes).
    std::shared_ptr<XMQDoc> dynamic_driver_ {}; // Configuration loaded from driver file.
    std::string dynamic_file_name_; // Name of actual loaded driver file.
    std::string dynamic_source_xmq_ {}; // A copy of the xmq used to create a dynamic driver.

public:
    ~DriverInfo();
    DriverInfo() {};
    void setName(std::string n) { name_ = n; }
    void addNameAlias(std::string n) { name_aliases_.push_back(n); }
    void setMeterType(MeterType t) { type_ = t; }
    void setAliases(std::string f);
    void setDefaultFields(std::string f) { default_fields_ = splitString(f, ','); }
    void addLinkMode(LinkMode lm) { linkmodes_.addLinkMode(lm); }
    void forceMfctIndex(int i) { force_mfct_index_ = i; }
    void setConstructor(std::function<std::shared_ptr<Meter>(MeterInfo&,DriverInfo&)> c) { constructor_ = c; }
    void addMVT(uint16_t mfct, uchar type, uchar ver) { mvts_.push_back({ mfct, ver, type }); }
    void usesProcessContent() { has_process_content_ = true; }
    void setMediaType(std::string m) { media_type_ = m; }
    const std::string &mediaType() { return media_type_; }
    void setDynamic(const std::string &file_name, XMQDoc *driver) {
        dynamic_file_name_ = file_name;
        dynamic_driver_ = std::shared_ptr<XMQDoc>(driver, [](XMQDoc* d) { if (d) xmqFreeDoc(d); } );
    }
    void setDynamicSource(const std::string &content) { dynamic_source_xmq_ = content; }

    XMQDoc *getDynamicDriver() { return dynamic_driver_.get(); }
    const std::string &getDynamicFileName() { return dynamic_file_name_; }
    const std::string &getDynamicSource() { return dynamic_source_xmq_; }

    std::vector<MVT> &mvts() { return mvts_; }

    DriverName name() { return name_; }
    std::vector<DriverName>& nameAliases() { return name_aliases_; }
    bool hasDriverName(DriverName dn) {
        if (name_ == dn) return true;
        for (auto &i : name_aliases_) if (i == dn) return true;
        return false;
    }

    MeterType type() { return type_; }
    std::vector<std::string>& defaultFields() { return default_fields_; }
    LinkModeSet linkModes() { return linkmodes_; }
    Translate::Lookup &mfctTPLStatusBits() { return mfct_tpl_status_bits_; }
    std::shared_ptr<Meter> construct(MeterInfo& mi) { return constructor_(mi, *this); }
    bool detect(uint16_t mfct, uchar version, uchar type);
    bool isValidMedia(uchar type);
    bool isCloseEnoughMedia(uchar type);
    int forceMfctIndex() { return force_mfct_index_; }
    bool hasProcessContent() { return has_process_content_; }
};

bool staticRegisterDriver(std::function<void(DriverInfo&di)> setup);
// Lookup (and load if necessary) driver from memory or disk.
DriverInfo *lookupDriver(const std::string &name);
bool lookupDriverInfo(const std::string& driver, DriverInfo *di = NULL);
// Return the best driver match for a telegram.
DriverInfo pickMeterDriver(Telegram *t);
// Return true for mbus and S2/C2/T2 drivers.
bool driverNeedsPolling(DriverName& dn);

std::string loadDriver(const std::string &file, const char *content);

std::vector<DriverInfo*>& allDrivers();
std::string removedDriverExplanation(const std::string &name);

////////////////////////////////////////////////////////////////////////////////////////////////////////////

enum class VifScaling
{
    Auto, // Scale to normalized VIF unit (ie kwh, m3, m3h etc)
    None, // No auto scaling.
    Unknown
};

const char* toString(VifScaling s);
VifScaling toVifScaling(const char *s);

enum class DifSignedness
{
    Signed,   // By default the binary values are interpreted as signed.
    Unsigned, // We can override for non-compliant meters.
    Unknown
};

const char* toString(DifSignedness s);
DifSignedness toDifSignedness(const char *s);

enum PrintProperty
{
    REQUIRED = 1, // If no data has arrived, then print this field anyway with NaN or null.
    DEPRECATED = 2, // This field is about to be removed or changed in a newer driver, which will have a new name.
    STATUS = 4, // This is >the< status field and it should read OK of not error flags are set.
    INCLUDE_TPL_STATUS = 8, // This text field also includes the tpl status decoding. multiple OK:s collapse to a single OK.
    INJECT_INTO_STATUS = 16, // This text field is injected into the already defined status field. multiple OK:s collapse.
    HIDE = 32, // This field is only used in calculations, do not print it!
    Unknown = 1024
};

const char* toString(PrintProperty p);
PrintProperty toPrintProperty(const char *s);

struct PrintProperties
{
    PrintProperties(int x) : props_(x) {}

    bool hasREQUIRED() { return props_ & PrintProperty::REQUIRED; }
    bool hasDEPRECATED() { return props_ & PrintProperty::DEPRECATED; }
    bool hasSTATUS() { return props_ & PrintProperty::STATUS; }
    bool hasINCLUDETPLSTATUS() { return props_ & PrintProperty::INCLUDE_TPL_STATUS; }
    bool hasINJECTINTOSTATUS() { return props_ & PrintProperty::INJECT_INTO_STATUS; }
    bool hasHIDE() { return props_ & PrintProperty::HIDE; }
    bool hasUnknown() { return props_ & PrintProperty::Unknown; }

private:
    int props_;
};

// Parse a string like DEPRECATED,HIDE into bits.
PrintProperties toPrintProperties(std::string s);

#define DEFAULT_PRINT_PROPERTIES 0

struct FieldInfo
{
    ~FieldInfo();
    FieldInfo(int index,
              std::string vname,
              Quantity quantity,
              Unit display_unit,
              VifScaling vif_scaling,
              DifSignedness dif_signedness,
              double scale,
              FieldMatcher matcher,
              std::string help,
              PrintProperties print_properties,
              std::function<double(Unit)> get_numeric_value_override,
              std::function<std::string()> get_string_value_override,
              std::function<void(Unit,double)> set_numeric_value_override,
              std::function<void(std::string)> set_string_value_override,
              Translate::Lookup lookup,
              Formula *formula,
              Meter *m
        );

    int index() { return index_; }
    std::string vname() { return vname_; }
    Quantity xuantity() { return xuantity_; }
    Unit displayUnit() { return display_unit_; }
    VifScaling vifScaling() { return vif_scaling_; }
    DifSignedness difSignedness() { return dif_signedness_; }
    double scale() { return scale_; }
    FieldMatcher& matcher() { return matcher_; }
    std::string help() { return help_; }
    PrintProperties printProperties() { return print_properties_; }

    bool extractNumeric(Meter *m, Telegram *t, DVEntry *dve = NULL);
    bool extractString(Meter *m, Telegram *t, DVEntry *dve = NULL);
    bool hasMatcher();
    bool hasFormula();
    bool hasIXML();
    bool matches(DVEntry *dve);
    void performExtraction(Meter *m, Telegram *t, DVEntry *dve);
    void performCalculation(Meter *m);

    std::string renderJsonOnlyDefaultUnit(Meter *m);
    std::string renderJson(Meter *m, DVEntry *dve);
    std::string renderJsonText(Meter *m, DVEntry *dve);
    // Render the field name based on the actual field from the telegram.
    // A FieldInfo can be declared to handle any number of storage fields of a certain range.
    // The vname is then a pattern total_at_month_{storage_counter} that gets translated into
    // total_at_month_2 (for the dventry with storage nr 2.)
    std::string generateFieldNameWithUnit(Meter *m, DVEntry *dve);
    std::string generateFieldNameNoUnit(Meter *m, DVEntry *dve);
    // Check if the meter object stores a value for this field.
    bool hasValue(Meter *m);

    Translate::Lookup& lookup() { return lookup_; }

    std::string str();

    void markAsLibrary() { from_library_ = true; index_ = -1; }

    void useIXML(const std::string& ixml);
    XMQDoc *ixmlGrammar() { return ixml_grammar_.get(); }

    void matchEntirePayload(bool b) { match_entire_payload_ = b; }
    bool matchEntirePayload() { return match_entire_payload_; }

    void setReadableString(ReadableString rs) { readable_string_ = rs; }
    ReadableString readableString() { return readable_string_; }

    void setNullValue(double nv) { has_null_value_ = true; null_value_ = nv; }
    bool hasNullValue() { return has_null_value_; }
    double nullValue() { return null_value_; }

private:

    int index_; // The field infos for a meter are ordered.
    std::string vname_; // Value name, like: total current previous target, ie no unit suffix.
    Quantity xuantity_; // Quantity: Energy, Volume
    Unit display_unit_; // Selected display unit for above quantity: KWH, M3
    VifScaling vif_scaling_;
    DifSignedness dif_signedness_;
    double scale_; // A hardcoded scale factor. Used only for manufacturer specific values with unknown units for the vifs.
    FieldMatcher matcher_;
    std::string help_; // Helpful information on this meters use of this value.
    PrintProperties print_properties_;

    // Normally the values are stored inside the meter object using its setNumeric/setString/getNumeric/getString
    // But for special fields we can override this default location with these setters/getters.
    std::function<double(Unit)> get_numeric_value_override_; // Callback to fetch the value from the meter.
    std::function<std::string()> get_string_value_override_; // Callback to fetch the value from the meter.
    std::function<void(Unit,double)> set_numeric_value_override_; // Call back to set the value in the c++ object
    std::function<void(std::string)> set_string_value_override_; // Call back to set the value string in the c++ object

    // Lookup bits to strings.
    Translate::Lookup lookup_;

    // For calculated fields.
    std::shared_ptr<Formula> formula_;

    // For the generated field name.
    std::shared_ptr<StringInterpolator> field_name_;

    // If the field name template could not be parsed.
    bool valid_field_name_ {};

    // If true then this field was fetched from the library.
    bool from_library_ {};

    // If a field has a mfct specific decoder.
    std::shared_ptr<XMQDoc> ixml_grammar_ {};

    // For ixml parsing, nab the entire payload, since it does not conform to the difvif structure.
    bool match_entire_payload_ {};

    // If true, then force readable string extraction.
    ReadableString readable_string_ {};

    // If set, this extracted value is treated as null (no data).
    bool has_null_value_ {};
    double null_value_ {};
};

struct BusManager;
struct MeterManager;

struct Meter
{
    // Meters are instantiated on the fly from a template, when a telegram arrives
    // and no exact meter exists. Index 1 is the first meter created etc.
    virtual int index() = 0;
    virtual void setIndex(int i) = 0;
    // Use this bus to send messages to the meter.
    virtual std::string bus() = 0;
    // This meter listens to these address expressions.
    virtual std::vector<AddressExpression>& addressExpressions() = 0;
    virtual IdentityMode identityMode() = 0;
    // This meter can report these fields, like total_m3, temp_c.
    virtual std::vector<FieldInfo> &fieldInfos() = 0;
    virtual std::vector<std::string> &extraConstantFields() = 0;
    // Either the default fields specified in the driver, or override fields in the meter configuration file.
    virtual std::vector<std::string> &selectedFields() = 0;
    virtual void setSelectedFields(std::vector<std::string> &f) = 0;
    virtual std::string name() = 0;
    virtual DriverName driverName() = 0;
    virtual DriverInfo *driverInfo() = 0;
    virtual bool hasReceivedFirstTelegram() = 0;
    virtual void markFirstTelegramReceived() = 0;

    virtual std::string datetimeOfUpdateHumanReadable() = 0;
    virtual std::string datetimeOfUpdateRobot() = 0;
    virtual std::string unixTimestampOfUpdate() = 0;
    virtual time_t timestampLastUpdate() = 0;
    virtual void setPollInterval(time_t interval) = 0;
    virtual time_t pollInterval() = 0;
    virtual bool usesPolling() = 0;

    virtual void setNumericValue(std::string vname, Unit u, double v) = 0;
    virtual void setNumericValue(FieldInfo *fi, DVEntry *dve, Unit u, double v) = 0;
    virtual double getNumericValue(std::string vname, Unit u) = 0;
    virtual double getNumericValue(FieldInfo *fi, Unit u) = 0;
    virtual void setStringValue(FieldInfo *fi, std::string v, DVEntry *dve) = 0;
    virtual void setStringValue(std::string vname, std::string v, DVEntry *dve = NULL) = 0;
    virtual std::string getStringValue(FieldInfo *fi) = 0;
    virtual std::string decodeTPLStatusByte(uchar sts) = 0;

    virtual void onUpdate(std::function<void(Telegram*t,Meter*)> cb) = 0;
    virtual int numUpdates() = 0;

    virtual void createMeterEnv(std::string id,
                                std::vector<std::string> *envs,
                                std::vector<std::string> *more_json) = 0;
    virtual void printMeter(Telegram *t,
                            std::string *human_readable,
                            std::string *fields, char separator,
                            std::string *json,
                            std::vector<std::string> *envs,
                            std::vector<std::string> *more_json,
                            std::vector<std::string> *selected_fields,
                            bool pretty_print_json) = 0;

    // The handleTelegram expects an input_frame where the DLL crcs have been removed.
    // Returns true of this meter handled this telegram!
    // Sets id_match to true, if there was an id match, even though the telegram could not be properly handled.
    virtual bool handleTelegram(AboutTelegram &about, std::vector<uchar> input_frame,
                                bool simulated, std::vector<Address> *addresses,
                                bool *id_match, Telegram *out_t = NULL) = 0;
    virtual MeterKeys *meterKeys() = 0;
    virtual void setMeterManager(MeterManager *mm) = 0;
    virtual MeterManager *meterManager() = 0;

    virtual void addExtraCalculatedField(std::string ecf) = 0;
    virtual void addShellMeterAdded(std::string cmdline) = 0;
    virtual void addShellMeterUpdated(std::string cmdline) = 0;
    virtual std::vector<std::string> &shellCmdlinesMeterAdded() = 0;
    virtual std::vector<std::string> &shellCmdlinesMeterUpdated() = 0;
    virtual void poll(std::shared_ptr<BusManager> bus) = 0;

    virtual FieldInfo *findFieldInfo(std::string vname, Quantity xuantity) = 0;
    virtual std::string renderJsonOnlyDefaultUnit(std::string vname, Quantity xuantity) = 0;

    virtual std::string debugValues() = 0;

    virtual ~Meter() = default;
};

struct MeterManager
{
    virtual void addMeterTemplate(MeterInfo &mi) = 0;
    virtual void addMeter(std::shared_ptr<Meter> meter) = 0;
    virtual Meter*lastAddedMeter() = 0;
    virtual void removeAllMeters() = 0;
    virtual void forEachMeter(std::function<void(Meter*)> cb) = 0;
    virtual bool handleTelegram(AboutTelegram &about, std::vector<uchar> data, bool simulated) = 0;
    virtual bool hasAllMetersReceivedATelegram() = 0;
    virtual bool hasMeters() = 0;
    virtual void onTelegram(std::function<bool(AboutTelegram&, std::vector<uchar>)> cb) = 0;
    virtual void whenMeterUpdated(std::function<void(Telegram*t,Meter*)> cb) = 0;
    virtual void pollMeters(std::shared_ptr<BusManager> bus) = 0;
    virtual void analyzeEnabled(bool b, OutputFormat f, std::string force_driver, std::string key, bool verbose, int profile) = 0;
    virtual void analyzeTelegram(AboutTelegram &about, std::vector<uchar> &input_frame, bool simulated) = 0;

    virtual ~MeterManager() = default;
};

std::shared_ptr<MeterManager> createMeterManager(bool daemon);

const char *toString(MeterType type);
MeterType toMeterType(std::string type);
std::string toString(DriverInfo &driver);
const char *toString(ReadableString rs);
ReadableString toReadableString(std::string rs);

struct Configuration;
struct MeterInfo;
std::shared_ptr<Meter> createMeter(MeterInfo *mi);

const char *availableMeterTypes();

#endif
