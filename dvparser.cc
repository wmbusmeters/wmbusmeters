/*
 Copyright (C) 2018 Fredrik Öhrström

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

using namespace std;

bool parseDV(Telegram *t,
             vector<uchar>::iterator data,
             size_t data_len,
             map<string,pair<int,string>> *values,
             vector<uchar>::iterator *format,
             size_t format_len,
             uint16_t *format_hash)
{
    map<string,int> dv_count;
    vector<uchar> format_bytes;
    vector<uchar> data_bytes;
    string dv, key;
    size_t parsed_len = t->parsed.size();
    vector<uchar>::iterator data_start = data;
    vector<uchar>::iterator data_end = data+data_len;
    vector<uchar>::iterator format_end;
    bool full_header = false;

    if (format == NULL) {
        format = &data;
        format_end = data_end;
        full_header = true;
    } else {
        format_end = *format+format_len;
        string s = bin2hex(*format, format_len);
        debug("(dvparser) using format \"%s\"\n", s.c_str());
    }

    // Data format is:
    // DIF byte (defines how the binary data bits should be decoded and how man data bytes there are)
    // (sometimes a DIFE byte, not yet implemented)
    // VIF byte (defines what the decoded value means, water,energy,power,etc.)
    // (sometimes a VIFE byte when VIF=0xff)
    // Data bytes
    // DIF again...

    // A DifVif(Vife) tuple (triplet) can be for example be 02FF20 for Multical21 vendor specific status bits.
    // The parser stores the data bytes under the key "02FF20" for the first occurence of the 02FF20 data.
    // The second identical tuple(triplet) stores its data under the key "02FF20_2"

    format_bytes.clear();
    for (;;) {
        if (*format == format_end) break;
        uchar dif = **format;
        int len = difLenBytes(dif);
        if (full_header) {
            format_bytes.push_back(dif);
            t->addExplanation(*format, 1, "%02X dif (%s)", dif, difType(dif).c_str());
        } else {
            (*format)++;
        }

        if (*format == format_end) { warning("(dvparser) warning: unexpected end of data (vif expected)"); break; }

        uchar vif = **format;
        if (full_header) {
            format_bytes.push_back(vif);
            t->addExplanation(*format, 1, "%02X vif (%s)", vif, vifType(vif).c_str());
        } else {
            (*format)++;
        }

        strprintf(dv, "%02X%02X", dif, vif);
        if ((vif&0x80) == 0x80) {
            // vif extension
            if (*format == format_end) { warning("(dvparser) warning: unexpected end of data (vife expected)"); break; }
            uchar vife = **format;
            if (full_header) {
                format_bytes.push_back(vife);
                t->addExplanation(*format, 1, "%02X vife (%s)", vife, vifeType(vif, vife).c_str());
            } else {
                (*format)++;
            }
            strprintf(dv, "%02X%02X%02X", dif, vif, vife);
        }

        int count = ++dv_count[key];
        if (count > 1) {
            strprintf(key, "%s_%d", dv.c_str(), count);
        } else {
            strprintf(key, "%s", dv.c_str());
        }

        string value = bin2hex(data, len);
        (*values)[key] = { parsed_len+data-data_start, value };
        t->addExplanation(data, len, "%s", value.c_str());
    }

    string format_string = bin2hex(format_bytes);
    uint16_t hash = crc16_EN13757(&format_bytes[0], format_bytes.size());

    if (full_header) {
        debug("(dvparser) found format \"%s\" with hash %x\n", format_string.c_str(), hash);
    }

    return true;
}

void extractDV(string &s, uchar *dif, uchar *vif, uchar *vife)
{
    vector<uchar> bytes;
    hex2bin(s, &bytes);

    *dif = bytes[0];
    *vif = bytes[1];
    if (bytes.size() > 2) {
        *vife = bytes[2];
    } else {
        *vife = 0;
    }
}

bool extractDVuint16(map<string,pair<int,string>> *values,
                     string key,
                     int *offset,
                     uint16_t *value)
{
    if ((*values).count(key) == 0) {
        warning("(dvparser) warning: cannot extract uint16 from non-existant key \"%s\"\n", key.c_str());
        *offset = -1;
        *value = 0;
        return false;
    }
    uchar dif, vif, vife;
    extractDV(key, &dif, &vif, &vife);

    pair<int,string>&  p = (*values)[key];
    *offset = p.first;
    vector<uchar> v;
    hex2bin(p.second, &v);

    *value = v[1]<<8 | v[0];
    return true;
}

bool extractDVfloat(map<string,pair<int,string>> *values,
                    string key,
                    int *offset,
                    float *value)
{
    if ((*values).count(key) == 0) {
        warning("(dvparser) warning: cannot extract float from non-existant key \"%s\"\n", key.c_str());
        *offset = 0;
        *value = 0;
        return false;
    }
    uchar dif, vif, vife;
    extractDV(key, &dif, &vif, &vife);

    pair<int,string>&  p = (*values)[key];
    *offset = p.first;
    vector<uchar> v;
    hex2bin(p.second, &v);

    int raw = v[3]*256*256*256 + v[2]*256*256 + v[1]*256 + v[0];
    float scale = vifScale(vif);
    *value = ((float)raw) / scale;

    return true;
}

bool extractDVfloatCombined(std::map<std::string,std::pair<int,std::string>> *values,
                            std::string key_high_bits, // Which key to extract high bits from.
                            std::string key,
                            int *offset,
                            float *value)
{
    if ((*values).count(key) == 0 || (*values).count(key_high_bits) == 0) {
        warning("(dvparser) warning: cannot extract combined float since at least one key is missing \"%s\" \"%s\"\n", key.c_str(),
                key_high_bits.c_str());
        *offset = 0;
        *value = 0;
        return false;
    }
    uchar dif, vif, vife;
    extractDV(key, &dif, &vif, &vife);

    pair<int,string>&  p = (*values)[key];
    *offset = p.first;
    vector<uchar> v;
    hex2bin(p.second, &v);

    pair<int,string>&  pp = (*values)[key];
    vector<uchar> v_high;
    hex2bin(pp.second, &v_high);

    int raw = v_high[3]*256*256*256 + v_high[2]*256*256 + v[1]*256 + v[0];
    float scale = vifScale(vif);
    *value = ((float)raw) / scale;

    return true;
}
