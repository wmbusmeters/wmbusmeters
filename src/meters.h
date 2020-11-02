/*
 Copyright (C) 2017-2020 Fredrik Öhrström

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

#include<string>
#include<vector>

#define LIST_OF_METERS \
    X(amiplus,    T1_bit, Electricity, AMIPLUS,     Amiplus)      \
    X(apator08,   T1_bit,        Water,       APATOR08,    Apator08)    \
    X(apator162,  C1_bit|T1_bit, Water,       APATOR162,   Apator162)   \
    X(ultrimis,   T1_bit,        Water,       ULTRIMIS, Ultrimis)  \
    X(cma12w,     C1_bit|T1_bit, TempHygro,   CMA12W,      CMa12w)      \
    X(compact5,   T1_bit, Heat,        COMPACT5,    Compact5)     \
    X(ebzwmbe,    T1_bit, Electricity, EBZWMBE, EBZWMBE)   \
    X(eurisii,    T1_bit, HeatCostAllocation, EURISII, EurisII)   \
    X(ehzp,       T1_bit, Electricity, EHZP,        EHZP)         \
    X(esyswm,     T1_bit, Electricity, ESYSWM,      ESYSWM)       \
    X(flowiq3100, C1_bit, Water,       FLOWIQ3100,  FlowIQ3100)   \
    X(em24,       C1_bit, Electricity, EM24,        EM24)         \
    X(fhkvdataiii,   T1_bit, HeatCostAllocation,        FHKVDATAIII,    FHKVDataIII)     \
    X(hydrus,     T1_bit, Water,       HYDRUS,      Hydrus)       \
    X(hydrodigit, T1_bit, Water,       HYDRODIGIT,  Hydrodigit)   \
    X(iperl,      T1_bit, Water,       IPERL,       Iperl)        \
    X(izar,       T1_bit, Water,       IZAR,        Izar)         \
    X(izar3,      T1_bit, Water,       IZAR3,       Izar3)        \
    X(lansensm,   T1_bit, Smoke,       LANSENSM,    LansenSM)     \
    X(lansenth,   T1_bit, TempHygro,   LANSENTH,    LansenTH)     \
    X(lansendw,   T1_bit, DoorWindow,  LANSENDW,    LansenDW)     \
    X(lansenpu,   T1_bit, Pulse,       LANSENPU,    LansenPU)     \
    X(mkradio3,   T1_bit, Water,       MKRADIO3,    MKRadio3)     \
    X(multical21, C1_bit, Water,       MULTICAL21,  Multical21)   \
    X(multical302,C1_bit, Heat,        MULTICAL302, Multical302)  \
    X(multical403,C1_bit, Heat,        MULTICAL403, Multical403)  \
    X(multical603,C1_bit, Heat,        MULTICAL603, Multical603)  \
    X(omnipower,  C1_bit, Electricity, OMNIPOWER,   Omnipower)    \
    X(rfmamb,     T1_bit, TempHygro,   RFMAMB,      RfmAmb)       \
    X(rfmtx1,     T1_bit, Water,       RFMTX1,      RfmTX1)       \
    X(q400,       T1_bit, Water,       Q400,        Q400)  \
    X(qcaloric,   C1_bit, HeatCostAllocation, QCALORIC, QCaloric) \
    X(supercom587,T1_bit, Water,       SUPERCOM587, Supercom587)  \
    X(vario451,   T1_bit, Heat,        VARIO451,    Vario451)     \
    X(waterstarm, C1_bit|T1_bit, Water,WATERSTARM,  WaterstarM)   \
    X(topaseskr, T1_bit, Water,   TOPASESKR, TopasEsKr)  \


// List of numbers that can be used to detect the meter driver
// from a telegram. Currently these values are checked against
// the outermost DLL layer. Thus this cannot handle if a telegram
// is relayed and the meter telegram is wrapped in an outer
// DLL which identifies the relayer. Something to fix in the future
// is someone reports problems using a relay.
//
// The future solution might have RELAY_DETECTION and
// for such telegrams it will strip the outer layer and
// recreate the inner telegram.
//
//    meter driver,       manufacturer,  media,  version
//
#define METER_DETECTION \
    X(AMIPLUS,   MANUFACTURER_APA,  0x02,  0x02) \
    X(AMIPLUS,   MANUFACTURER_DEV,  0x37,  0x02) \
    X(APATOR08,  0x8614/*APT?*/,    0x03,  0x03) \
    X(APATOR162, MANUFACTURER_APA,  0x06,  0x05) \
    X(APATOR162, MANUFACTURER_APA,  0x07,  0x05) \
    X(ULTRIMIS,  MANUFACTURER_APA,  0x16,  0x01) \
    X(CMA12W,    MANUFACTURER_ELV,  0x1b,  0x20) \
    X(COMPACT5,  MANUFACTURER_TCH,  0x04,  0x45) \
    X(COMPACT5,  MANUFACTURER_TCH,  0xc3,  0x45) \
    X(EBZWMBE,   MANUFACTURER_EBZ,  0x37,  0x02) \
    X(EURISII,   MANUFACTURER_INE,  0x08,  0x55) \
    X(EHZP,      MANUFACTURER_EMH,  0x02,  0x02) \
    X(ESYSWM,    MANUFACTURER_ESY,  0x37,  0x30) \
    X(FLOWIQ3100,MANUFACTURER_KAM,  0x16,  0x1d) \
    X(EM24,      MANUFACTURER_KAM,  0x02,  0x33) \
    X(FHKVDATAIII,MANUFACTURER_TCH, 0x80,  0x69) \
    X(HYDRUS,    MANUFACTURER_DME,  0x07,  0x70) \
    X(HYDRUS,    MANUFACTURER_HYD,  0x07,  0x24) \
    X(HYDRUS,    MANUFACTURER_DME,  0x06,  0x70) \
    X(HYDRODIGIT,MANUFACTURER_BMT,  0x07,  0x13) \
    X(IPERL,     MANUFACTURER_SEN,  0x06,  0x68) \
    X(IPERL,     MANUFACTURER_SEN,  0x07,  0x68) \
    X(IPERL,     MANUFACTURER_SEN,  0x07,  0x7c) \
    X(IZAR,      MANUFACTURER_SAP,  0x01,    -1) \
    X(IZAR,      MANUFACTURER_SAP,  0x15,    -1) \
    X(IZAR,      MANUFACTURER_SAP,  0x66,    -1) \
    X(IZAR,      MANUFACTURER_DME,  0x66,    -1) \
    X(IZAR3,     MANUFACTURER_SAP,  0x00,  0x88) \
    X(LANSENSM,  MANUFACTURER_LAS,  0x1a,  0x03) \
    X(LANSENTH,  MANUFACTURER_LAS,  0x1b,  0x07) \
    X(LANSENDW,  MANUFACTURER_LAS,  0x1d,  0x07) \
    X(LANSENPU,  MANUFACTURER_LAS,  0x00,  0x14) \
    X(LANSENPU,  MANUFACTURER_LAS,  0x00,  0x0b) \
    X(MKRADIO3,  MANUFACTURER_TCH, 0x62,  0x74) \
    X(MKRADIO3,  MANUFACTURER_TCH, 0x72,  0x74) \
    X(MULTICAL21, MANUFACTURER_KAM,  0x06,  0x1b) \
    X(MULTICAL21, MANUFACTURER_KAM,  0x16,  0x1b) \
    X(MULTICAL302,MANUFACTURER_KAM, 0x04,  0x30) \
    X(MULTICAL302,MANUFACTURER_KAM, 0x0d,  0x30) \
    X(MULTICAL403,MANUFACTURER_KAM, 0x0a,  0x34) \
    X(MULTICAL403,MANUFACTURER_KAM, 0x0b,  0x34) \
    X(MULTICAL403,MANUFACTURER_KAM, 0x0c,  0x34) \
    X(MULTICAL403,MANUFACTURER_KAM, 0x0d,  0x34) \
    X(MULTICAL603,MANUFACTURER_KAM, 0x04,  0x35) \
    X(OMNIPOWER,  MANUFACTURER_KAM, 0x02,  0x01) \
    X(RFMAMB,     MANUFACTURER_BMT, 0x1b,  0x10) \
    X(RFMTX1,     MANUFACTURER_BMT, 0x07,  0x05) \
    X(Q400,       MANUFACTURER_AXI, 0x07,  0x10) \
    X(QCALORIC,   MANUFACTURER_QDS, 0x08,  0x35) \
    X(SUPERCOM587,MANUFACTURER_SON, 0x06,  0x3c) \
    X(SUPERCOM587,MANUFACTURER_SON, 0x07,  0x3c) \
    X(VARIO451,   MANUFACTURER_TCH, 0x04,  0x27) \
    X(VARIO451,   MANUFACTURER_TCH, 0xc3,  0x27) \
    X(WATERSTARM, MANUFACTURER_DWZ, 0x06,  0x02) \
    X(TOPASESKR,  MANUFACTURER_AMT, 0x06,  0xf1) \
    X(TOPASESKR,  MANUFACTURER_AMT, 0x07,  0xf1) \

enum class MeterType {
#define X(mname,linkmode,info,type,cname) type,
LIST_OF_METERS
#undef X
    UNKNOWN
};

struct MeterMatch
{
    MeterType driver;
    int manufacturer;
    int media;
    int version;
};

// Return a list of matching drivers, like: multical21
void detectMeterDriver(int manufacturer, int media, int version, std::vector<std::string> *drivers);
// When entering the driver, check that the telegram is indeed known to be
// compatible with the driver(type), if not then print a warning.
bool isMeterDriverValid(MeterType type, int manufacturer, int media, int version);

using namespace std;

typedef unsigned char uchar;

struct MeterInfo
{
    string name;
    string type;
    string id;
    string key;
    LinkModeSet link_modes;
    vector<string> shells;
    vector<string> jsons; // Additional static jsons that are added to each message.

    MeterInfo(string n, string t, string i, string k, LinkModeSet lms, vector<string> &s, vector<string> &j)
    {
        name = n;
        type = t;
        id = i;
        key = k;
        shells = s;
        jsons = j;
        link_modes = lms;
    }
};

struct Meter
{
    // This meter listens to these ids.
    virtual vector<string> ids() = 0;
    // This meter can report these fields, like total_m3, temp_c.
    virtual vector<string> fields() = 0;
    virtual string meterName() = 0;
    virtual string name() = 0;
    virtual MeterType type() = 0;
    virtual WMBus *bus() = 0;

    virtual string datetimeOfUpdateHumanReadable() = 0;
    virtual string datetimeOfUpdateRobot() = 0;

    virtual void onUpdate(function<void(Telegram*t,Meter*)> cb) = 0;
    virtual int numUpdates() = 0;

    virtual void printMeter(Telegram *t,
                            string *human_readable,
                            string *fields, char separator,
                            string *json,
                            vector<string> *envs,
                            vector<string> *more_json,
                            vector<string> *selected_fields) = 0;

    // The handleTelegram expects an input_frame where the DLL crcs have been removed.
    bool handleTelegram(vector<uchar> input_frame);
    virtual bool isTelegramForMe(Telegram *t) = 0;
    virtual MeterKeys *meterKeys() = 0;

    // Dynamically access all data received for the meter.
    virtual std::vector<std::string> getRecords() = 0;
    virtual double getRecordAsDouble(std::string record) = 0;
    virtual uint16_t getRecordAsUInt16(std::string record) = 0;

    virtual void addConversions(std::vector<Unit> cs) = 0;
    virtual void addShell(std::string cmdline) = 0;
    virtual vector<string> &shellCmdlines() = 0;

    virtual ~Meter() = default;
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

struct HeatCostMeter : public virtual Meter
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

struct GenericMeter : public virtual Meter {
};

string toMeterName(MeterType mt);
MeterType toMeterType(string& type);
LinkModeSet toMeterLinkModeSet(string& type);
unique_ptr<WaterMeter> createMultical21(WMBus *bus, MeterInfo &m);
unique_ptr<WaterMeter> createFlowIQ3100(WMBus *bus, MeterInfo &m);
unique_ptr<HeatMeter> createMultical302(WMBus *bus, MeterInfo &m);
unique_ptr<HeatMeter> createMultical403(WMBus *bus, MeterInfo &m);
unique_ptr<HeatMeter> createMultical603(WMBus *bus, MeterInfo &m);
unique_ptr<HeatMeter> createVario451(WMBus *bus, MeterInfo &m);
unique_ptr<HeatMeter> createCompact5(WMBus *bus, MeterInfo &m);
unique_ptr<WaterMeter> createWaterstarM(WMBus *bus, MeterInfo &m);
unique_ptr<ElectricityMeter> createOmnipower(WMBus *bus, MeterInfo &m);
unique_ptr<ElectricityMeter> createAmiplus(WMBus *bus, MeterInfo &m);
unique_ptr<ElectricityMeter> createEM24(WMBus *bus, MeterInfo &m);
unique_ptr<WaterMeter> createSupercom587(WMBus *bus, MeterInfo &m);
unique_ptr<WaterMeter> createMKRadio3(WMBus *bus, MeterInfo &m);
unique_ptr<WaterMeter> createApator08(WMBus *bus, MeterInfo &m);
unique_ptr<WaterMeter> createApator162(WMBus *bus, MeterInfo &m);
unique_ptr<WaterMeter> createUltrimis(WMBus *bus, MeterInfo &m);
unique_ptr<WaterMeter> createIperl(WMBus *bus, MeterInfo &m);
unique_ptr<WaterMeter> createHydrus(WMBus *bus, MeterInfo &m);
unique_ptr<WaterMeter> createHydrodigit(WMBus *bus, MeterInfo &m);
unique_ptr<WaterMeter> createIzar(WMBus *bus, MeterInfo &m);
unique_ptr<WaterMeter> createIzar3(WMBus *bus, MeterInfo &m);
unique_ptr<WaterMeter> createQ400(WMBus *bus, MeterInfo &m);
unique_ptr<HeatCostMeter> createQCaloric(WMBus *bus, MeterInfo &m);
unique_ptr<HeatCostMeter> createEurisII(WMBus *bus, MeterInfo &m);
unique_ptr<HeatCostMeter> createFHKVDataIII(WMBus *bus, MeterInfo &m);
unique_ptr<TempHygroMeter> createLansenTH(WMBus *bus, MeterInfo &m);
unique_ptr<SmokeDetector> createLansenSM(WMBus *bus, MeterInfo &m);
unique_ptr<PulseCounter> createLansenPU(WMBus *bus, MeterInfo &m);
unique_ptr<DoorWindowDetector> createLansenDW(WMBus *bus, MeterInfo &m);
unique_ptr<TempHygroMeter> createCMa12w(WMBus *bus, MeterInfo &m);
unique_ptr<TempHygroMeter> createRfmAmb(WMBus *bus, MeterInfo &m);
unique_ptr<WaterMeter> createRfmTX1(WMBus *bus, MeterInfo &m);
unique_ptr<ElectricityMeter> createEHZP(WMBus *bus, MeterInfo &m);
unique_ptr<ElectricityMeter> createESYSWM(WMBus *bus, MeterInfo &m);
unique_ptr<ElectricityMeter> createEBZWMBE(WMBus *bus, MeterInfo &m);
unique_ptr<WaterMeter> createTopasEsKr(WMBus *bus, MeterInfo &m);
GenericMeter *createGeneric(WMBus *bus, MeterInfo &m);

#endif
