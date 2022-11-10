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

#ifndef DVPARSER_H
#define DVPARSER_H

#include"util.h"
#include"units.h"

#include<map>
#include<set>
#include<stdint.h>
#include<time.h>
#include<functional>
#include<vector>

#define LIST_OF_VIF_RANGES \
    X(Volume,0x10,0x17,Quantity::Volume,Unit::M3) \
    X(OnTime,0x20,0x23, Quantity::Time, Unit::Hour)  \
    X(OperatingTime,0x24,0x27, Quantity::Time, Unit::Hour)  \
    X(VolumeFlow,0x38,0x3F, Quantity::Flow, Unit::M3H) \
    X(FlowTemperature,0x58,0x5B, Quantity::Temperature, Unit::C) \
    X(ReturnTemperature,0x5C,0x5F, Quantity::Temperature, Unit::C) \
    X(TemperatureDifference,0x60,0x63, Quantity::Temperature, Unit::C) \
    X(ExternalTemperature,0x64,0x67, Quantity::Temperature, Unit::C) \
    X(Pressure,0x68,0x6B, Quantity::Pressure, Unit::BAR) \
    X(HeatCostAllocation,0x6E,0x6E, Quantity::HCA, Unit::HCA) \
    X(Date,0x6C,0x6C, Quantity::PointInTime, Unit::DateTimeLT) \
    X(DateTime,0x6D,0x6D, Quantity::PointInTime, Unit::DateTimeLT) \
    X(EnergyMJ,0x08,0x0F, Quantity::Energy, Unit::MJ) \
    X(EnergyWh,0x00,0x07, Quantity::Energy, Unit::KWH) \
    X(PowerW,0x28,0x2f, Quantity::Power, Unit::KW) \
    X(ActualityDuration,0x74,0x77, Quantity::Time, Unit::Hour) \
    X(FabricationNo,0x78,0x78, Quantity::Text, Unit::TXT) \
    X(EnhancedIdentification,0x79,0x79, Quantity::Text, Unit::TXT) \
    X(RelativeHumidity,0x7B1A,0x7B1B, Quantity::RH, Unit::RH) \
    X(AccessNumber,0x7D08,0x7D08, Quantity::Counter, Unit::COUNTER) \
    X(ParameterSet,0x7D0B,0x7D0B, Quantity::Text, Unit::TXT) \
    X(ModelVersion,0x7D0C,0x7D0C, Quantity::Text, Unit::TXT) \
    X(SoftwareVersion,0x7D0F,0x7D0F, Quantity::Text, Unit::TXT) \
    X(Customer,0x7D11,0x7D11, Quantity::Text, Unit::TXT) \
    X(ErrorFlags,0x7D17,0x7D17, Quantity::Text, Unit::TXT) \
    X(DigitalInput,0x7D1B,0x7D1B, Quantity::Text, Unit::TXT) \
    X(DurationOfTariff,0x7D31,0x7D33, Quantity::Time, Unit::Hour) \
    X(Dimensionless,0x7D3A,0x7D3A, Quantity::Counter, Unit::COUNTER) \
    X(Voltage,0x7D40,0x7D4F, Quantity::Voltage, Unit::Volt) \
    X(Current,0x7D50,0x7D5F, Quantity::Current, Unit::Ampere) \
    X(ResetCounter,0x7D60,0x7D60, Quantity::Counter, Unit::COUNTER) \
    X(CumulationCounter,0x7D61,0x7D61, Quantity::Counter, Unit::COUNTER) \
    X(RemainingBattery,0x7D74,0x7D74, Quantity::Time, Unit::Day) \
    X(DurationSinceReadout,0x7DAC,0x7DAC, Quantity::Time, Unit::Hour) \
    X(AnyVolumeVIF,0x00,0x00, Quantity::Volume, Unit::Unknown) \
    X(AnyEnergyVIF,0x00,0x00, Quantity::Energy, Unit::Unknown)  \
    X(AnyPowerVIF,0x00,0x00, Quantity::Power, Unit::Unknown)


enum class VIFRange
{
    None,
    Any,
#define X(name,from,to,quantity,unit) name,
LIST_OF_VIF_RANGES
#undef X
};

const char *toString(VIFRange v);
Unit toDefaultUnit(VIFRange v);
VIFRange toVIFRange(int i);
bool isInsideVIFRange(int i, VIFRange range);

#define LIST_OF_VIF_COMBINABLES \
    X(Reserved,0x00,0x11) \
    X(Average,0x12,0x12) \
    X(InverseCompactProfile,0x13,0x13) \
    X(RelativeDeviation,0x14,0x14) \
    X(RecordErrorCodeMeterToController,0x15,0x1c) \
    X(StandardConformantDataContent,0x1d,0x1d) \
    X(CompactProfileWithRegister,0x1e,0x1e) \
    X(CompactProfile,0x1f,0x1f) \
    X(PerSecond,0x20,0x20) \
    X(PerMinute,0x21,0x21) \
    X(PerHour,0x22,0x22) \
    X(PerDay,0x23,0x23) \
    X(PerWeek,0x24,0x24) \
    X(PerMonth,0x25,0x25) \
    X(PerYear,0x26,0x26) \
    X(PerRevolutionMeasurement,0x27,0x27) \
    X(IncrPerInputPulseChannel0,0x28,0x28) \
    X(IncrPerInputPulseChannel1,0x29,0x29) \
    X(IncrPerOutputPulseChannel0,0x2a,0x2a) \
    X(IncrPerOutputPulseChannel1,0x2b,0x2b) \
    X(PerLitre,0x2c,0x2c) \
    X(PerM3,0x2d,0x2d) \
    X(PerKg,0x2e,0x2e) \
    X(PerKelvin,0x2f,0x2f) \
    X(PerKWh,0x30,0x30) \
    X(PerGJ,0x31,0x31) \
    X(PerKW,0x32,0x32) \
    X(PerKelvinLitreW,0x33,0x33) \
    X(PerVolt,0x34,0x34) \
    X(PerAmpere,0x35,0x35) \
    X(MultipliedByS,0x36,0x36) \
    X(MultipliedBySDivV,0x37,0x37) \
    X(MultipliedBySDivA,0x38,0x38) \
    X(StartDateTimeOfAB,0x39,0x39) \
    X(UncorrectedMeterUnit,0x3a,0x3a) \
    X(ForwardFlow,0x3b,0x3b) \
    X(BackwardFlow,0x3c,0x3c) \
    X(ReservedNonMetric,0x3d,0x3d) \
    X(ValueAtBaseCondC,0x3e,0x3e) \
    X(ObisDeclaration,0x3f,0x3f) \
    X(LowerLimit,0x40,0x40) \
    X(ExceedsLowerLimit,0x41,0x41) \
    X(DateTimeExceedsLowerFirstBegin, 0x42,0x42) \
    X(DateTimeExceedsLowerFirstEnd, 0x43,0x43) \
    X(DateTimeExceedsLowerLastBegin, 0x46,0x46) \
    X(DateTimeExceedsLowerLastEnd, 0x47,0x47) \
    X(UpperLimit,0x48,0x48) \
    X(ExceedsUpperLimit,0x49,0x49) \
    X(DateTimeExceedsUpperFirstBegin, 0x4a,0x4a) \
    X(DateTimeExceedsUpperFirstEnd, 0x4b,0x4b) \
    X(DateTimeExceedsUpperLastBegin, 0x4d,0x4d) \
    X(DateTimeExceedsUpperLastEnd, 0x4e,0x4e) \
    X(DurationExceedsLowerFirst,0x50,0x53) \
    X(DurationExceedsLowerLast,0x54,0x57) \
    X(DurationExceedsUpperFirst,0x58,0x5b) \
    X(DurationExceedsUpperLast,0x5c,0x5f) \
    X(DurationOfDFirst,0x60,0x63) \
    X(DurationOfDLast,0x64,0x67) \
    X(ValueDuringLowerLimitExceeded,0x68,0x68) \
    X(LeakageValues,0x69,0x69) \
    X(OverflowValues,0x6a,0x6a) \
    X(ValueDuringUpperLimitExceeded,0x6c,0x6c) \
    X(DateTimeOfDEFirstBegin,0x6a,0x6a) \
    X(DateTimeOfDEFirstEnd,0x6b,0x6b) \
    X(DateTimeOfDELastBegin,0x6e,0x6e) \
    X(DateTimeOfDELastEnd,0x6f,0x6f) \
    X(MultiplicativeCorrectionFactorForValue,0x70,0x77) \
    X(AdditiveCorrectionConstant,0x78,0x7b) \
    X(CombinableVIFExtension,0x7c,0x7c) \
    X(MultiplicativeCorrectionFactorForValue103,0x7d,0x7d) \
    X(FutureValue,0x7e,0x7e) \
    X(MfctSpecific,0x7f,0x7f) \
    X(AtPhase1,0x7c01,0x7c01) \
    X(AtPhase2,0x7c02,0x7c02) \
    X(AtPhase3,0x7c03,0x7c03) \
    X(AtNeutral,0x7c04,0x7c04) \
    X(BetweenPhaseL1AndL2,0x7c05,0x7c05) \
    X(BetweenPhaseL2AndL3,0x7c06,0x7c06) \
    X(BetweenPhaseL3AndL1,0x7c07,0x7c07) \
    X(AtQuadrantQ1,0x7c08,0x7c08) \
    X(AtQuadrantQ2,0x7c09,0x7c09) \
    X(AtQuadrantQ3,0x7c0a,0x7c0a) \
    X(AtQuadrantQ4,0x7c0b,0x7c0b) \
    X(DeltaBetweenImportAndExport,0x7c0c,0x7c0c) \
    X(AccumulationOfAbsoluteValue,0x7c10,0x7c10) \
    X(DataPresentedWithTypeC,0x7c11,0x7c11) \
    X(DataPresentedWithTypeD,0x7c12,0x7c12) \


enum class VIFCombinable
{
    None,
    Any,
#define X(name,from,to) name,
LIST_OF_VIF_COMBINABLES
#undef X
};

VIFCombinable toVIFCombinable(int i);
const char *toString(VIFCombinable v);

enum class MeasurementType
{
    Any,
    Instantaneous,
    Minimum,
    Maximum,
    AtError
};

const char *toString(MeasurementType mt);

void extractDV(std::string &s, uchar *dif, int *vif, bool *has_difes, bool *has_vifes);

struct DifVifKey
{
    DifVifKey(std::string key) : key_(key) {
        extractDV(key, &dif_, &vif_, &has_difes_, &has_vifes_);
    }
    std::string str() { return key_; }
    bool operator==(DifVifKey &dvk) { return key_ == dvk.key_; }
    uchar dif() { return dif_; }
    int vif() { return vif_; }
    bool hasDifes() { return has_difes_; }
    bool hasVifes() { return has_vifes_; }

private:

    std::string key_;
    uchar dif_;
    int vif_;
    bool has_difes_;
    bool has_vifes_;
};

void extractDV(DifVifKey &s, uchar *dif, int *vif, bool *has_difes, bool *has_vifes);

static DifVifKey NoDifVifKey = DifVifKey("");

struct Vif
{
    Vif(int n) : nr_(n) {}
    int intValue() { return nr_; }
    bool operator==(Vif s) { return nr_ == s.nr_; }

private:
    int nr_;
};

struct StorageNr
{
    StorageNr(int n) : nr_(n) {}
    int intValue() { return nr_; }
    bool operator==(StorageNr s) { return nr_ == s.nr_; }
    bool operator!=(StorageNr s) { return nr_ != s.nr_; }
    bool operator>=(StorageNr s) { return nr_ >= s.nr_; }
    bool operator<=(StorageNr s) { return nr_ <= s.nr_; }

private:
    int nr_;
};

static StorageNr AnyStorageNr = StorageNr(-1);

struct TariffNr
{
    TariffNr(int n) : nr_(n) {}
    int intValue() { return nr_; }
    bool operator==(TariffNr s) { return nr_ == s.nr_; }
    bool operator!=(TariffNr s) { return nr_ != s.nr_; }
    bool operator>=(TariffNr s) { return nr_ >= s.nr_; }
    bool operator<=(TariffNr s) { return nr_ <= s.nr_; }

private:
    int nr_;
};

static TariffNr AnyTariffNr = TariffNr(-1);

struct SubUnitNr
{
    SubUnitNr(int n) : nr_(n) {}
    int intValue() { return nr_; }
    bool operator==(SubUnitNr s) { return nr_ == s.nr_; }
    bool operator>=(SubUnitNr s) { return nr_ >= s.nr_; }
    bool operator<=(SubUnitNr s) { return nr_ <= s.nr_; }

private:
    int nr_;
};

struct IndexNr
{
    IndexNr(int n) : nr_(n) {}
    int intValue() { return nr_; }
    bool operator==(IndexNr s) { return nr_ == s.nr_; }

private:
    int nr_;
};

static IndexNr AnyIndexNr = IndexNr(-1);

struct FieldInfo;

struct DVEntry
{
    int offset {}; // Where in the telegram this dventry was found.
    DifVifKey dif_vif_key;
    MeasurementType measurement_type;
    Vif vif;
    std::set<VIFCombinable> combinable_vifs;
    StorageNr storage_nr;
    TariffNr tariff_nr;
    SubUnitNr subunit_nr;
    std::string value;

    DVEntry(int off,
            DifVifKey dvk,
            MeasurementType mt,
            Vif vi,
            std::set<VIFCombinable> vc,
            StorageNr st,
            TariffNr ta,
            SubUnitNr su,
            std::string &val) :
        offset(off),
        dif_vif_key(dvk),
        measurement_type(mt),
        vif(vi),
        combinable_vifs(vc),
        storage_nr(st),
        tariff_nr(ta),
        subunit_nr(su),
        value(val)
    {
    }

    DVEntry() :
        offset(999999),
        dif_vif_key("????"),
        measurement_type(MeasurementType::Instantaneous),
        vif(0),
        storage_nr(0),
        tariff_nr(0),
        subunit_nr(0),
        value("x")
    {
    }

    bool extractDouble(double *out, bool auto_scale, bool assume_signed);
    bool extractLong(uint64_t *out);
    bool extractDate(struct tm *out);
    bool extractReadableString(std::string *out);
    void addFieldInfo(FieldInfo *fi) { field_infos_.insert(fi); }
    bool hasFieldInfo(FieldInfo *fi) { return field_infos_.count(fi) > 0; }
    std::string str();

private:
    std::set<FieldInfo*> field_infos_; // The field infos selected to decode this entry.
};

struct FieldMatcher
{
    // If not actually used, this remains false.
    bool active = false;

    // Exact difvif hex string matching all other checks are ignored.
    bool match_dif_vif_key = false;
    DifVifKey dif_vif_key { "" };

    // Match the measurement_type.
    bool match_measurement_type = false;
    MeasurementType measurement_type { MeasurementType::Instantaneous };

    // Match the value information range. See dvparser.h
    bool match_vif_range = false;
    VIFRange vif_range { VIFRange::Any };

    // Match any vif combinables.
    std::set<VIFCombinable> vif_combinables;

    // Match the storage nr. If no storage is specified, default to match only 0.
    bool match_storage_nr = true;
    StorageNr storage_nr_from { 0 };
    StorageNr storage_nr_to { 0 };

    // Match the tariff nr. If no tariff is specified, default to match only 0.
    bool match_tariff_nr = true;
    TariffNr tariff_nr_from { 0 };
    TariffNr tariff_nr_to { 0 };

    // Match the subunit. If no subunit is specified, default to match only 0.
    bool match_subunit_nr = true;
    SubUnitNr subunit_nr_from { 0 };
    SubUnitNr subunit_nr_to { 0 };

    // If the telegram has multiple identical difvif entries, use entry with this index nr.
    // First entry has nr 1, which is the default value.
    IndexNr index_nr { 1 };

    FieldMatcher() : active(false) { }
    FieldMatcher(bool act) : active(act) { }
    static FieldMatcher build() { return FieldMatcher(true); }
    FieldMatcher &set(DifVifKey k) {
        dif_vif_key = k;
        match_dif_vif_key = (k.str() != ""); return *this; }
    FieldMatcher &set(MeasurementType mt) {
        measurement_type = mt;
        match_measurement_type = (mt != MeasurementType::Any);
        return *this; }
    FieldMatcher &set(VIFRange v) {
        vif_range = v;
        match_vif_range = (v != VIFRange::Any);
        return *this; }
    FieldMatcher &add(VIFCombinable v) {
        vif_combinables.insert(v);
        return *this; }
    FieldMatcher &set(StorageNr s) {
        storage_nr_from = storage_nr_to = s;
        match_storage_nr = (s != AnyStorageNr);
        return *this; }
    FieldMatcher &set(StorageNr from, StorageNr to) {
        storage_nr_from = from;
        storage_nr_to = to;
        match_storage_nr = true;
        return *this; }
    FieldMatcher &set(TariffNr s) {
        tariff_nr_from = tariff_nr_to = s;
        match_tariff_nr = (s != AnyTariffNr);
        return *this; }
    FieldMatcher &set(TariffNr from, TariffNr to) {
        tariff_nr_from = from;
        tariff_nr_to = to;
        match_tariff_nr = true;
        return *this; }
    FieldMatcher &set(SubUnitNr s) {
        subunit_nr_from = subunit_nr_to = s;
        match_subunit_nr = true;
        return *this; }
    FieldMatcher &set(SubUnitNr from, SubUnitNr to) {
        subunit_nr_from = from;
        subunit_nr_to = to;
        match_subunit_nr = true; return *this; }

    FieldMatcher &set(IndexNr i) { index_nr = i; return *this; }

    bool matches(DVEntry &dv_entry);
    std::string str();
};

bool loadFormatBytesFromSignature(uint16_t format_signature, std::vector<uchar> *format_bytes);

struct Telegram;

bool parseDV(Telegram *t,
             std::vector<uchar> &databytes,
             std::vector<uchar>::iterator data,
             size_t data_len,
             std::map<std::string,std::pair<int,DVEntry>> *dv_entries,
             std::vector<uchar>::iterator *format = NULL,
             size_t format_len = 0,
             uint16_t *format_hash = NULL);

// Instead of using a hardcoded difvif as key in the extractDV... below,
// find an existing difvif entry in the values based on the desired value information type.
// Like: Volume, VolumeFlow, FlowTemperature, ExternalTemperature etc
// in combination with the storagenr. (Later I will add tariff/subunit)
bool findKey(MeasurementType mt, VIFRange vi, StorageNr storagenr, TariffNr tariffnr,
             std::string *key, std::map<std::string,std::pair<int,DVEntry>> *values);
// Some meters have multiple identical DIF/VIF values! Meh, they are not using storage nrs or tariff nrs.
// So here we can pick for example nr 2 of an identical set if DIF/VIF values.
// Nr 1 means the first found value.
bool findKeyWithNr(MeasurementType mt, VIFRange vi, StorageNr storagenr, TariffNr tariffnr, int indexnr,
                   std::string *key, std::map<std::string,std::pair<int,DVEntry>> *values);

bool hasKey(std::map<std::string,std::pair<int,DVEntry>> *values, std::string key);

bool extractDVuint8(std::map<std::string,std::pair<int,DVEntry>> *values,
                    std::string key,
                    int *offset,
                    uchar *value);

bool extractDVuint16(std::map<std::string,std::pair<int,DVEntry>> *values,
                     std::string key,
                     int *offset,
                     uint16_t *value);

bool extractDVuint24(std::map<std::string,std::pair<int,DVEntry>> *values,
                     std::string key,
                     int *offset,
                     uint32_t *value);

bool extractDVuint32(std::map<std::string,std::pair<int,DVEntry>> *values,
                     std::string key,
                     int *offset,
                     uint32_t *value);

// All values are scaled according to the vif and wmbusmeters scaling defaults.
bool extractDVdouble(std::map<std::string,std::pair<int,DVEntry>> *values,
                     std::string key,
                     int *offset,
                     double *value,
                     bool auto_scale = true,
                     bool assume_signed = false);

// Extract a value without scaling. Works for 8bits to 64 bits, binary and bcd.
bool extractDVlong(std::map<std::string,std::pair<int,DVEntry>> *values,
                   std::string key,
                   int *offset,
                   uint64_t *value);

// Just copy the raw hex data into the string, not reversed or anything.
bool extractDVHexString(std::map<std::string,std::pair<int,DVEntry>> *values,
                        std::string key,
                        int *offset,
                        std::string *value);

// Read the content and attempt to reverse and transform it into a readble string
// based on the dif information.
bool extractDVReadableString(std::map<std::string,std::pair<int,DVEntry>> *values,
                             std::string key,
                             int *offset,
                             std::string *value);

bool extractDVdate(std::map<std::string,std::pair<int,DVEntry>> *values,
                   std::string key,
                   int *offset,
                   struct tm *value);


#endif
