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

#include"util.h"
#include"units.h"
#include"wmbus.h"

#include<functional>
#include<numeric>
#include<string>
#include<vector>

#define LIST_OF_METERS \
    X(auto,       0,      UnknownMeter, AUTO, Auto) \
    X(unknown,    0,      UnknownMeter, UNKNOWN, Unknown) \
    X(amiplus,    T1_bit, ElectricityMeter, AMIPLUS,     Amiplus)      \
    X(apator08,   T1_bit,        WaterMeter,       APATOR08,    Apator08)    \
    X(apator162,  C1_bit|T1_bit, WaterMeter,       APATOR162,   Apator162)   \
    X(aventieswm, T1_bit,        WaterMeter,       AVENTIESWM,  AventiesWM)   \
    X(aventieshca,T1_bit, HeatCostAllocationMeter, AVENTIESHCA, AventiesHCA)   \
    X(bfw240radio,T1_bit, HeatCostAllocationMeter, BFW240RADIO, BFW240Radio)   \
    X(cma12w,     C1_bit|T1_bit, TempHygroMeter,   CMA12W,      CMa12w)      \
    X(compact5,   T1_bit, HeatMeter,        COMPACT5,    Compact5)     \
    X(dme_07,     T1_bit, WaterMeter,       DME_07,      DME_07)      \
    X(ebzwmbe,    T1_bit, ElectricityMeter, EBZWMBE, EBZWMBE)          \
    X(eurisii,    T1_bit, HeatCostAllocationMeter, EURISII, EurisII)   \
    X(ehzp,       T1_bit, ElectricityMeter, EHZP,        EHZP)         \
    X(esyswm,     T1_bit, ElectricityMeter, ESYSWM,      ESYSWM)       \
    X(flowiq2200, C1_bit, WaterMeter,       FLOWIQ2200,  FlowIQ2200)   \
    X(flowiq3100, C1_bit, WaterMeter,       FLOWIQ3100,  FlowIQ3100)   \
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
    X(iperl,      T1_bit, WaterMeter,       IPERL,       Iperl)        \
    X(izar,       T1_bit, WaterMeter,       IZAR,        Izar)         \
    X(izar3,      T1_bit, WaterMeter,       IZAR3,       Izar3)        \
    X(lansensm,   T1_bit, SmokeDetector,    LANSENSM,    LansenSM)     \
    X(lansenth,   T1_bit, TempHygroMeter,   LANSENTH,    LansenTH)     \
    X(lansendw,   T1_bit, DoorWindowDetector, LANSENDW,  LansenDW)     \
    X(lansenpu,   T1_bit, PulseCounter,     LANSENPU,    LansenPU)     \
    X(lse_07_17,  S1_bit, WaterMeter,       LSE_07_17,   LSE_07_17)    \
    X(minomess,   C1_bit, WaterMeter,       MINOMESS,    Minomess) \
    X(mkradio3,   T1_bit, WaterMeter,       MKRADIO3,    MKRadio3)     \
    X(mkradio4,   T1_bit, WaterMeter,       MKRADIO4,    MKRadio4)     \
    X(multical21, C1_bit|T1_bit, WaterMeter,       MULTICAL21,  Multical21)   \
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
    X(sharky,     T1_bit, HeatMeter,        SHARKY,      Sharky)       \
    X(sontex868,  T1_bit, HeatCostAllocationMeter, SONTEX868, Sontex868) \
    X(supercom587,T1_bit, WaterMeter,       SUPERCOM587, Supercom587)  \
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
// Return the best driver match for a telegram.
MeterDriver pickMeterDriver(Telegram *t);

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

struct Print
{
    string vname; // Value name, like: total current previous target
    Quantity quantity; // Quantity: Energy, Volume
    Unit default_unit; // Default unit for above quantity: KWH, M3
    function<double(Unit)> getValueDouble; // Callback to fetch the value from the meter.
    function<string()> getValueString; // Callback to fetch the value from the meter.
    string help; // Helpful information on this meters use of this value.
    bool field; // If true, print in hr/fields output.
    bool json; // If true, print in json and shell env variables.
    string field_name; // Field name for default unit.
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
    virtual vector<Print> prints() = 0;
    virtual string meterDriver() = 0;
    virtual string name() = 0;
    virtual MeterDriver driver() = 0;

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
                            vector<string> *selected_fields) = 0;

    // The handleTelegram expects an input_frame where the DLL crcs have been removed.
    // Returns true of this meter handled this telegram!
    // Sets id_match to true, if there was an id match, even though the telegram could not be properly handled.
    virtual bool handleTelegram(AboutTelegram &about, vector<uchar> input_frame, bool simulated, string *id, bool *id_match) = 0;
    virtual MeterKeys *meterKeys() = 0;

    // Dynamically access all data received for the meter.
    virtual std::vector<std::string> getRecords() = 0;
    virtual double getRecordAsDouble(std::string record) = 0;
    virtual uint16_t getRecordAsUInt16(std::string record) = 0;

    virtual void addConversions(std::vector<Unit> cs) = 0;
    virtual void addShell(std::string cmdline) = 0;
    virtual vector<string> &shellCmdlines() = 0;
    virtual void poll(shared_ptr<BusManager> bus) = 0;

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
    virtual void analyzeEnabled(bool b) = 0;
    virtual void analyzeTelegram(AboutTelegram &about, vector<uchar> &input_frame, bool simulated) = 0;

    virtual ~MeterManager() = default;
};

shared_ptr<MeterManager> createMeterManager(bool daemon);

struct UnknownMeter : public virtual Meter
{
};

struct WaterMeter : public virtual Meter
{
    virtual double totalWaterConsumption(Unit u); // m3
    virtual bool  hasTotalWaterConsumption();
    virtual double targetWaterConsumption(Unit u); // m3
    virtual bool  hasTargetWaterConsumption();
    virtual double maxFlow(Unit u); // m3/s
    virtual bool  hasMaxFlow();
    virtual double flowTemperature(Unit u); // °C
    virtual bool hasFlowTemperature();
    virtual double externalTemperature(Unit u); // °C
    virtual bool hasExternalTemperature();

    virtual string statusHumanReadable();
    virtual string status();
    virtual string timeDry();
    virtual string timeReversed();
    virtual string timeLeaking();
    virtual string timeBursting();
};

struct GasMeter : public virtual Meter
{
    virtual double totalGasConsumption(Unit u); // m3
    virtual bool  hasTotalGasConsumption();
    virtual double targetGasConsumption(Unit u); // m3
    virtual bool  hasTargetGasConsumption();
    virtual double maxFlow(Unit u); // m3/s
    virtual bool  hasMaxFlow();
    virtual double flowTemperature(Unit u); // °C
    virtual bool hasFlowTemperature();
    virtual double externalTemperature(Unit u); // °C
    virtual bool hasExternalTemperature();

    virtual string statusHumanReadable();
    virtual string status();
    virtual string timeDry();
    virtual string timeReversed();
    virtual string timeLeaking();
    virtual string timeBursting();
};

struct HeatMeter : public virtual Meter
{
    virtual double totalEnergyConsumption(Unit u); // kwh
    virtual double currentPeriodEnergyConsumption(Unit u); // kwh
    virtual double previousPeriodEnergyConsumption(Unit u); // kwh
    virtual double currentPowerConsumption(Unit u); // kw
    virtual double totalVolume(Unit u); // m3
};

struct ElectricityMeter : public virtual Meter
{
    virtual double totalEnergyConsumption(Unit u); // kwh
    virtual double totalEnergyProduction(Unit u); // kwh

    virtual double totalReactiveEnergyConsumption(Unit u); // kvarh
    virtual double totalReactiveEnergyProduction(Unit u); // kvarh

    virtual double totalApparentEnergyConsumption(Unit u); // kvah
    virtual double totalApparentEnergyProduction(Unit u); // kvah

    virtual double currentPowerConsumption(Unit u); // kw
    virtual double currentPowerProduction(Unit u); // kw
};

struct HeatCostAllocationMeter : public virtual Meter
{
    virtual double currentConsumption(Unit u);
    virtual string setDate();
    virtual double consumptionAtSetDate(Unit u);
};

struct TempMeter : public virtual Meter
{
    virtual double currentTemperature(Unit u) = 0; // °C
    virtual ~TempMeter() = default;
};

struct HygroMeter : public virtual Meter
{
    virtual double currentRelativeHumidity() = 0; // RH
    virtual ~HygroMeter() = default;
};

struct TempHygroMeter : public virtual TempMeter, public virtual HygroMeter
{
    virtual ~TempHygroMeter() = default;
};

struct SmokeDetector : public virtual Meter
{
    virtual bool smokeDetected() = 0;
    virtual ~SmokeDetector() = default;
};

struct DoorWindowDetector : public virtual Meter
{
    virtual bool open() = 0;
    virtual ~DoorWindowDetector() = default;
};

struct PulseCounter : public virtual Meter
{
    virtual double counterA() = 0;
    virtual double counterB() = 0;
    virtual ~PulseCounter() = default;
};

struct Generic : public virtual Meter {
};

string toString(MeterDriver driver);
MeterDriver toMeterDriver(string& driver);
LinkModeSet toMeterLinkModeSet(string& driver);
LinkModeSet toMeterLinkModeSet(MeterDriver driver);

#define X(mname,linkmode,info,type,cname) shared_ptr<info> create##cname(MeterInfo &m);
LIST_OF_METERS
#undef X

Generic *createGeneric(WMBus *bus, MeterInfo &m);

struct Configuration;
struct MeterInfo;
shared_ptr<Meter> createMeter(MeterInfo *mi);

#endif
