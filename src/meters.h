/*
 Copyright (C) 2017-2021 Fredrik Öhrström

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
#include"util.h"
#include"units.h"
#include"translatebits.h"
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
    X(HeatCostAllocationMeter) \
    X(HeatMeter) \
    X(PulseCounter) \
    X(SmokeDetector) \
    X(TempHygroMeter) \
    X(WaterMeter)  \

enum class MeterType {
#define X(name) name,
LIST_OF_METER_TYPES
#undef X
};

#define LIST_OF_METERS \
    X(auto,       0,      AutoMeter, AUTO, Auto) \
    X(unknown,    0,      UnknownMeter, UNKNOWN, Unknown) \
    X(apator162,  C1_bit|T1_bit, WaterMeter,       APATOR162,   Apator162)   \
    X(bfw240radio,T1_bit, HeatCostAllocationMeter, BFW240RADIO, BFW240Radio)   \
    X(cma12w,     C1_bit|T1_bit, TempHygroMeter,   CMA12W,      CMa12w)      \
    X(compact5,   T1_bit, HeatMeter,        COMPACT5,    Compact5)     \
    X(dme_07,     T1_bit, WaterMeter,       DME_07,      DME_07)      \
    X(ebzwmbe,    T1_bit, ElectricityMeter, EBZWMBE, EBZWMBE)          \
    X(eurisii,    T1_bit, HeatCostAllocationMeter, EURISII, EurisII)   \
    X(ehzp,       T1_bit, ElectricityMeter, EHZP,        EHZP)         \
    X(esyswm,     T1_bit, ElectricityMeter, ESYSWM,      ESYSWM)       \
    X(ei6500,     C1_bit, SmokeDetector,    EI6500,      EI6500)       \
    X(elf,        T1_bit, HeatMeter,        ELF,         Elf)          \
    X(em24,       C1_bit, ElectricityMeter, EM24,        EM24)         \
    X(emerlin868, T1_bit, WaterMeter,       EMERLIN868,  EMerlin868)   \
    X(ev200,      T1_bit, WaterMeter,       EV200,       EV200)        \
    X(evo868,     T1_bit, WaterMeter,       EVO868,      EVO868)       \
    X(fhkvdataiii,T1_bit, HeatCostAllocationMeter,       FHKVDATAIII,   FHKVDataIII)    \
    X(fhkvdataiv, T1_bit, HeatCostAllocationMeter,       FHKVDATAIV,    FHKVDataIV)     \
    X(gransystems,T1_bit, ElectricityMeter, CCx01, CCx01) 		       \
    X(hydrus,     T1_bit, WaterMeter,       HYDRUS,      Hydrus)       \
    X(hydrocalm3, T1_bit, HeatMeter,        HYDROCALM3,  HydrocalM3)   \
    X(hydrodigit, T1_bit, WaterMeter,       HYDRODIGIT,  Hydrodigit)   \
    X(izar,       T1_bit, WaterMeter,       IZAR,        Izar)         \
    X(izar3,      T1_bit, WaterMeter,       IZAR3,       Izar3)        \
    X(lansensm,   T1_bit, SmokeDetector,    LANSENSM,    LansenSM)     \
    X(lansenth,   T1_bit, TempHygroMeter,   LANSENTH,    LansenTH)     \
    X(lansenpu,   T1_bit, PulseCounter,     LANSENPU,    LansenPU)     \
    X(lse_07_17,  S1_bit, WaterMeter,       LSE_07_17,   LSE_07_17)    \
    X(mkradio3,   T1_bit, WaterMeter,       MKRADIO3,    MKRadio3)     \
    X(mkradio4,   T1_bit, WaterMeter,       MKRADIO4,    MKRadio4)     \
    X(multical21, C1_bit|T1_bit, WaterMeter,       MULTICAL21,  Multical21)   \
    X(flowiq2200, C1_bit, WaterMeter,       FLOWIQ2200,  FlowIQ2200)   \
    X(flowiq3100, C1_bit, WaterMeter,       FLOWIQ3100,  FlowIQ3100)   \
    X(multical302,C1_bit|T1_bit, HeatMeter,        MULTICAL302, Multical302)  \
    X(multical403,C1_bit, HeatMeter,        MULTICAL403, Multical403)  \
    X(multical602,C1_bit, HeatMeter,        MULTICAL602, Multical602)  \
    X(multical603,C1_bit, HeatMeter,        MULTICAL603, Multical603)  \
    X(multical803,C1_bit, HeatMeter,        MULTICAL803, Multical803)  \
    X(omnipower,  C1_bit, ElectricityMeter, OMNIPOWER,   Omnipower)    \
    X(piigth,     MBUS_bit, TempHygroMeter, PIIGTH,      PiigTH)       \
    X(rfmamb,     T1_bit, TempHygroMeter,   RFMAMB,      RfmAmb)       \
    X(rfmtx1,     T1_bit, WaterMeter,       RFMTX1,      RfmTX1)       \
    X(tsd2,       T1_bit, SmokeDetector,    TSD2,        TSD2)         \
    X(q400,       T1_bit, WaterMeter,       Q400,        Q400)         \
    X(qcaloric,   C1_bit, HeatCostAllocationMeter, QCALORIC, QCaloric) \
    X(qheat,      T1_bit, HeatMeter,        QHEAT,       QHeat)        \
    X(qsmoke,     T1_bit, SmokeDetector,    QSMOKE,      QSmoke)       \
    X(sensostar,  C1_bit|T1_bit, HeatMeter,SENSOSTAR,  Sensostar)      \
    X(sharky774,  T1_bit, HeatMeter,        SHARKY774,   Sharky774)    \
    X(sontex868,  T1_bit, HeatCostAllocationMeter, SONTEX868, Sontex868) \
    X(topaseskr,  T1_bit, WaterMeter,   TOPASESKR, TopasEsKr)          \
    X(ultrimis,   T1_bit, WaterMeter,       ULTRIMIS,    Ultrimis)     \
    X(vario451,   T1_bit, HeatMeter,        VARIO451,    Vario451)     \
    X(waterstarm, C1_bit|T1_bit, WaterMeter,WATERSTARM,  WaterstarM)   \
    X(whe46x,     S1_bit, HeatCostAllocationMeter, WHE46X, Whe46x)     \
    X(whe5x,      S1_bit, HeatCostAllocationMeter, WHE5X, Whe5x)       \
    X(lse_08,     S1_bit|C1_bit, HeatCostAllocationMeter, LSE_08, LSE_08) \
    X(weh_07,     C1_bit, WaterMeter,       WEH_07,       WEH_07)      \
    X(unismart,   T1_bit, GasMeter,       UNISMART, Unismart)  \
    X(munia,      T1_bit, TempHygroMeter, MUNIA, Munia) \

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

// Return true for mbus and C2/T2 meters.
bool needsPolling(MeterDriver driver);

// Return a list of matching drivers, like: multical21
void detectMeterDrivers(int manufacturer, int media, int version, std::vector<std::string> *drivers);
// When entering the driver, check that the telegram is indeed known to be
// compatible with the driver(type), if not then print a warning.
bool isMeterDriverValid(MeterDriver type, int manufacturer, int media, int version);
// For an unknown telegram, when analyzing check if the media type is reasonable in relation to the driver.
// Ie. do not try to decode a door sensor telegram with a water meter driver.
bool isMeterDriverReasonableForMedia(MeterDriver type, string driver_name, int media);

bool isValidKey(string& key, MeterDriver mt);

using namespace std;

typedef unsigned char uchar;

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
    vector<Unit> conversions; // Additional units desired in json.

    // If this is a meter that needs to be polled.
    int    poll_seconds; // Poll every x seconds.
    int    poll_hour_offset; // Instead of
    string poll_time_period; // Poll only during these hours.

    MeterInfo()
    {
    }

    string str();
    DriverName driverName();

    MeterInfo(string b, string n, MeterDriver d, string e, vector<string> i, string k, LinkModeSet lms, int baud, vector<string> &s, vector<string> &j)
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
        link_modes.clear();
        bps = 0;
    }

    bool parse(string name, string driver, string id, string key);
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
    LinkModeSet linkmodes_; // C1, T1, S1 or combinations thereof.
    MeterType type_; // Water, Electricity etc.
    function<shared_ptr<Meter>(MeterInfo&,DriverInfo&di)> constructor_; // Invoke this to create an instance of the driver.
    vector<DriverDetect> detect_;

public:
    DriverInfo() {};
    DriverInfo(MeterDriver mt) : driver_(mt) {};
    void setName(std::string n) { name_ = n; }
    void setMeterType(MeterType t) { type_ = t; }
    void setExpectedELLSecurityMode(ELLSecurityMode dsm);
    void setExpectedTPLSecurityMode(TPLSecurityMode tsm);

    void addLinkMode(LinkMode lm) { linkmodes_.addLinkMode(lm); }
    void setConstructor(function<shared_ptr<Meter>(MeterInfo&,DriverInfo&)> c) { constructor_ = c; }
    void addDetection(uint16_t mfct, uchar type, uchar ver) { detect_.push_back({ mfct, type, ver }); }
    vector<DriverDetect> &detect() { return detect_; }

    MeterDriver driver() { return driver_; }
    DriverName name() { return name_; }
    MeterType type() { return type_; }
    LinkModeSet linkModes() { return linkmodes_; }
    shared_ptr<Meter> construct(MeterInfo& mi) { return constructor_(mi, *this); }
    bool detect(uint16_t mfct, uchar type, uchar version);
    bool isValidMedia(uchar type);
};

bool registerDriver(function<void(DriverInfo&di)> setup);
bool lookupDriverInfo(string& driver, DriverInfo *di);
// Return the best driver match for a telegram.
DriverInfo pickMeterDriver(Telegram *t);

vector<DriverInfo>& allRegisteredDrivers();

////////////////////////////////////////////////////////////////////////////////////////////////////////////

enum class VifScaling
{
    None, // No auto scaling.
    Auto, // Scale to normalized VIF unit (ie kwh, m3, m3h etc)
    NoneSigned, // No auto scaling however assume the value is signed.
    AutoSigned // Scale and assume the value is signed.
};

struct FieldInfo
{
    FieldInfo(string vname,
              Quantity xuantity,
              Unit default_unit,
              DifVifKey dif_vif_key,
              VifScaling vif_scaling,
              MeasurementType measurement_type,
              ValueInformation value_information,
              StorageNr storage_nr,
              TariffNr tariff_nr,
              IndexNr index_nr,
              string help,
              bool field,
              bool json,
              bool important,
              string field_name,
              function<double(Unit)> get_value_double,
              function<string()> get_value_string,
              function<void(Unit,double)> set_value_double,
              function<void(string)> set_value_string,
              function<bool(FieldInfo*, Meter *mi, Telegram *t)> extract_double,
              function<bool(FieldInfo*, Meter *mi, Telegram *t)> extract_string,
              Translate::Lookup lookup
        ) :
        vname_(vname),
        xuantity_(xuantity),
        default_unit_(default_unit),
        dif_vif_key_(dif_vif_key),
        vif_scaling_(vif_scaling),
        measurement_type_(measurement_type),
        value_information_(value_information),
        storage_nr_(storage_nr),
        tariff_nr_(tariff_nr),
        index_nr_(index_nr),
        help_(help),
        field_(field),
        json_(json),
        important_(important),
        field_name_(field_name),
        get_value_double_(get_value_double),
        get_value_string_(get_value_string),
        set_value_double_(set_value_double),
        set_value_string_(set_value_string),
        extract_double_(extract_double),
        extract_string_(extract_string),
        lookup_(lookup)
    {}

    string vname() { return vname_; }
    Quantity xuantity() { return xuantity_; }
    Unit defaultUnit() { return default_unit_; }
    DifVifKey difVifKey() { return dif_vif_key_; }
    VifScaling vifScaling() { return vif_scaling_; }
    MeasurementType measurementType() { return measurement_type_; }
    ValueInformation valueInformation() { return value_information_; }
    StorageNr storageNr() { return storage_nr_; }
    TariffNr tariffNr() { return tariff_nr_; }
    IndexNr indexNr() { return index_nr_; }
    string help() { return help_; }
    bool field() { return field_; }
    bool json() { return json_; }
    bool important() { return important_; }
    string fieldName() { return field_name_; }

    double getValueDouble(Unit u) { if (get_value_double_) return get_value_double_(u); else return -12345678; }
    bool hasGetValueDouble() { return get_value_double_ != NULL; }
    string getValueString() { if (get_value_string_) return get_value_string_(); else return "?"; }
    bool hasGetValueString() { return get_value_string_ != NULL; }

    void setValueDouble(Unit u, double d) { if (set_value_double_) set_value_double_(u, d);  }
    void setValueString(string s) { if (set_value_string_) set_value_string_(s); }

    void performExtraction(Meter *m, Telegram *t);

    string renderJsonOnlyDefaultUnit();
    string renderJson(vector<Unit> *additional_conversions);
    string renderJsonText();

    Translate::Lookup& lookup() { return lookup_; }

private:

    string vname_; // Value name, like: total current previous target
    Quantity xuantity_; // Quantity: Energy, Volume
    Unit default_unit_; // Default unit for above quantity: KWH, M3
    DifVifKey dif_vif_key_; // Hardcoded difvif key, if empty string then search for mt,vi,s,t,i instead.
    VifScaling vif_scaling_;
    MeasurementType measurement_type_;
    ValueInformation value_information_;
    StorageNr storage_nr_;
    TariffNr tariff_nr_;
    IndexNr index_nr_;
    string help_; // Helpful information on this meters use of this value.
    bool field_; // If true, print in hr/fields output.
    bool json_; // If true, print in json and shell env variables.
    bool important_; // If true, then print this for --format=hr and in the summary when listening to all.
    string field_name_; // Field name for default unit.

    function<double(Unit)> get_value_double_; // Callback to fetch the value from the meter.
    function<string()> get_value_string_; // Callback to fetch the value from the meter.
    function<void(Unit,double)> set_value_double_; // Call back to set the value in the c++ object
    function<void(string)> set_value_string_; // Call back to set the value string in the c++ object
    function<bool(FieldInfo*, Meter *mi, Telegram *t)> extract_double_; // Extract field from telegram and insert into meter.
    function<bool(FieldInfo*, Meter *mi, Telegram *t)> extract_string_; // Extract field from telegram and insert into meter.
    Translate::Lookup lookup_;
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
    virtual vector<string> fields() = 0;
    virtual vector<FieldInfo> prints() = 0;
    virtual string meterDriver() = 0;
    virtual string name() = 0;
    virtual MeterDriver driver() = 0;
    virtual DriverName  driverName() = 0;

    virtual string datetimeOfUpdateHumanReadable() = 0;
    virtual string datetimeOfUpdateRobot() = 0;
    virtual string unixTimestampOfUpdate() = 0;

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
    virtual vector<Unit>& conversions() = 0;
    virtual void addShell(std::string cmdline) = 0;
    virtual vector<string> &shellCmdlines() = 0;
    virtual void poll(shared_ptr<BusManager> bus) = 0;

    virtual FieldInfo *findFieldInfo(string vname) = 0;
    virtual string renderJsonOnlyDefaultUnit(string vname) = 0;

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
    virtual void onTelegram(function<void(AboutTelegram&,vector<uchar>)> cb) = 0;
    virtual void whenMeterUpdated(std::function<void(Telegram*t,Meter*)> cb) = 0;
    virtual void pollMeters(shared_ptr<BusManager> bus) = 0;
    virtual void analyzeEnabled(bool b, OutputFormat f) = 0;
    virtual void analyzeTelegram(AboutTelegram &about, vector<uchar> &input_frame, bool simulated) = 0;

    virtual ~MeterManager() = default;
};

shared_ptr<MeterManager> createMeterManager(bool daemon);

const char *toString(MeterType type);
string toString(MeterDriver driver);
string toString(DriverInfo &driver);
MeterDriver toMeterDriver(string& driver);
LinkModeSet toMeterLinkModeSet(string& driver);
LinkModeSet toMeterLinkModeSet(MeterDriver driver);

#define X(mname,linkmode,info,type,cname) shared_ptr<Meter> create##cname(MeterInfo &m);
LIST_OF_METERS
#undef X

struct Configuration;
struct MeterInfo;
shared_ptr<Meter> createMeter(MeterInfo *mi);

#endif
