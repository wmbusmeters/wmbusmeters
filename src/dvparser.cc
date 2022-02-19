/*
 Copyright (C) 2018-2020 Fredrik Öhrström (gpl-3.0-or-later)

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

#include"dvparser.h"
#include"util.h"

#include<assert.h>
#include<memory.h>

// The parser should not crash on invalid data, but yeah, when I
// need to debug it because it crashes on invalid data, then
// I enable the following define...
//#define DEBUG_PARSER(...) fprintf(stdout, __VA_ARGS__)
#define DEBUG_PARSER(...)

using namespace std;

const char *toString(ValueInformation v)
{
    switch (v) {
        case ValueInformation::None: return "None";
        case ValueInformation::Any: return "Any";
#define X(name,from,to,quantity,unit) case ValueInformation::name: return #name;
LIST_OF_VALUETYPES
#undef X
    }
    assert(0);
}

Unit toDefaultUnit(ValueInformation v)
{
    switch (v) {
    case ValueInformation::Any:
    case ValueInformation::None:
        assert(0);
        break;
#define X(name,from,to,quantity,unit) case ValueInformation::name: return unit;
LIST_OF_VALUETYPES
#undef X
    }
    assert(0);
}

ValueInformation toValueInformation(int i)
{
#define X(name,from,to,quantity,unit) if (from <= i && i <= to) return ValueInformation::name;
LIST_OF_VALUETYPES
#undef X
    return ValueInformation::None;
}

map<uint16_t,string> hash_to_format_;

bool loadFormatBytesFromSignature(uint16_t format_signature, vector<uchar> *format_bytes)
{
    if (hash_to_format_.count(format_signature) > 0) {
        debug("(dvparser) found remembered format for hash %x\n", format_signature);
        // Return the proper hash!
        hex2bin(hash_to_format_[format_signature], format_bytes);
        return true;
    }
    // Unknown format signature.
    return false;
}

bool parseDV(Telegram *t,
             vector<uchar> &databytes,
             vector<uchar>::iterator data,
             size_t data_len,
             map<string,pair<int,DVEntry>> *values,
             vector<uchar>::iterator *format,
             size_t format_len,
             uint16_t *format_hash)
{
    map<string,int> dv_count;
    vector<uchar> format_bytes;
    vector<uchar> id_bytes;
    vector<uchar> data_bytes;
    string dv, key;
    size_t start_parse_here = t->parsed.size();
    vector<uchar>::iterator data_start = data;
    vector<uchar>::iterator data_end = data+data_len;
    vector<uchar>::iterator format_end;
    bool data_has_difvifs = true;
    bool variable_length = false;

    if (format == NULL) {
        // No format string was supplied, we therefore assume
        // that the difvifs necessary to parse the data is
        // part of the data! This is the default.
        format = &data;
        format_end = data_end;
    } else {
        // A format string has been supplied. The data is compressed,
        // and can only be decoded using the supplied difvifs.
        // Since the data does not have the difvifs.
        data_has_difvifs = false;
        format_end = *format+format_len;
        string s = bin2hex(*format, format_end, format_len);
        debug("(dvparser) using format \"%s\"\n", s.c_str());
    }

    // Data format is:

    // DIF byte (defines how the binary data bits should be decoded and how man data bytes there are)
    // Sometimes followed by one or more dife bytes, if the 0x80 high bit is set.
    // The last dife byte does not have the 0x80 bit set.

    // VIF byte (defines what the decoded value means, water,energy,power,etc.)
    // Sometimes followed by one or more vife bytes, if the 0x80 high bit is set.
    // The last vife byte does not have the 0x80 bit set.

    // Data bytes, the number of data bytes are defined by the dif format.
    // Or if the dif says variable length, then the first data byte specifies the number of data bytes.

    // DIF again...

    // A Dif(Difes)Vif(Vifes) identifier can be for example be the 02FF20 for the Multical21
    // vendor specific status bits. The parser then uses this identifier as a key to store the
    // data bytes in a map. The same identifier could occur several times in a telegram,
    // even though it often don't. Since the first occurence is stored under 02FF20,
    // the second identical identifier stores its data under the key "02FF20_2" etc for 3 and forth...
    // A proper meter would use storagenr etc to differentiate between different measurements of
    // the same value.

    format_bytes.clear();
    id_bytes.clear();
    for (;;)
    {
        id_bytes.clear();
        DEBUG_PARSER("(dvparser debug) Remaining format data %ju\n", std::distance(*format,format_end));
        if (*format == format_end) break;
        uchar dif = **format;

        MeasurementType mt = difMeasurementType(dif);
        int datalen = difLenBytes(dif);
        DEBUG_PARSER("(dvparser debug) dif=%02x datalen=%d \"%s\" type=%s\n", dif, datalen, difType(dif).c_str(),
                     measurementTypeName(mt).c_str());
        if (datalen == -2)
        {
            if (dif == 0x0f)
            {
                DEBUG_PARSER("(dvparser) reached manufacturer specific data 0f, parsing is done.\n");
                datalen = std::distance(data,data_end);
                string value = bin2hex(data+1, data_end, datalen-1);
                t->mfct_0f_index = 1+std::distance(data_start, data);
                assert(t->mfct_0f_index >= 0);
                t->addExplanationAndIncrementPos(data, datalen, KindOfData::PROTOCOL, Understanding::NONE, "%02X manufacturer specific data %s", dif, value.c_str());
                break;
            }
            debug("(dvparser) cannot handle dif %02X ignoring rest of telegram.\n", dif);
            break;
        }
        if (dif == 0x2f) {
            t->addExplanationAndIncrementPos(*format, 1, KindOfData::PROTOCOL, Understanding::FULL, "%02X skip", dif);
            DEBUG_PARSER("\n");
            continue;
        }
        if (datalen == -1) {
            variable_length = true;
        } else {
            variable_length = false;
        }
        if (data_has_difvifs) {
            format_bytes.push_back(dif);
            id_bytes.push_back(dif);
            t->addExplanationAndIncrementPos(*format, 1, KindOfData::PROTOCOL, Understanding::FULL, "%02X dif (%s)", dif, difType(dif).c_str());
        } else {
            id_bytes.push_back(**format);
            (*format)++;
        }


        int difenr = 0;
        int subunit = 0;
        int tariff = 0;
        int lsb_of_storage_nr = (dif & 0x40) >> 6;
        int storage_nr = lsb_of_storage_nr;

        bool has_another_dife = (dif & 0x80) == 0x80;

        while (has_another_dife) {
            if (*format == format_end) { debug("(dvparser) warning: unexpected end of data (dife expected)\n"); break; }
            uchar dife = **format;
            int subunit_bit = (dife & 0x40) >> 6;
            subunit |= subunit_bit << difenr;
            int tariff_bits = (dife & 0x30) >> 4;
            tariff |= tariff_bits << (difenr*2);
            int storage_nr_bits = (dife & 0x0f);
            storage_nr |= storage_nr_bits << (1+difenr*4);

            DEBUG_PARSER("(dvparser debug) dife=%02x (subunit=%d tariff=%d storagenr=%d)\n",
                         dife, subunit, tariff, storage_nr);
            if (data_has_difvifs) {
                format_bytes.push_back(dife);
                id_bytes.push_back(dife);
                t->addExplanationAndIncrementPos(*format, 1, KindOfData::PROTOCOL, Understanding::FULL,
                                                 "%02X dife (subunit=%d tariff=%d storagenr=%d)",
                                                 dife, subunit, tariff, storage_nr);
            } else {
                id_bytes.push_back(**format);
                (*format)++;
            }

            has_another_dife = (dife & 0x80) == 0x80;
            difenr++;
        }

        if (*format == format_end) { debug("(dvparser) warning: unexpected end of data (vif expected)\n"); break; }

        uchar vif = **format;
        DEBUG_PARSER("(dvparser debug) vif=%02x \"%s\"\n", vif, vifType(vif).c_str());
        if (data_has_difvifs) {
            format_bytes.push_back(vif);
            id_bytes.push_back(vif);
            t->addExplanationAndIncrementPos(*format, 1, KindOfData::PROTOCOL, Understanding::FULL,
                                             "%02X vif (%s)", vif, vifType(vif).c_str());
        } else {
            id_bytes.push_back(**format);
            (*format)++;
        }

        // Grabbing a variable length vif. This does not currently work
        // with the compact format.
        if (vif == 0x7c)
        {
            DEBUG_PARSER("(dvparser debug) variable length vif found\n");
            if (*format == format_end) { debug("(dvparser) warning: unexpected end of data (vif varlen expected)\n"); break; }
            uchar viflen = **format;
            t->addExplanationAndIncrementPos(*format, 1, KindOfData::PROTOCOL, Understanding::FULL,
                                             "%02X viflen (%d)", viflen, viflen);
            for (uchar i = 0; i < viflen; ++i)
            {
                if (*format == format_end) { debug("(dvparser) warning: unexpected end of data (vif varlen byte %d/%d expected)\n",
                                                   i+1, viflen); break; }
                uchar v = **format;
                t->addExplanationAndIncrementPos(*format, 1, KindOfData::PROTOCOL, Understanding::FULL,
                                                 "%02X vif (%c)", v, v);
                id_bytes.push_back(v);
            }
        }

        bool has_another_vife = (vif & 0x80) == 0x80;
        while (has_another_vife) {
            if (*format == format_end) { debug("(dvparser) warning: unexpected end of data (vife expected)\n"); break; }
            uchar vife = **format;
            DEBUG_PARSER("(dvparser debug) vife=%02x (%s)\n", vife, vifeType(dif, vif, vife).c_str());
            if (data_has_difvifs) {
                format_bytes.push_back(vife);
                id_bytes.push_back(vife);
                t->addExplanationAndIncrementPos(*format, 1, KindOfData::PROTOCOL, Understanding::FULL,
                                                 "%02X vife (%s)", vife, vifeType(dif, vif, vife).c_str());
            } else {
                id_bytes.push_back(**format);
                (*format)++;
            }
            has_another_vife = (vife & 0x80) == 0x80;
        }

        dv = "";
        for (uchar c : id_bytes) {
            char hex[3];
            hex[2] = 0;
            snprintf(hex, 3, "%02X", c);
            dv.append(hex);
        }
        DEBUG_PARSER("(dvparser debug) key \"%s\"\n", dv.c_str());

        int count = ++dv_count[dv];
        if (count > 1) {
            strprintf(key, "%s_%d", dv.c_str(), count);
        } else {
            strprintf(key, "%s", dv.c_str());
        }
        DEBUG_PARSER("(dvparser debug) DifVif key is %s\n", key.c_str());

        int remaining = std::distance(data, data_end);
        if (variable_length) {
            DEBUG_PARSER("(dvparser debug) varlen %02x\n", *(data+0));
            datalen = *(data);
        }
        DEBUG_PARSER("(dvparser debug) remaining data %d len=%d\n", remaining, datalen);
        if (remaining < datalen) {
            debug("(dvparser) warning: unexpected end of data\n");
            datalen = remaining-1;
        }

        // Skip the length byte in the variable length data.
        if (variable_length) {
            t->addExplanationAndIncrementPos(data, 1, KindOfData::PROTOCOL, Understanding::FULL, "%02X varlen=%d", *(data+0), datalen);
        }
        string value = bin2hex(data, data_end, datalen);
        int offset = start_parse_here+data-data_start;
        (*values)[key] = { offset, DVEntry(mt, vif&0x7f, storage_nr, tariff, subunit, value) };
        if (value.length() > 0) {
            // This call increments data with datalen.
            t->addExplanationAndIncrementPos(data, datalen, KindOfData::CONTENT, Understanding::NONE, "%s", value.c_str());
            DEBUG_PARSER("(dvparser debug) data \"%s\"\n\n", value.c_str());
        }
        if (remaining == datalen || data == databytes.end()) {
            // We are done here!
            break;
        }
    }

    string format_string = bin2hex(format_bytes);
    uint16_t hash = crc16_EN13757(&format_bytes[0], format_bytes.size());

    if (data_has_difvifs) {
        if (hash_to_format_.count(hash) == 0) {
            hash_to_format_[hash] = format_string;
            debug("(dvparser) found new format \"%s\" with hash %x, remembering!\n", format_string.c_str(), hash);
        }
    }

    return true;
}

void valueInfoRange(ValueInformation v, int *low, int *hi)
{
    switch (v) {
    case ValueInformation::Any:
    case ValueInformation::None: *low = 0; *hi = 0; return;
#define X(name,from,to,quantity,unit) case ValueInformation::name: *low = from; *hi = to; return;
LIST_OF_VALUETYPES
#undef X
    }
    assert(0);
}

bool matchSingleVif(int vi, ValueInformation vif)
{
    int low, hi;
    valueInfoRange(vif, &low, &hi);

    return vi >= low && vi <= hi;
}

bool isVIFMatch(int vi, ValueInformation vif)
{
    if (vif == ValueInformation::AnyVolumeVIF)
    {
        // There are more volume units in the standard that will be added here.
        return matchSingleVif(vi, ValueInformation::Volume);
    }
    if (vif == ValueInformation::AnyEnergyVIF)
    {
        return
            matchSingleVif(vi, ValueInformation::EnergyWh) ||
            matchSingleVif(vi, ValueInformation::EnergyMJ);
    }
    if (vif == ValueInformation::AnyPowerVIF)
    {
        // There are more power units in the standard that will be added here.
        return matchSingleVif(vi, ValueInformation::PowerW);
    }

    return matchSingleVif(vi, vif);
}

bool hasKey(std::map<std::string,std::pair<int,DVEntry>> *values, std::string key)
{
    return values->count(key) > 0;
}

bool findKey(MeasurementType mit, ValueInformation vif, int storagenr, int tariffnr,
             std::string *key, std::map<std::string,std::pair<int,DVEntry>> *values)
{
    return findKeyWithNr(mit, vif, storagenr, tariffnr, 1, key, values);
}

bool findKeyWithNr(MeasurementType mit, ValueInformation vif, int storagenr, int tariffnr, int nr,
                   std::string *key, std::map<std::string,std::pair<int,DVEntry>> *values)
{
    /*debug("(dvparser) looking for type=%s vif=%s storagenr=%d value_ran_low=%02x value_ran_hi=%02x\n",
          measurementTypeName(mit).c_str(), toString(vif), storagenr,
          low, hi);*/

    for (auto& v : *values)
    {
        MeasurementType ty = v.second.second.type;
        int vi = v.second.second.value_information;
        int sn = v.second.second.storagenr;
        int tn = v.second.second.tariff;
        /*debug("(dvparser) match? %s type=%s vif=%02x (%s) and storagenr=%d\n",
              v.first.c_str(),
              measurementTypeName(ty).c_str(), vi, toString(toValueInformation(vi)), storagenr, sn);*/

        if (isVIFMatch(vi, vif) &&
            (mit == MeasurementType::Unknown || mit == ty) &&
            (storagenr == ANY_STORAGENR || storagenr == sn) &&
            (tariffnr == ANY_TARIFFNR || tariffnr == tn))
        {
            *key = v.first;
            nr--;
            if (nr <= 0) return true;
            /*debug("(dvparser) found key %s for type=%s vif=%02x (%s) storagenr=%d\n",
                  v.first.c_str(), measurementTypeName(ty).c_str(),
                  vi, toString(toValueInformation(vi)), storagenr);*/
        }
    }
    return false;
}

void extractDV(string &s, uchar *dif, uchar *vif)
{
    vector<uchar> bytes;
    hex2bin(s, &bytes);
    *dif = bytes[0];
    bool has_another_dife = (*dif & 0x80) == 0x80;
    size_t i=1;
    while (has_another_dife) {
        if (i >= bytes.size()) {
            debug("(dvparser) Invalid key \"%s\" used. Settinf vif to zero.\n", s.c_str());
            *vif = 0;
            return;
        }
        uchar dife = bytes[i];
        has_another_dife = (dife & 0x80) == 0x80;
        i++;
    }

    *vif = bytes[i];
}

bool extractDVuint8(map<string,pair<int,DVEntry>> *values,
                    string key,
                    int *offset,
                    uchar *value)
{
    if ((*values).count(key) == 0) {
        verbose("(dvparser) warning: cannot extract uint8 from non-existant key \"%s\"\n", key.c_str());
        *offset = -1;
        *value = 0;
        return false;
    }
    uchar dif, vif;
    extractDV(key, &dif, &vif);

    pair<int,DVEntry>&  p = (*values)[key];
    *offset = p.first;
    vector<uchar> v;
    hex2bin(p.second.value, &v);

    *value = v[0];
    return true;
}

bool extractDVuint16(map<string,pair<int,DVEntry>> *values,
                     string key,
                     int *offset,
                     uint16_t *value)
{
    if ((*values).count(key) == 0) {
        verbose("(dvparser) warning: cannot extract uint16 from non-existant key \"%s\"\n", key.c_str());
        *offset = -1;
        *value = 0;
        return false;
    }
    uchar dif, vif;
    extractDV(key, &dif, &vif);

    pair<int,DVEntry>&  p = (*values)[key];
    *offset = p.first;
    vector<uchar> v;
    hex2bin(p.second.value, &v);

    *value = v[1]<<8 | v[0];
    return true;
}

bool extractDVuint24(map<string,pair<int,DVEntry>> *values,
                     string key,
                     int *offset,
                     uint32_t *value)
{
    if ((*values).count(key) == 0) {
        verbose("(dvparser) warning: cannot extract uint24 from non-existant key \"%s\"\n", key.c_str());
        *offset = -1;
        *value = 0;
        return false;
    }
    uchar dif, vif;
    extractDV(key, &dif, &vif);

    pair<int,DVEntry>&  p = (*values)[key];
    *offset = p.first;
    vector<uchar> v;
    hex2bin(p.second.value, &v);

    *value = v[2] << 16 | v[1]<<8 | v[0];
    return true;
}

bool extractDVuint32(map<string,pair<int,DVEntry>> *values,
                     string key,
                     int *offset,
                     uint32_t *value)
{
    if ((*values).count(key) == 0) {
        verbose("(dvparser) warning: cannot extract uint32 from non-existant key \"%s\"\n", key.c_str());
        *offset = -1;
        *value = 0;
        return false;
    }
    uchar dif, vif;
    extractDV(key, &dif, &vif);

    pair<int,DVEntry>&  p = (*values)[key];
    *offset = p.first;
    vector<uchar> v;
    hex2bin(p.second.value, &v);

    *value = (uint32_t(v[3]) << 24) |  (uint32_t(v[2]) << 16) | (uint32_t(v[1])<<8) | uint32_t(v[0]);
    return true;
}

bool extractDVdouble(map<string,pair<int,DVEntry>> *values,
                     string key,
                     int *offset,
                     double *value,
                     bool auto_scale,
                     bool assume_signed)
{
    if ((*values).count(key) == 0) {
        verbose("(dvparser) warning: cannot extract double from non-existant key \"%s\"\n", key.c_str());
        *offset = 0;
        *value = 0;
        return false;
    }
    uchar dif, vif;
    extractDV(key, &dif, &vif);

    pair<int,DVEntry>&  p = (*values)[key];
    *offset = p.first;

    if (p.second.value.length() == 0) {
        verbose("(dvparser) warning: key found but no data  \"%s\"\n", key.c_str());
        *offset = 0;
        *value = 0;
        return false;
    }

    int t = dif&0xf;
    if (t == 0x1 || // 8 Bit Integer/Binary
        t == 0x2 || // 16 Bit Integer/Binary
        t == 0x3 || // 24 Bit Integer/Binary
        t == 0x4 || // 32 Bit Integer/Binary
        t == 0x6 || // 48 Bit Integer/Binary
        t == 0x7)   // 64 Bit Integer/Binary
    {
        vector<uchar> v;
        hex2bin(p.second.value, &v);
        uint64_t raw = 0;
        bool negate = false;
        uint64_t negate_mask = 0;
        if (t == 0x1) {
            assert(v.size() == 1);
            raw = v[0];
            if (assume_signed && (raw & (uint64_t)0x80UL) != 0) { negate = true; negate_mask = ~((uint64_t)0)<<8; }
        } else if (t == 0x2) {
            assert(v.size() == 2);
            raw = v[1]*256 + v[0];
            if (assume_signed && (raw & (uint64_t)0x8000UL) != 0) { negate = true; negate_mask = ~((uint64_t)0)<<16; }
        } else if (t == 0x3) {
            assert(v.size() == 3);
            raw = v[2]*256*256 + v[1]*256 + v[0];
            if (assume_signed && (raw & (uint64_t)0x800000UL) != 0) { negate = true; negate_mask = ~((uint64_t)0)<<24; }
        } else if (t == 0x4) {
            assert(v.size() == 4);
            raw = ((unsigned int)v[3])*256*256*256
                + ((unsigned int)v[2])*256*256
                + ((unsigned int)v[1])*256
                + ((unsigned int)v[0]);
            if (assume_signed && (raw & (uint64_t)0x80000000UL) != 0) { negate = true; negate_mask = ~((uint64_t)0)<<32; }
        } else if (t == 0x6) {
            assert(v.size() == 6);
            raw = ((uint64_t)v[5])*256*256*256*256*256
                + ((uint64_t)v[4])*256*256*256*256
                + ((uint64_t)v[3])*256*256*256
                + ((uint64_t)v[2])*256*256
                + ((uint64_t)v[1])*256
                + ((uint64_t)v[0]);
            if (assume_signed && (raw & (uint64_t)0x800000000000UL) != 0) { negate = true; negate_mask = ~((uint64_t)0)<<48; }
        } else if (t == 0x7) {
            assert(v.size() == 8);
            raw = ((uint64_t)v[7])*256*256*256*256*256*256*256
                + ((uint64_t)v[6])*256*256*256*256*256*256
                + ((uint64_t)v[5])*256*256*256*256*256
                + ((uint64_t)v[4])*256*256*256*256
                + ((uint64_t)v[3])*256*256*256
                + ((uint64_t)v[2])*256*256
                + ((uint64_t)v[1])*256
                + ((uint64_t)v[0]);
            if (assume_signed && (raw & (uint64_t)0x8000000000000000UL) != 0) { negate = true; negate_mask = 0; }
        }
        double scale = 1.0;
        double draw = (double)raw;
        if (negate)
        {
            draw = (double)((int64_t)(negate_mask | raw));
        }
        if (auto_scale) scale = vifScale(vif);
        *value = (draw) / scale;
    }
    else
    if (t == 0x9 || // 2 digit BCD
        t == 0xA || // 4 digit BCD
        t == 0xB || // 6 digit BCD
        t == 0xC || // 8 digit BCD
        t == 0xE)   // 12 digit BCD
    {
        // 74140000 -> 00001474
        string& v = p.second.value;
        uint64_t raw = 0;
        bool negate = false;
        // assert(assume_signed == false); // We do not expect negative bcd values.
        // Even though it is theoretically possible with nines complement.
        if (t == 0x9) {
            assert(v.size() == 2);
            if (assume_signed && v[0] == 'F') { negate = true; v[0] = '0'; }
            raw = (v[0]-'0')*10 + (v[1]-'0');
        } else if (t ==  0xA) {
            assert(v.size() == 4);
            if (assume_signed && v[2] == 'F') { negate = true; v[2] = '0'; }
            raw = (v[2]-'0')*10*10*10 + (v[3]-'0')*10*10
                + (v[0]-'0')*10 + (v[1]-'0');
        } else if (t ==  0xB) {
            assert(v.size() == 6);
            if (assume_signed && v[4] == 'F') { negate = true; v[4] = '0'; }
            raw = (v[4]-'0')*10*10*10*10*10 + (v[5]-'0')*10*10*10*10
                + (v[2]-'0')*10*10*10 + (v[3]-'0')*10*10
                + (v[0]-'0')*10 + (v[1]-'0');
        } else if (t ==  0xC) {
            assert(v.size() == 8);
            if (assume_signed && v[6] == 'F') { negate = true; v[6] = '0'; }
            raw = (v[6]-'0')*10*10*10*10*10*10*10 + (v[7]-'0')*10*10*10*10*10*10
                + (v[4]-'0')*10*10*10*10*10 + (v[5]-'0')*10*10*10*10
                + (v[2]-'0')*10*10*10 + (v[3]-'0')*10*10
                + (v[0]-'0')*10 + (v[1]-'0');
        } else if (t ==  0xE) {
            assert(v.size() == 12);
            if (assume_signed && v[10] == 'F') { negate = true; v[10] = '0'; }
            raw =(v[10]-'0')*10*10*10*10*10*10*10*10*10*10*10 + (v[11]-'0')*10*10*10*10*10*10*10*10*10*10
                + (v[8]-'0')*10*10*10*10*10*10*10*10*10 + (v[9]-'0')*10*10*10*10*10*10*10*10
                + (v[6]-'0')*10*10*10*10*10*10*10 + (v[7]-'0')*10*10*10*10*10*10
                + (v[4]-'0')*10*10*10*10*10 + (v[5]-'0')*10*10*10*10
                + (v[2]-'0')*10*10*10 + (v[3]-'0')*10*10
                + (v[0]-'0')*10 + (v[1]-'0');
        }
        double scale = 1.0;
        double draw = (double)raw;
        if (negate)
        {
            draw = (double)draw * -1;
        }
        if (auto_scale) scale = vifScale(vif);
        *value = (draw) / scale;
    }
    else
    {
        error("Unsupported dif format for extraction to double! dif=%02x\n", dif);
    }

    return true;
}

bool extractDVlong(map<string,pair<int,DVEntry>> *values,
                   string key,
                   int *offset,
                   uint64_t *value)
{
    if ((*values).count(key) == 0) {
        verbose("(dvparser) warning: cannot extract long from non-existant key \"%s\"\n", key.c_str());
        *offset = 0;
        *value = 0;
        return false;
    }
    uchar dif, vif;
    extractDV(key, &dif, &vif);

    pair<int,DVEntry>&  p = (*values)[key];
    *offset = p.first;

    if (p.second.value.length() == 0) {
        verbose("(dvparser) warning: key found but no data  \"%s\"\n", key.c_str());
        *offset = 0;
        *value = 0;
        return false;
    }

    int t = dif&0xf;
    if (t == 0x1 || // 8 Bit Integer/Binary
        t == 0x2 || // 16 Bit Integer/Binary
        t == 0x3 || // 24 Bit Integer/Binary
        t == 0x4 || // 32 Bit Integer/Binary
        t == 0x6 || // 48 Bit Integer/Binary
        t == 0x7)   // 64 Bit Integer/Binary
    {
        vector<uchar> v;
        hex2bin(p.second.value, &v);
        uint64_t raw = 0;
        if (t == 0x1) {
            assert(v.size() == 1);
            raw = v[0];
        } else if (t == 0x2) {
            assert(v.size() == 2);
            raw = v[1]*256 + v[0];
        } else if (t == 0x3) {
            assert(v.size() == 3);
            raw = v[2]*256*256 + v[1]*256 + v[0];
        } else if (t == 0x4) {
            assert(v.size() == 4);
            raw = ((unsigned int)v[3])*256*256*256
                + ((unsigned int)v[2])*256*256
                + ((unsigned int)v[1])*256
                + ((unsigned int)v[0]);
        } else if (t == 0x6) {
            assert(v.size() == 6);
            raw = ((uint64_t)v[5])*256*256*256*256*256
                + ((uint64_t)v[4])*256*256*256*256
                + ((uint64_t)v[3])*256*256*256
                + ((uint64_t)v[2])*256*256
                + ((uint64_t)v[1])*256
                + ((uint64_t)v[0]);
        } else if (t == 0x7) {
            assert(v.size() == 8);
            raw = ((uint64_t)v[7])*256*256*256*256*256*256*256
                + ((uint64_t)v[6])*256*256*256*256*256*256
                + ((uint64_t)v[5])*256*256*256*256*256
                + ((uint64_t)v[4])*256*256*256*256
                + ((uint64_t)v[3])*256*256*256
                + ((uint64_t)v[2])*256*256
                + ((uint64_t)v[1])*256
                + ((uint64_t)v[0]);
        }
        *value = raw;
    }
    else
    if (t == 0x9 || // 2 digit BCD
        t == 0xA || // 4 digit BCD
        t == 0xB || // 6 digit BCD
        t == 0xC || // 8 digit BCD
        t == 0xE)   // 12 digit BCD
    {
        // 74140000 -> 00001474
        string& v = p.second.value;
        uint64_t raw = 0;
        if (t == 0x9) {
            assert(v.size() == 2);
            raw = (v[0]-'0')*10 + (v[1]-'0');
        } else if (t ==  0xA) {
            assert(v.size() == 4);
            raw = (v[2]-'0')*10*10*10 + (v[3]-'0')*10*10
                + (v[0]-'0')*10 + (v[1]-'0');
        } else if (t ==  0xB) {
            assert(v.size() == 6);
            raw = (v[4]-'0')*10*10*10*10*10 + (v[5]-'0')*10*10*10*10
                + (v[2]-'0')*10*10*10 + (v[3]-'0')*10*10
                + (v[0]-'0')*10 + (v[1]-'0');
        } else if (t ==  0xC) {
            assert(v.size() == 8);
            raw = (v[6]-'0')*10*10*10*10*10*10*10 + (v[7]-'0')*10*10*10*10*10*10
                + (v[4]-'0')*10*10*10*10*10 + (v[5]-'0')*10*10*10*10
                + (v[2]-'0')*10*10*10 + (v[3]-'0')*10*10
                + (v[0]-'0')*10 + (v[1]-'0');
        } else if (t ==  0xE) {
            assert(v.size() == 12);
            raw =(v[10]-'0')*10*10*10*10*10*10*10*10*10*10*10 + (v[11]-'0')*10*10*10*10*10*10*10*10*10*10
                + (v[8]-'0')*10*10*10*10*10*10*10*10*10 + (v[9]-'0')*10*10*10*10*10*10*10*10
                + (v[6]-'0')*10*10*10*10*10*10*10 + (v[7]-'0')*10*10*10*10*10*10
                + (v[4]-'0')*10*10*10*10*10 + (v[5]-'0')*10*10*10*10
                + (v[2]-'0')*10*10*10 + (v[3]-'0')*10*10
                + (v[0]-'0')*10 + (v[1]-'0');
        }

        *value = raw;
    }
    else
    {
        error("Unsupported dif format for extraction to long! dif=%02x\n", dif);
    }

    return true;
}

bool extractDVHexString(map<string,pair<int,DVEntry>> *values,
                        string key,
                        int *offset,
                        string *value)
{
    if ((*values).count(key) == 0) {
        verbose("(dvparser) warning: cannot extract string from non-existant key \"%s\"\n", key.c_str());
        *offset = -1;
        return false;
    }
    pair<int,DVEntry>&  p = (*values)[key];
    *offset = p.first;
    *value = p.second.value;

    return true;
}


bool extractDVReadableString(map<string,pair<int,DVEntry>> *values,
                             string key,
                             int *offset,
                             string *value)
{
    if ((*values).count(key) == 0) {
        verbose("(dvparser) warning: cannot extract string from non-existant key \"%s\"\n", key.c_str());
        *offset = -1;
        return false;
    }
    uchar dif, vif;
    extractDV(key, &dif, &vif);
    int t = dif&0xf;

    pair<int,DVEntry>&  p = (*values)[key];
    *offset = p.first;

    string v = p.second.value;

    if (t == 0x1 || // 8 Bit Integer/Binary
        t == 0x2 || // 16 Bit Integer/Binary
        t == 0x3 || // 24 Bit Integer/Binary
        t == 0x4 || // 32 Bit Integer/Binary
        t == 0x6 || // 48 Bit Integer/Binary
        t == 0x7 || // 64 Bit Integer/Binary
        t == 0xD)   // Variable length
    {
        // For example an enhanced id 32 bits binary looks like:
        // 44434241 and will be reversed to: 41424344 and translated using ascii
        // to ABCD
        v = reverseBinaryAsciiSafeToString(v);
    }
    if (t == 0x9 || // 2 digit BCD
        t == 0xA || // 4 digit BCD
        t == 0xB || // 6 digit BCD
        t == 0xC || // 8 digit BCD
        t == 0xE)   // 12 digit BCD
    {
        // For example an enhanced id 12 digit bcd looks like:
        // 618171183100 and will be reversed to: 003118718161
        v = reverseBCD(v);
    }

    *value = v;
    return true;
}

bool extractDate(uchar hi, uchar lo, struct tm *date)
{
    // |     hi    |    lo     |
    // | YYYY MMMM | YYY DDDDD |

    int day   = (0x1f) & lo;
    int year1 = ((0xe0) & lo) >> 5;
    int month = (0x0f) & hi;
    int year2 = ((0xf0) & hi) >> 1;
    int year  = (2000 + year1 + year2);

    date->tm_mday = day;         /* Day of the month (1-31) */
    date->tm_mon = month - 1;    /* Month (0-11) */
    date->tm_year = year - 1900; /* Year - 1900 */

    if (month > 12) return false;
    return true;
}

bool extractTime(uchar hi, uchar lo, struct tm *date)
{
    // |    hi    |    lo    |
    // | ...hhhhh | ..mmmmmm |
    int min  = (0x3f) & lo;
    int hour = (0x1f) & hi;

    date->tm_min = min;
    date->tm_hour = hour;

    if (min > 59) return false;
    if (hour > 23) return false;
    return true;
}

bool extractDVdate(map<string,pair<int,DVEntry>> *values,
                   string key,
                   int *offset,
                   struct tm *value)
{
    if ((*values).count(key) == 0)
    {
        verbose("(dvparser) warning: cannot extract date from non-existant key \"%s\"\n", key.c_str());
        *offset = -1;
        memset(value, 0, sizeof(struct tm));
        return false;
    }
    // This will install the correct timezone
    // offset tm_gmtoff into the timestamp.
    time_t t = time(NULL);
    localtime_r(&t, value);
    value->tm_hour = 0;
    value->tm_min = 0;
    value->tm_sec = 0;
    value->tm_mday = 0;
    value->tm_mon = 0;
    value->tm_year = 0;

    uchar dif, vif;
    extractDV(key, &dif, &vif);

    pair<int,DVEntry>&  p = (*values)[key];
    *offset = p.first;
    vector<uchar> v;
    hex2bin(p.second.value, &v);

    bool ok = true;
    if (v.size() == 2) {
        ok &= extractDate(v[1], v[0], value);
    }
    else if (v.size() == 4) {
        ok &= extractDate(v[3], v[2], value);
        ok &= extractTime(v[1], v[0], value);
    }
    else if (v.size() == 6) {
        ok &= extractDate(v[4], v[3], value);
        ok &= extractTime(v[2], v[1], value);
        // ..ss ssss
        int sec  = (0x3f) & v[0];
        value->tm_sec = sec;
        // some daylight saving time decoding needed here....
    }

    return ok;
}
