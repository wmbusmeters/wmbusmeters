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

#include"dvparser.h"
#include"wmbus.h"
#include"util.h"

#include<assert.h>
#include<memory.h>
#include<limits>

// The parser should not crash on invalid data, but yeah, when I
// need to debug it because it crashes on invalid data, then
// I enable the following define...
//#define DEBUG_PARSER(...) fprintf(stdout, __VA_ARGS__)
#define DEBUG_PARSER(...)

using namespace std;

union RealConversion
{
    uint32_t i;
    float f;
};

const char *toString(VIFRange v)
{
    switch (v) {
        case VIFRange::None: return "None";
        case VIFRange::Any: return "Any";
#define X(name,from,to,quantity,unit) case VIFRange::name: return #name;
LIST_OF_VIF_RANGES
#undef X
    }
    assert(0);
}

VIFRange toVIFRange(const char *s)
{
    if (!strcmp(s, "None")) return VIFRange::None;
    if (!strcmp(s, "Any")) return VIFRange::Any;
#define X(name,from,to,quantity,unit) if (!strcmp(s, #name)) return VIFRange::name;
LIST_OF_VIF_RANGES
#undef X

    return VIFRange::None;
}

const char *toString(VIFCombinable v)
{
    switch (v) {
        case VIFCombinable::None: return "None";
        case VIFCombinable::Any: return "Any";
#define X(name,from,to) case VIFCombinable::name: return #name;
LIST_OF_VIF_COMBINABLES
#undef X
    }
    assert(0);
}

VIFCombinable toVIFCombinable(int i)
{
#define X(name,from,to) if (from <= i && i <= to) return VIFCombinable::name;
LIST_OF_VIF_COMBINABLES
#undef X
    return VIFCombinable::None;
}

Unit toDefaultUnit(Vif v)
{
#define X(name,from,to,quantity,unit) { if (from <= v.intValue() && v.intValue() <= to) return unit; }
LIST_OF_VIF_RANGES
#undef X
    return Unit::Unknown;
}

Unit toDefaultUnit(VIFRange v)
{
    switch (v) {
    case VIFRange::Any:
    case VIFRange::None:
        assert(0);
        break;
#define X(name,from,to,quantity,unit) case VIFRange::name: return unit;
LIST_OF_VIF_RANGES
#undef X
    }
    assert(0);
}

VIFRange toVIFRange(int i)
{
#define X(name,from,to,quantity,unit) if (from <= i && i <= to) return VIFRange::name;
LIST_OF_VIF_RANGES
#undef X
    return VIFRange::None;
}

bool isInsideVIFRange(Vif vif, VIFRange vif_range)
{
    if (vif_range == VIFRange::AnyVolumeVIF)
    {
        // There are more volume units in the standard that will be added here.
        return isInsideVIFRange(vif, VIFRange::Volume);
    }
    if (vif_range == VIFRange::AnyEnergyVIF)
    {
        return
            isInsideVIFRange(vif, VIFRange::EnergyWh) ||
            isInsideVIFRange(vif, VIFRange::EnergyMJ);
    }
    if (vif_range == VIFRange::AnyPowerVIF)
    {
        // There are more power units in the standard that will be added here.
        return isInsideVIFRange(vif, VIFRange::PowerW);
    }

#define X(name,from,to,quantity,unit) if (VIFRange::name == vif_range) { return from <= vif.intValue() && vif.intValue() <= to; }
LIST_OF_VIF_RANGES
#undef X
    return false;
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
             map<string,pair<int,DVEntry>> *dv_entries,
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
    int force_mfct_index = t->force_mfct_index;

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

    dv_entries->clear();

    // Data format is:

    // DIF byte (defines how the binary data bits should be decoded and howy man data bytes there are)
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

        if (force_mfct_index != -1)
        {
            // This is an old meter without a proper 0f or other hear start manufacturer data marker.
            int index = std::distance(data_start, data);

            if (index >= force_mfct_index)
            {
                DEBUG_PARSER("(dvparser) manufacturer specific data, parsing is done.\n", dif);
                size_t datalen = std::distance(data, data_end);
                string value = bin2hex(data, data_end, datalen);
                t->addExplanationAndIncrementPos(data, datalen, KindOfData::CONTENT, Understanding::NONE, "manufacturer specific data %s", value.c_str());
                break;
            }
        }

        uchar dif = **format;

        MeasurementType mt = difMeasurementType(dif);
        int datalen = difLenBytes(dif);
        DEBUG_PARSER("(dvparser debug) dif=%02x datalen=%d \"%s\" type=%s\n", dif, datalen, difType(dif).c_str(),
                     measurementTypeName(mt).c_str());

        if (datalen == -2)
        {
            if (dif == 0x0f)
            {
                DEBUG_PARSER("(dvparser) reached dif %02x manufacturer specific data, parsing is done.\n", dif);
                datalen = std::distance(data,data_end);
                string value = bin2hex(data+1, data_end, datalen-1);
                t->mfct_0f_index = 1+std::distance(data_start, data);
                assert(t->mfct_0f_index >= 0);
                t->addExplanationAndIncrementPos(data, datalen, KindOfData::CONTENT, Understanding::NONE, "%02X manufacturer specific data %s", dif, value.c_str());
                break;
            }
            DEBUG_PARSER("(dvparser) reached unknown dif %02x treating remaining data as manufacturer specific, parsing is done.\n", dif);
            datalen = std::distance(data,data_end);
            string value = bin2hex(data+1, data_end, datalen-1);
            t->mfct_0f_index = 1+std::distance(data_start, data);
            assert(t->mfct_0f_index >= 0);
            t->addExplanationAndIncrementPos(data, datalen, KindOfData::CONTENT, Understanding::NONE, "%02X unknown dif treating remaining data as mfct specific %s", dif, value.c_str());
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

        while (has_another_dife)
        {
            if (*format == format_end) { debug("(dvparser) warning: unexpected end of data (dife expected)\n"); break; }

            uchar dife = **format;
            int subunit_bit = (dife & 0x40) >> 6;
            subunit |= subunit_bit << difenr;
            int tariff_bits = (dife & 0x30) >> 4;
            tariff |= tariff_bits << (difenr*2);
            int storage_nr_bits = (dife & 0x0f);
            storage_nr |= storage_nr_bits << (1+difenr*4);

            DEBUG_PARSER("(dvparser debug) dife=%02x (subunit=%d tariff=%d storagenr=%d)\n", dife, subunit, tariff, storage_nr);

            if (data_has_difvifs)
            {
                format_bytes.push_back(dife);
                id_bytes.push_back(dife);
                t->addExplanationAndIncrementPos(*format, 1, KindOfData::PROTOCOL, Understanding::FULL,
                                                 "%02X dife (subunit=%d tariff=%d storagenr=%d)",
                                                 dife, subunit, tariff, storage_nr);
            }
            else
            {
                id_bytes.push_back(**format);
                (*format)++;
            }

            has_another_dife = (dife & 0x80) == 0x80;
            difenr++;
        }

        if (*format == format_end) { debug("(dvparser) warning: unexpected end of data (vif expected)\n"); break; }

        uchar vif = **format;
        int full_vif = vif & 0x7f;
        bool extension_vif = false;
        int combinable_full_vif = 0;
        bool combinable_extension_vif = false;
        set<VIFCombinable> found_combinable_vifs;
        set<uint16_t> found_combinable_vifs_raw;

        DEBUG_PARSER("(dvparser debug) vif=%04x \"%s\"\n", vif, vifType(vif).c_str());

        if (data_has_difvifs)
        {
            format_bytes.push_back(vif);
            id_bytes.push_back(vif);
            t->addExplanationAndIncrementPos(*format, 1, KindOfData::PROTOCOL, Understanding::FULL,
                                             "%02X vif (%s)", vif, vifType(vif).c_str());
        } else
        {
            id_bytes.push_back(**format);
            (*format)++;
        }

        // Check if this is marker for one of the extended sets of vifs: first, second and third or manufacturer.
        if (vif == 0xfb || vif == 0xfd || vif == 0xef || vif == 0xff)
        {
            // Extension vifs.
            full_vif <<= 8;
            extension_vif = true;
        }

        // Grabbing a variable length vif. This does not currently work
        // with the compact format.
        if (vif == 0x7c)
        {
            DEBUG_PARSER("(dvparser debug) variable length vif found\n");
            if (*format == format_end) { debug("(dvparser) warning: unexpected end of data (vif varlen expected)\n"); break; }
            uchar viflen = **format;
            id_bytes.push_back(viflen);
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

        // Do we have another vife byte? We better have one, if extension_vif is true.
        bool has_another_vife = (vif & 0x80) == 0x80;
        while (has_another_vife)
        {
            if (*format == format_end) { debug("(dvparser) warning: unexpected end of data (vife expected)\n"); break; }

            uchar vife = **format;
            DEBUG_PARSER("(dvparser debug) vife=%02x (%s)\n", vife, vifeType(dif, vif, vife).c_str());

            if (data_has_difvifs)
            {
                // Collect the difvifs to create signature for future use.
                format_bytes.push_back(vife);
                id_bytes.push_back(vife);
            }
            else
            {
                // Reuse the existing
                id_bytes.push_back(**format);
                (*format)++;
            }

            has_another_vife = (vife & 0x80) == 0x80;

            if (extension_vif)
            {
                // First vife after the extension marker is the real vif.
                full_vif |= (vife & 0x7f);
                extension_vif = false;
                if (data_has_difvifs)
                {
                    t->addExplanationAndIncrementPos(*format, 1, KindOfData::PROTOCOL, Understanding::FULL,
                                                     "%02X vife (%s)", vife, vifeType(dif, vif, vife).c_str());
                }
            }
            else
            {
                if (combinable_extension_vif)
                {
                    // First vife after the combinable extension marker is the real vif.
                    combinable_full_vif |= (vife & 0x7f);
                    combinable_extension_vif = false;
                    VIFCombinable vc = toVIFCombinable(combinable_full_vif);
                    found_combinable_vifs.insert(vc);
                    found_combinable_vifs_raw.insert(combinable_full_vif);

                    if (data_has_difvifs)
                    {
                        t->addExplanationAndIncrementPos(*format, 1, KindOfData::PROTOCOL, Understanding::FULL,
                                                         "%02X combinable extension vife", vife);
                    }
                }
                else
                {
                    combinable_full_vif = vife & 0x7f;
                    // Check if this is marker for one of the extended combinable vifs
                    if (combinable_full_vif == 0x7c || combinable_full_vif == 0x7f)
                    {
                        combinable_full_vif <<= 8;
                        combinable_extension_vif = true;
                        VIFCombinable vc = toVIFCombinable(vife & 0x7f);
                        if (data_has_difvifs)
                        {
                            t->addExplanationAndIncrementPos(*format, 1, KindOfData::PROTOCOL, Understanding::FULL,
                                                             "%02X combinable vif (%s)", vife, toString(vc));
                        }
                    }
                    else
                    {
                        VIFCombinable vc = toVIFCombinable(combinable_full_vif);
                        found_combinable_vifs.insert(vc);
                        found_combinable_vifs_raw.insert(combinable_full_vif);

                        if (data_has_difvifs)
                        {
                            t->addExplanationAndIncrementPos(*format, 1, KindOfData::PROTOCOL, Understanding::FULL,
                                                             "%02X combinable vif (%s)", vife, toString(vc));
                        }
                    }
                }
            }
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
            strprintf(&key, "%s_%d", dv.c_str(), count);
        } else {
            strprintf(&key, "%s", dv.c_str());
        }
        DEBUG_PARSER("(dvparser debug) DifVif key is %s\n", key.c_str());

        int remaining = std::distance(data, data_end);
        if (remaining < 1)
        {
            debug("(dvparser) warning: unexpected end of data\n");
            break;
        }

        if (variable_length) {
            DEBUG_PARSER("(dvparser debug) varlen %02x\n", *(data+0));
            datalen = *(data);
            t->addExplanationAndIncrementPos(data, 1, KindOfData::PROTOCOL, Understanding::FULL, "%02X varlen=%d", *(data+0), datalen);
            remaining--; // Drop the length byte.
        }
        DEBUG_PARSER("(dvparser debug) remaining data %d len=%d\n", remaining, datalen);
        if (remaining < datalen)
        {
            debug("(dvparser) warning: unexpected end of data\n");
            datalen = remaining-1;
        }

        string value = bin2hex(data, data_end, datalen);
        int offset = start_parse_here+data-data_start;

        (*dv_entries)[key] = { offset, DVEntry(offset,
                                               key,
                                               mt,
                                               Vif(full_vif),
                                               found_combinable_vifs,
                                               found_combinable_vifs_raw,
                                               StorageNr(storage_nr),
                                               TariffNr(tariff),
                                               SubUnitNr(subunit),
                                               value) };

        DVEntry *dve = &(*dv_entries)[key].second;

        if (isTraceEnabled())
        {
            trace("[DVPARSER] entry %s\n", dve->str().c_str());
        }

        assert(key == dve->dif_vif_key.str());

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
    uint16_t hash = crc16_EN13757(safeButUnsafeVectorPtr(format_bytes), format_bytes.size());

    if (data_has_difvifs) {
        if (hash_to_format_.count(hash) == 0) {
            hash_to_format_[hash] = format_string;
            debug("(dvparser) found new format \"%s\" with hash %x, remembering!\n", format_string.c_str(), hash);
        }
    }

    return true;
}

bool hasKey(std::map<std::string,std::pair<int,DVEntry>> *dv_entries, std::string key)
{
    return dv_entries->count(key) > 0;
}

bool findKey(MeasurementType mit, VIFRange vif_range, StorageNr storagenr, TariffNr tariffnr,
             std::string *key, std::map<std::string,std::pair<int,DVEntry>> *dv_entries)
{
    return findKeyWithNr(mit, vif_range, storagenr, tariffnr, 1, key, dv_entries);
}

bool findKeyWithNr(MeasurementType mit, VIFRange vif_range, StorageNr storagenr, TariffNr tariffnr, int nr,
                   std::string *key, std::map<std::string,std::pair<int,DVEntry>> *dv_entries)
{
    /*debug("(dvparser) looking for type=%s vifrange=%s storagenr=%d tariffnr=%d\n",
      measurementTypeName(mit).c_str(), toString(vif_range), storagenr.intValue(), tariffnr.intValue());*/

    for (auto& v : *dv_entries)
    {
        MeasurementType ty = v.second.second.measurement_type;
        Vif vi = v.second.second.vif;
        StorageNr sn = v.second.second.storage_nr;
        TariffNr tn = v.second.second.tariff_nr;

        /* debug("(dvparser) match? %s type=%s vife=%x (%s) and storagenr=%d\n",
              v.first.c_str(),
              measurementTypeName(ty).c_str(), vi.intValue(), storagenr, sn);*/

        if (isInsideVIFRange(vi, vif_range) &&
            (mit == MeasurementType::Instantaneous || mit == ty) &&
            (storagenr == AnyStorageNr || storagenr == sn) &&
            (tariffnr == AnyTariffNr || tariffnr == tn))
        {
            *key = v.first;
            nr--;
            if (nr <= 0) return true;
            debug("(dvparser) found key %s for type=%s vif=%x storagenr=%d\n",
                  v.first.c_str(), measurementTypeName(ty).c_str(),
                  vi.intValue(), storagenr.intValue());
        }
    }
    return false;
}

void extractDV(DifVifKey &dvk, uchar *dif, int *vif, bool *has_difes, bool *has_vifes)
{
    string tmp = dvk.str();
    extractDV(tmp, dif, vif, has_difes, has_vifes);
}

void extractDV(string &s, uchar *dif, int *vif, bool *has_difes, bool *has_vifes)
{
    vector<uchar> bytes;
    hex2bin(s, &bytes);
    size_t i = 0;
    *has_difes = false;
    *has_vifes = false;
    if (bytes.size() == 0)
    {
        *dif = 0;
        *vif = 0;
        return;
    }

    *dif = bytes[i];
    while (i < bytes.size() && (bytes[i] & 0x80))
    {
        i++;
        *has_difes = true;
    }
    i++;

    if (i >= bytes.size())
    {
        *vif = 0;
        return;
    }

    *vif = bytes[i];
    if (*vif == 0xfb || // first extension
        *vif == 0xfd || // second extensio
        *vif == 0xef || // third extension
        *vif == 0xff)   // vendor extension
    {
        if (i+1 < bytes.size())
        {
            // Create an extended vif, like 0xfd31 for example.
            *vif = bytes[i] << 8 | bytes[i+1];
            i++;
        }
    }

    while (i < bytes.size() && (bytes[i] & 0x80))
    {
        i++;
        *has_vifes = true;
    }
}

bool extractDVuint8(map<string,pair<int,DVEntry>> *dv_entries,
                    string key,
                    int *offset,
                    uchar *value)
{
    if ((*dv_entries).count(key) == 0) {
        verbose("(dvparser) warning: cannot extract uint8 from non-existant key \"%s\"\n", key.c_str());
        *offset = -1;
        *value = 0;
        return false;
    }

    pair<int,DVEntry>&  p = (*dv_entries)[key];
    *offset = p.first;
    vector<uchar> v;
    hex2bin(p.second.value, &v);

    *value = v[0];
    return true;
}

bool extractDVuint16(map<string,pair<int,DVEntry>> *dv_entries,
                     string key,
                     int *offset,
                     uint16_t *value)
{
    if ((*dv_entries).count(key) == 0) {
        verbose("(dvparser) warning: cannot extract uint16 from non-existant key \"%s\"\n", key.c_str());
        *offset = -1;
        *value = 0;
        return false;
    }

    pair<int,DVEntry>&  p = (*dv_entries)[key];
    *offset = p.first;
    vector<uchar> v;
    hex2bin(p.second.value, &v);

    *value = v[1]<<8 | v[0];
    return true;
}

bool extractDVuint24(map<string,pair<int,DVEntry>> *dv_entries,
                     string key,
                     int *offset,
                     uint32_t *value)
{
    if ((*dv_entries).count(key) == 0) {
        verbose("(dvparser) warning: cannot extract uint24 from non-existant key \"%s\"\n", key.c_str());
        *offset = -1;
        *value = 0;
        return false;
    }

    pair<int,DVEntry>&  p = (*dv_entries)[key];
    *offset = p.first;
    vector<uchar> v;
    hex2bin(p.second.value, &v);

    *value = v[2] << 16 | v[1]<<8 | v[0];
    return true;
}

bool extractDVuint32(map<string,pair<int,DVEntry>> *dv_entries,
                     string key,
                     int *offset,
                     uint32_t *value)
{
    if ((*dv_entries).count(key) == 0) {
        verbose("(dvparser) warning: cannot extract uint32 from non-existant key \"%s\"\n", key.c_str());
        *offset = -1;
        *value = 0;
        return false;
    }

    pair<int,DVEntry>&  p = (*dv_entries)[key];
    *offset = p.first;
    vector<uchar> v;
    hex2bin(p.second.value, &v);

    *value = (uint32_t(v[3]) << 24) |  (uint32_t(v[2]) << 16) | (uint32_t(v[1])<<8) | uint32_t(v[0]);
    return true;
}

bool extractDVdouble(map<string,pair<int,DVEntry>> *dv_entries,
                     string key,
                     int *offset,
                     double *value,
                     bool auto_scale,
                     bool assume_signed)
{
    if ((*dv_entries).count(key) == 0) {
        verbose("(dvparser) warning: cannot extract double from non-existant key \"%s\"\n", key.c_str());
        *offset = 0;
        *value = 0;
        return false;
    }
    pair<int,DVEntry>&  p = (*dv_entries)[key];
    *offset = p.first;

    if (p.second.value.length() == 0) {
        verbose("(dvparser) warning: key found but no data  \"%s\"\n", key.c_str());
        *offset = 0;
        *value = 0;
        return false;
    }

    return p.second.extractDouble(value, auto_scale, assume_signed);
}

bool checkSizeHex(size_t expected_len, DifVifKey &dvk, string &v)
{
    if (v.length() == expected_len) return true;

    warning("(dvparser) bad decode since difvif %s expected %d hex chars but got \"%s\"\n",
            dvk.str().c_str(), expected_len, v.c_str());
    return false;
}

bool DVEntry::extractDouble(double *out, bool auto_scale, bool assume_signed)
{
    int t = dif_vif_key.dif() & 0xf;
    if (t == 0x0 ||
        t == 0x8 ||
        t == 0xd ||
        t == 0xf)
    {
        // Cannot extract from nothing, selection for readout, variable length or special.
        // Variable length is used for compact varlen history. Should be added in the future.
        return false;
    }
    else
    if (t == 0x1 || // 8 Bit Integer/Binary
        t == 0x2 || // 16 Bit Integer/Binary
        t == 0x3 || // 24 Bit Integer/Binary
        t == 0x4 || // 32 Bit Integer/Binary
        t == 0x6 || // 48 Bit Integer/Binary
        t == 0x7)   // 64 Bit Integer/Binary
    {
        vector<uchar> v;
        hex2bin(value, &v);
        uint64_t raw = 0;
        bool negate = false;
        uint64_t negate_mask = 0;
        if (t == 0x1) {
            if (!checkSizeHex(2, dif_vif_key, value)) return false;
            assert(v.size() == 1);
            raw = v[0];
            if (assume_signed && (raw & (uint64_t)0x80UL) != 0) { negate = true; negate_mask = ~((uint64_t)0)<<8; }
        } else if (t == 0x2) {
            if (!checkSizeHex(4, dif_vif_key, value)) return false;
            assert(v.size() == 2);
            raw = v[1]*256 + v[0];
            if (assume_signed && (raw & (uint64_t)0x8000UL) != 0) { negate = true; negate_mask = ~((uint64_t)0)<<16; }
        } else if (t == 0x3) {
            if (!checkSizeHex(6, dif_vif_key, value)) return false;
            assert(v.size() == 3);
            raw = v[2]*256*256 + v[1]*256 + v[0];
            if (assume_signed && (raw & (uint64_t)0x800000UL) != 0) { negate = true; negate_mask = ~((uint64_t)0)<<24; }
        } else if (t == 0x4) {
            if (!checkSizeHex(8, dif_vif_key, value)) return false;
            assert(v.size() == 4);
            raw = ((unsigned int)v[3])*256*256*256
                + ((unsigned int)v[2])*256*256
                + ((unsigned int)v[1])*256
                + ((unsigned int)v[0]);
            if (assume_signed && (raw & (uint64_t)0x80000000UL) != 0) { negate = true; negate_mask = ~((uint64_t)0)<<32; }
        } else if (t == 0x6) {
            if (!checkSizeHex(12, dif_vif_key, value)) return false;
            assert(v.size() == 6);
            raw = ((uint64_t)v[5])*256*256*256*256*256
                + ((uint64_t)v[4])*256*256*256*256
                + ((uint64_t)v[3])*256*256*256
                + ((uint64_t)v[2])*256*256
                + ((uint64_t)v[1])*256
                + ((uint64_t)v[0]);
            if (assume_signed && (raw & (uint64_t)0x800000000000UL) != 0) { negate = true; negate_mask = ~((uint64_t)0)<<48; }
        } else if (t == 0x7) {
            if (!checkSizeHex(16, dif_vif_key, value)) return false;
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
        if (auto_scale) scale = vifScale(dif_vif_key.vif());
        *out = (draw) / scale;
    }
    else
    if (t == 0x9 || // 2 digit BCD
        t == 0xA || // 4 digit BCD
        t == 0xB || // 6 digit BCD
        t == 0xC || // 8 digit BCD
        t == 0xE)   // 12 digit BCD
    {
        // 74140000 -> 00001474
        string& v = value;
        uint64_t raw = 0;
        bool negate = false;
        if (t == 0x9) {
            if (!checkSizeHex(2, dif_vif_key, v)) return false;
            if (assume_signed && v[0] == 'F') { negate = true; v[0] = '0'; }
            raw = (v[0]-'0')*10 + (v[1]-'0');
        } else if (t ==  0xA) {
            if (!checkSizeHex(4, dif_vif_key, v)) return false;
            if (assume_signed && v[2] == 'F') { negate = true; v[2] = '0'; }
            raw = (v[2]-'0')*10*10*10 + (v[3]-'0')*10*10
                + (v[0]-'0')*10 + (v[1]-'0');
        } else if (t ==  0xB) {
            if (!checkSizeHex(6, dif_vif_key, v)) return false;
            if (assume_signed && v[4] == 'F') { negate = true; v[4] = '0'; }
            raw = (v[4]-'0')*10*10*10*10*10 + (v[5]-'0')*10*10*10*10
                + (v[2]-'0')*10*10*10 + (v[3]-'0')*10*10
                + (v[0]-'0')*10 + (v[1]-'0');
        } else if (t ==  0xC) {
            if (!checkSizeHex(8, dif_vif_key, v)) return false;
            if (assume_signed && v[6] == 'F') { negate = true; v[6] = '0'; }
            raw = (v[6]-'0')*10*10*10*10*10*10*10 + (v[7]-'0')*10*10*10*10*10*10
                + (v[4]-'0')*10*10*10*10*10 + (v[5]-'0')*10*10*10*10
                + (v[2]-'0')*10*10*10 + (v[3]-'0')*10*10
                + (v[0]-'0')*10 + (v[1]-'0');
        } else if (t ==  0xE) {
            if (!checkSizeHex(12, dif_vif_key, v)) return false;
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
        if (auto_scale) scale = vifScale(dif_vif_key.vif());
        *out = (draw) / scale;
    }
    else
    if (t == 0x5) // 32 Bit Real
    {
        vector<uchar> v;
        hex2bin(value, &v);
        if (!checkSizeHex(8, dif_vif_key, value)) return false;
        assert(v.size() == 4);
        RealConversion rc;
        rc.i = v[3]<<24 | v[2]<<16 | v[1]<<8 | v[0];

        // Assumes float uses the standard IEEE 754 bit set.
        // 1 bit sign,  8 bit exp, 23 bit mantissa
        // RealConversion is tested on an amd64 platform. How about
        // other platsforms with different byte ordering?
        double draw = rc.f;
        double scale = 1.0;
        if (auto_scale) scale = vifScale(dif_vif_key.vif());
        *out = (draw) / scale;
    }
    else
    {
        warning("(dvparser) Unsupported dif format for extraction to double! dif=%02x\n", dif_vif_key.dif());
        return false;
    }

    return true;
}

bool extractDVlong(map<string,pair<int,DVEntry>> *dv_entries,
                   string key,
                   int *offset,
                   uint64_t *out)
{
    if ((*dv_entries).count(key) == 0) {
        verbose("(dvparser) warning: cannot extract long from non-existant key \"%s\"\n", key.c_str());
        *offset = 0;
        *out = 0;
        return false;
    }

    pair<int,DVEntry>&  p = (*dv_entries)[key];
    *offset = p.first;

    if (p.second.value.length() == 0) {
        verbose("(dvparser) warning: key found but no data  \"%s\"\n", key.c_str());
        *offset = 0;
        *out = 0;
        return false;
    }

    return p.second.extractLong(out);
}

bool DVEntry::extractLong(uint64_t *out)
{
    int t = dif_vif_key.dif() & 0xf;
    if (t == 0x1 || // 8 Bit Integer/Binary
        t == 0x2 || // 16 Bit Integer/Binary
        t == 0x3 || // 24 Bit Integer/Binary
        t == 0x4 || // 32 Bit Integer/Binary
        t == 0x6 || // 48 Bit Integer/Binary
        t == 0x7)   // 64 Bit Integer/Binary
    {
        vector<uchar> v;
        hex2bin(value, &v);
        uint64_t raw = 0;
        if (t == 0x1) {
            if (!checkSizeHex(2, dif_vif_key, value)) return false;
            assert(v.size() == 1);
            raw = v[0];
        } else if (t == 0x2) {
            if (!checkSizeHex(4, dif_vif_key, value)) return false;
            assert(v.size() == 2);
            raw = v[1]*256 + v[0];
        } else if (t == 0x3) {
            if (!checkSizeHex(6, dif_vif_key, value)) return false;
            assert(v.size() == 3);
            raw = v[2]*256*256 + v[1]*256 + v[0];
        } else if (t == 0x4) {
            if (!checkSizeHex(8, dif_vif_key, value)) return false;
            assert(v.size() == 4);
            raw = ((unsigned int)v[3])*256*256*256
                + ((unsigned int)v[2])*256*256
                + ((unsigned int)v[1])*256
                + ((unsigned int)v[0]);
        } else if (t == 0x6) {
            if (!checkSizeHex(12, dif_vif_key, value)) return false;
            assert(v.size() == 6);
            raw = ((uint64_t)v[5])*256*256*256*256*256
                + ((uint64_t)v[4])*256*256*256*256
                + ((uint64_t)v[3])*256*256*256
                + ((uint64_t)v[2])*256*256
                + ((uint64_t)v[1])*256
                + ((uint64_t)v[0]);
        } else if (t == 0x7) {
            if (!checkSizeHex(16, dif_vif_key, value)) return false;
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
        *out = raw;
    }
    else
    if (t == 0x9 || // 2 digit BCD
        t == 0xA || // 4 digit BCD
        t == 0xB || // 6 digit BCD
        t == 0xC || // 8 digit BCD
        t == 0xE)   // 12 digit BCD
    {
        // 74140000 -> 00001474
        string& v = value;
        uint64_t raw = 0;
        if (t == 0x9) {
            if (!checkSizeHex(2, dif_vif_key, value)) return false;
            assert(v.size() == 2);
            raw = (v[0]-'0')*10 + (v[1]-'0');
        } else if (t ==  0xA) {
            if (!checkSizeHex(4, dif_vif_key, value)) return false;
            assert(v.size() == 4);
            raw = (v[2]-'0')*10*10*10 + (v[3]-'0')*10*10
                + (v[0]-'0')*10 + (v[1]-'0');
        } else if (t ==  0xB) {
            if (!checkSizeHex(6, dif_vif_key, value)) return false;
            assert(v.size() == 6);
            raw = (v[4]-'0')*10*10*10*10*10 + (v[5]-'0')*10*10*10*10
                + (v[2]-'0')*10*10*10 + (v[3]-'0')*10*10
                + (v[0]-'0')*10 + (v[1]-'0');
        } else if (t ==  0xC) {
            if (!checkSizeHex(8, dif_vif_key, value)) return false;
            assert(v.size() == 8);
            raw = (v[6]-'0')*10*10*10*10*10*10*10 + (v[7]-'0')*10*10*10*10*10*10
                + (v[4]-'0')*10*10*10*10*10 + (v[5]-'0')*10*10*10*10
                + (v[2]-'0')*10*10*10 + (v[3]-'0')*10*10
                + (v[0]-'0')*10 + (v[1]-'0');
        } else if (t ==  0xE) {
            if (!checkSizeHex(12, dif_vif_key, value)) return false;
            assert(v.size() == 12);
            raw =(v[10]-'0')*10*10*10*10*10*10*10*10*10*10*10 + (v[11]-'0')*10*10*10*10*10*10*10*10*10*10
                + (v[8]-'0')*10*10*10*10*10*10*10*10*10 + (v[9]-'0')*10*10*10*10*10*10*10*10
                + (v[6]-'0')*10*10*10*10*10*10*10 + (v[7]-'0')*10*10*10*10*10*10
                + (v[4]-'0')*10*10*10*10*10 + (v[5]-'0')*10*10*10*10
                + (v[2]-'0')*10*10*10 + (v[3]-'0')*10*10
                + (v[0]-'0')*10 + (v[1]-'0');
        }

        *out = raw;
    }
    else
    {
        error("Unsupported dif format for extraction to long! dif=%02x\n", dif_vif_key.dif());
    }

    return true;
}

bool extractDVHexString(map<string,pair<int,DVEntry>> *dv_entries,
                        string key,
                        int *offset,
                        string *value)
{
    if ((*dv_entries).count(key) == 0) {
        verbose("(dvparser) warning: cannot extract string from non-existant key \"%s\"\n", key.c_str());
        *offset = -1;
        return false;
    }
    pair<int,DVEntry>&  p = (*dv_entries)[key];
    *offset = p.first;
    *value = p.second.value;

    return true;
}


bool extractDVReadableString(map<string,pair<int,DVEntry>> *dv_entries,
                             string key,
                             int *offset,
                             string *out)
{
    if ((*dv_entries).count(key) == 0) {
        verbose("(dvparser) warning: cannot extract string from non-existant key \"%s\"\n", key.c_str());
        *offset = -1;
        return false;
    }
    pair<int,DVEntry>&  p = (*dv_entries)[key];
    *offset = p.first;

    return p.second.extractReadableString(out);
}

bool DVEntry::extractReadableString(string *out)
{
    int t = dif_vif_key.dif() & 0xf;

    string v = value;

    if (t == 0x1 || // 8 Bit Integer/Binary
        t == 0x2 || // 16 Bit Integer/Binary
        t == 0x3 || // 24 Bit Integer/Binary
        t == 0x4 || // 32 Bit Integer/Binary
        t == 0x6 || // 48 Bit Integer/Binary
        t == 0x7 || // 64 Bit Integer/Binary
        t == 0xD)   // Variable length
    {
        if (isLikelyAscii(v))
        {
            // For example an enhanced id 32 bits binary looks like:
            // 44434241 and will be reversed to: 41424344 and translated using ascii
            // to ABCD
            v = reverseBinaryAsciiSafeToString(v);
        }
        else
        {
            v = reverseBCD(v);
        }
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

    *out = v;
    return true;
}

double DVEntry::getCounter(DVEntryCounterType ct)
{
    switch (ct)
    {
    case DVEntryCounterType::STORAGE_COUNTER: return storage_nr.intValue();
    case DVEntryCounterType::TARIFF_COUNTER: return tariff_nr.intValue();
    case DVEntryCounterType::SUBUNIT_COUNTER: return subunit_nr.intValue();
    case DVEntryCounterType::UNKNOWN: break;
    }

    return std::numeric_limits<double>::quiet_NaN();
}

string DVEntry::str()
{
    string s =
        tostrprintf("%d: %s %s vif=%x %s%s st=%d ta=%d su=%d",
                    offset,
                    dif_vif_key.str().c_str(),
                    toString(measurement_type),
                    vif.intValue(),
                    combinable_vifs.size() > 0 ? "HASCOMB ":"",
                    combinable_vifs_raw.size() > 0 ? "HASCOMBRAW ":"",
                    storage_nr.intValue(),
                    tariff_nr.intValue(),
                    subunit_nr.intValue()
            );

    return s;
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

bool extractDVdate(map<string,pair<int,DVEntry>> *dv_entries,
                   string key,
                   int *offset,
                   struct tm *out)
{
    if ((*dv_entries).count(key) == 0)
    {
        verbose("(dvparser) warning: cannot extract date from non-existant key \"%s\"\n", key.c_str());
        *offset = -1;
        memset(out, 0, sizeof(struct tm));
        return false;
    }
    pair<int,DVEntry>&  p = (*dv_entries)[key];
    *offset = p.first;

    return p.second.extractDate(out);
}

bool DVEntry::extractDate(struct tm *out)
{
    memset(out, 0, sizeof(*out));
    out->tm_isdst = -1; // Figure out the dst automatically!

    vector<uchar> v;
    hex2bin(value, &v);

    bool ok = true;
    if (v.size() == 2) {
        ok &= ::extractDate(v[1], v[0], out);
    }
    else if (v.size() == 4) {
        ok &= ::extractDate(v[3], v[2], out);
        ok &= ::extractTime(v[1], v[0], out);
    }
    else if (v.size() == 6) {
        ok &= ::extractDate(v[4], v[3], out);
        ok &= ::extractTime(v[2], v[1], out);
        // ..ss ssss
        int sec  = (0x3f) & v[0];
        out->tm_sec = sec;
        // There are also bits for day of week, week of year.
        // A bit for if daylight saving is in use or not and its offset.
        // A bit if it is a leap year.
        // I am unsure how to deal with this here..... TODO
    }

    return ok;
}

bool FieldMatcher::matches(DVEntry &dv_entry)
{
    if (!active) return false;

    // Test an explicit dif vif key.
    if (match_dif_vif_key)
    {
        bool b = dv_entry.dif_vif_key == dif_vif_key;
        return b;
    }

    // Test ranges and types.
    bool b =
        (!match_vif_range || isInsideVIFRange(dv_entry.vif, vif_range)) &&
        (!match_vif_raw || dv_entry.vif == vif_raw) &&
        (!match_measurement_type || dv_entry.measurement_type == measurement_type) &&
        (!match_storage_nr || (dv_entry.storage_nr >= storage_nr_from && dv_entry.storage_nr <= storage_nr_to)) &&
        (!match_tariff_nr || (dv_entry.tariff_nr >= tariff_nr_from && dv_entry.tariff_nr <= tariff_nr_to)) &&
        (!match_subunit_nr || (dv_entry.subunit_nr >= subunit_nr_from && dv_entry.subunit_nr <= subunit_nr_to));

    if (!b) return false;

    // So far so good, now test the combinables.

    // If field matcher has no combinables, then do NOT match any dventry with a combinable!
    if (vif_combinables.size()== 0 && vif_combinables_raw.size() == 0)
    {
        // If there is a combinable vif, then there is a raw combinable vif. So comparing both not strictly necessary.
        if (dv_entry.combinable_vifs.size() == 0 && dv_entry.combinable_vifs_raw.size() == 0) return true;
        // Oups, field matcher does not expect any combinables, but the dv_entry has combinables.
        // This means no match for us since combinables must be handled explicitly.
        return false;
    }

    // Lets check that the dv_entry vif combinables raw contains the field matcher requested combinable raws.
    // The raws are used for meters using reserved and manufacturer specific vif combinables.
    for (uint16_t vcr : vif_combinables_raw)
    {
        if (dv_entry.combinable_vifs_raw.count(vcr) == 0)
        {
            // Ouch, one of the requested vif combinables raw did not exist in the dv_entry. No match!
            return false;
        }
    }

    // Lets check that the dv_entry combinables contains the field matcher requested combinables.
    // The named vif combinables are used by well behaved meters.
    for (VIFCombinable vc : vif_combinables)
    {
        if (vc != VIFCombinable::Any && dv_entry.combinable_vifs.count(vc) == 0)
        {
            // Ouch, one of the requested combinables did not exist in the dv_entry. No match!
            return false;
        }
    }

    // Now if we have not selected the Any combinable match pattern,
    // then we need to check if there are unmatched combinables in the telegram, if so fail the match.
    if (vif_combinables.count(VIFCombinable::Any) == 0)
    {
        if (vif_combinables.size() > 0)
        {
            for (VIFCombinable vc : dv_entry.combinable_vifs)
            {
                if (vif_combinables.count(vc) == 0)
                {
                    // Oups, the telegram entry had a vif combinable that we had no matcher for.
                    return false;
                }
            }
        }
        else
        {
            for (uint16_t vcr : dv_entry.combinable_vifs_raw)
            {
                if (vif_combinables_raw.count(vcr) == 0)
                {
                    // Oups, the telegram entry had a vif combinable raw that we had no matcher for.
                    return false;
                }
            }
        }
    }

    // Yay, they were all found.
    return true;
}

const char *toString(MeasurementType mt)
{
    switch (mt)
    {
    case MeasurementType::Any: return "Any";
    case MeasurementType::Instantaneous: return "Instantaneous";
    case MeasurementType::Minimum: return "Minimum";
    case MeasurementType::Maximum: return "Maximum";
    case MeasurementType::AtError: return "AtError";
    case MeasurementType::Unknown: return "Unknown";
    }
    return "?";
}

MeasurementType toMeasurementType(const char *s)
{
    if (!strcmp(s, "Any")) return MeasurementType::Any;
    if (!strcmp(s, "Instantaneous")) return MeasurementType::Instantaneous;
    if (!strcmp(s, "Minimum")) return MeasurementType::Minimum;
    if (!strcmp(s, "Maximum")) return MeasurementType::Maximum;
    if (!strcmp(s, "AtError")) return MeasurementType::AtError;
    if (!strcmp(s, "Unknown")) return MeasurementType::Unknown;

    return MeasurementType::Unknown;
}

string FieldMatcher::str()
{
    string s = "";

    if (match_dif_vif_key)
    {
        s = s+"DVK("+dif_vif_key.str()+") ";
    }

    if (match_measurement_type)
    {
        s = s+"MT("+toString(measurement_type)+") ";
    }

    if (match_vif_range)
    {
        s = s+"VR("+toString(vif_range)+") ";
    }

    if (match_vif_raw)
    {
        s = s+"VRR("+to_string(vif_raw)+") ";
    }

    if (vif_combinables.size() > 0)
    {
        s += "Comb(";

        for (auto vc : vif_combinables)
        {
            s = s+toString(vc)+" ";
        }

        s.pop_back();
        s += ")";
    }

    if (match_storage_nr)
    {
        s = s+"S("+to_string(storage_nr_from.intValue())+"-"+to_string(storage_nr_to.intValue())+") ";
    }

    if (match_tariff_nr)
    {
        s = s+"T("+to_string(tariff_nr_from.intValue())+"-"+to_string(tariff_nr_to.intValue())+") ";
    }

    if (match_subunit_nr)
    {
        s += "U("+to_string(subunit_nr_from.intValue())+"-"+to_string(subunit_nr_to.intValue())+") ";
    }

    if (index_nr.intValue() != 1)
    {
        s += "I("+to_string(index_nr.intValue())+")";
    }

    if (s.size() > 0)
    {
        s.pop_back();
    }

    return s;
}

DVEntryCounterType toDVEntryCounterType(const std::string &s)
{
    if (s == "storage_counter") return DVEntryCounterType::STORAGE_COUNTER;
    if (s == "tariff_counter") return DVEntryCounterType::TARIFF_COUNTER;
    if (s == "subunit_counter") return DVEntryCounterType::SUBUNIT_COUNTER;
    return DVEntryCounterType::UNKNOWN;
}

const char *toString(DVEntryCounterType ct)
{
    switch (ct)
    {
    case DVEntryCounterType::UNKNOWN: return "unknown";
    case DVEntryCounterType::STORAGE_COUNTER: return "storage_counter";
    case DVEntryCounterType::TARIFF_COUNTER: return "tariff_counter";
    case DVEntryCounterType::SUBUNIT_COUNTER: return "subunit_counter";
    }

    return "unknown";
}
