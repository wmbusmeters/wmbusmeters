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
    Unknown,
    Instantaneous,
    Minimum,
    Maximum,
    AtError
};

struct DifVifKey
{
    DifVifKey(std::string key) : key_(key) {}
    std::string str() { return key_; }
    bool useSearchInstead() { return key_ == ""; }

private:

    std::string key_;
};

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

private:
    int nr_;
};

static StorageNr AnyStorageNr = StorageNr(-1);

struct TariffNr
{
    TariffNr(int n) : nr_(n) {}
    int intValue() { return nr_; }
    bool operator==(TariffNr s) { return nr_ == s.nr_; }

private:
    int nr_;
};

static TariffNr AnyTariffNr = TariffNr(-1);

struct SubUnitNr
{
    SubUnitNr(int n) : nr_(n) {}
    int intValue() { return nr_; }
    bool operator==(SubUnitNr s) { return nr_ == s.nr_; }

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
    MeasurementType type {};
    Vif vif { 0 };
    StorageNr storagenr { 0 };
    TariffNr tariff { 0 };
    SubUnitNr subunit { 0 };
    std::string value;

    DVEntry() {}
    DVEntry(MeasurementType mt, Vif vi, StorageNr st, TariffNr ta, SubUnitNr su, std::string &val) :
    type(mt), vif(vi), storagenr(st), tariff(ta), subunit(su), value(val) {}
};

struct FieldMatcher
{
    // Exact difvif hex string matching all other checks are ignored.
    bool match_dif_vif_key = false;
    DifVifKey dif_vif_key { "" };

    // Match the measurement_type.
    bool match_measurement_type = false;
    MeasurementType measurement_type { MeasurementType::Instantaneous };

    // Match the value information range. See dvparser.h
    bool match_value_information = false;
    VIFRange value_information { VIFRange::None };

    // Match the storage nr.
    bool match_storage_nr = false;
    StorageNr storage_nr { 0 };

    // Match the tariff nr.
    bool match_tariff_nr = false;
    TariffNr tariff_nr { 0 };

    // Match the subunit.
    bool match_subunit_nr = false;
    SubUnitNr subunit_nr { 0 };

    // If the telegram has multiple identical difvif entries, use entry with this index nr.
    // First entry has nr 1, which is the default value.
    bool match_index_nr = false;
    IndexNr index_nr { 1 };

    static FieldMatcher build() { return FieldMatcher(); }
    void set(DifVifKey k) { dif_vif_key = k; }
    FieldMatcher &set(MeasurementType mt) { measurement_type = mt; return *this; }
    FieldMatcher &set(VIFRange vi) { value_information = vi; return *this; }
    FieldMatcher &set(StorageNr s) { storage_nr = s; return *this; }
    FieldMatcher &set(TariffNr t) { tariff_nr = t; return *this; }
    FieldMatcher &set(IndexNr i) { index_nr = i; return *this; }
};

bool loadFormatBytesFromSignature(uint16_t format_signature, std::vector<uchar> *format_bytes);

struct Telegram;

bool parseDV(Telegram *t,
             std::vector<uchar> &databytes,
             std::vector<uchar>::iterator data,
             size_t data_len,
             std::map<std::string,std::pair<int,DVEntry>> *values,
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

void extractDV(std::string &s, uchar *dif, uchar *vif);

#endif
