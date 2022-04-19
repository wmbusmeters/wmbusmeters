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
#include<stdint.h>
#include<time.h>
#include<functional>
#include<vector>

#define LIST_OF_VIF_RANGES \
    X(Volume,0x10,0x17,Quantity::Volume,Unit::M3) \
    X(OperatingTime,0x24,0x27, Quantity::Time, Unit::Second)  \
    X(VolumeFlow,0x38,0x3F, Quantity::Flow, Unit::M3H) \
    X(FlowTemperature,0x58,0x5B, Quantity::Temperature, Unit::C) \
    X(ReturnTemperature,0x5C,0x5F, Quantity::Temperature, Unit::C) \
    X(TemperatureDifference,0x60,0x63, Quantity::Temperature, Unit::C) \
    X(ExternalTemperature,0x64,0x67, Quantity::Temperature, Unit::C) \
    X(HeatCostAllocation,0x6E,0x6E, Quantity::HCA, Unit::HCA) \
    X(Date,0x6C,0x6C, Quantity::PointInTime, Unit::DateTimeLT) \
    X(DateTime,0x6D,0x6D, Quantity::PointInTime, Unit::DateTimeLT) \
    X(EnergyMJ,0x0E,0x0F, Quantity::Energy, Unit::MJ) \
    X(EnergyWh,0x00,0x07, Quantity::Energy, Unit::KWH) \
    X(PowerW,0x28,0x2f, Quantity::Power, Unit::KW) \
    X(ActualityDuration,0x74,0x77, Quantity::Time, Unit::Second) \
    X(FabricationNo,0x78,0x78, Quantity::Text, Unit::TXT) \
    X(EnhancedIdentification,0x79,0x79, Quantity::Text, Unit::TXT) \
    X(AnyVolumeVIF,0x00,0x00, Quantity::Volume, Unit::Unknown) \
    X(AnyEnergyVIF,0x00,0x00, Quantity::Energy, Unit::Unknown)  \
    X(AnyPowerVIF,0x00,0x00, Quantity::Power, Unit::Unknown)  \

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

enum class MeasurementType
{
    Any,
    Instantaneous,
    Minimum,
    Maximum,
    AtError
};

void extractDV(std::string &s, uchar *dif, uchar *vif, bool *has_difes, bool *has_vifes);

struct DifVifKey
{
    DifVifKey(std::string key) : key_(key) {
        extractDV(key, &dif_, &vif_, &has_difes_, &has_vifes_);
    }
    std::string str() { return key_; }
    bool operator==(DifVifKey &dvk) { return key_ == dvk.key_; }
    uchar dif() { return dif_; }
    uchar vif() { return vif_; }
    bool hasDifes() { return has_difes_; }
    bool hasVifes() { return has_vifes_; }

private:

    std::string key_;
    uchar dif_;
    uchar vif_;
    bool has_difes_;
    bool has_vifes_;
};

void extractDV(DifVifKey &s, uchar *dif, uchar *vif, bool *has_difes, bool *has_vifes);

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

struct DVEntry
{
    int offset; // Where in the telegram this dventry was found.
    DifVifKey dif_vif_key;
    MeasurementType measurement_type;
    Vif vif;
    StorageNr storage_nr;
    TariffNr tariff_nr;
    SubUnitNr subunit_nr;
    std::string value;
    DVEntry(int off, DifVifKey dvk, MeasurementType mt, Vif vi, StorageNr st, TariffNr ta, SubUnitNr su, std::string &val) :
        offset(off), dif_vif_key(dvk), measurement_type(mt), vif(vi), storage_nr(st), tariff_nr(ta), subunit_nr(su), value(val) {}
    DVEntry() :
        offset(999999), dif_vif_key("????"), measurement_type(MeasurementType::Instantaneous), vif(0), storage_nr(0), tariff_nr(0), subunit_nr(0), value("x") {}

    bool extractDouble(double *out, bool auto_scale, bool assume_signed);
    bool extractLong(uint64_t *out);
    bool extractDate(struct tm *out);
    bool extractReadableString(std::string *out);
    bool hasVifes();
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

    // Match the storage nr.
    bool match_storage_nr = false;
    StorageNr storage_nr_from { 0 };
    StorageNr storage_nr_to { 0 };

    // Match the tariff nr.
    bool match_tariff_nr = false;
    TariffNr tariff_nr_from { 0 };
    TariffNr tariff_nr_to { 0 };

    // Match the subunit.
    bool match_subunit_nr = false;
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
};

bool loadFormatBytesFromSignature(uint16_t format_signature, std::vector<uchar> *format_bytes);

struct Telegram;

bool parseDV(Telegram *t,
             std::vector<uchar> &databytes,
             std::vector<uchar>::iterator data,
             size_t data_len,
             std::map<std::string,std::pair<int,DVEntry>> *dv_entries,
             std::vector<DVEntry*> *dv_entries_ordered,
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
