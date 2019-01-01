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

#include<assert.h>
#include<memory.h>

// The parser should not crash on invalid data, but yeah, when I
// need to debug it because it crashes on invalid data, then
// I enable the following define...
//#define DEBUG_PARSER(...) fprintf(stdout, __VA_ARGS__)
#define DEBUG_PARSER(...)

using namespace std;

bool parseDV(Telegram *t,
             vector<uchar> &databytes,
             vector<uchar>::iterator data,
             size_t data_len,
             map<string,pair<int,string>> *values,
             vector<uchar>::iterator *format,
             size_t format_len,
             uint16_t *format_hash,
             function<int(int,int,int)> overrideDifLen)
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
        string s = bin2hex(*format, format_len);
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
    // data bytes in a map. The same identifier can occur several times in a telegram,
    // even though it often don't. Since the first occurence is stored under 02FF20,
    // the second identical identifier stores its data under the key "02FF20_2" etc for 3 and forth...

    format_bytes.clear();
    id_bytes.clear();
    for (;;)
    {
        id_bytes.clear();
        DEBUG_PARSER("(dvparser debug) Remaining format data %ju\n", std::distance(*format,format_end));
        if (*format == format_end) break;
        uchar dif = **format;

        int datalen = difLenBytes(dif);
        DEBUG_PARSER("(dvparser debug) dif=%02x datalen=%d \"%s\"\n", dif, datalen, difType(dif).c_str());
        if (datalen == -2) {
            debug("(dvparser) cannot handle dif %02X ignoring rest of telegram.\n\n", dif);
            break;
        }
        if (dif == 0x2f) {
            t->addExplanation(*format, 1, "%02X skip", dif);
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
            t->addExplanation(*format, 1, "%02X dif (%s)", dif, difType(dif).c_str());
        } else {
            id_bytes.push_back(**format);
            (*format)++;
        }

        bool has_another_dife = (dif & 0x80) == 0x80;
        while (has_another_dife) {
            if (*format == format_end) { debug("(dvparser) warning: unexpected end of data (dife expected)"); break; }
            uchar dife = **format;
            DEBUG_PARSER("(dvparser debug) dife=%02x (%s)\n", dife, "?");
            if (data_has_difvifs) {
                format_bytes.push_back(dife);
                id_bytes.push_back(dife);
                t->addExplanation(*format, 1, "%02X dife (%s)", dife, "?");
            } else {
                id_bytes.push_back(**format);
                (*format)++;
            }
            has_another_dife = (dife & 0x80) == 0x80;
        }

        if (*format == format_end) { debug("(dvparser) warning: unexpected end of data (vif expected)"); break; }

        uchar vif = **format;
        DEBUG_PARSER("(dvparser debug) vif=%02x \"%s\"\n", vif, vifType(vif).c_str());
        if (data_has_difvifs) {
            format_bytes.push_back(vif);
            id_bytes.push_back(vif);
            t->addExplanation(*format, 1, "%02X vif (%s)", vif, vifType(vif).c_str());
        } else {
            id_bytes.push_back(**format);
            (*format)++;
        }

        bool has_another_vife = (vif & 0x80) == 0x80;
        while (has_another_vife) {
            if (*format == format_end) { debug("(dvparser) warning: unexpected end of data (vife expected)"); break; }
            uchar vife = **format;
            DEBUG_PARSER("(dvparser debug) vife=%02x (%s)\n", vife, "?");
            if (data_has_difvifs) {
                format_bytes.push_back(vife);
                id_bytes.push_back(vife);
                t->addExplanation(*format, 1, "%02X vife (%s)", vife, vifeType(vif, vife).c_str());
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

        if (overrideDifLen) {
            int new_len = overrideDifLen(dif, vif, datalen);
            if (new_len != datalen) {
                DEBUG_PARSER("(dvparser debug) changing len %d to %d for dif=%02x vif=%02x\n", datalen, new_len, dif, vif);
                datalen = new_len;
            }
        }

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
            if (remaining > 2) {
                datalen = *(data);
            } else {
                datalen = remaining;
            }
        }
        DEBUG_PARSER("(dvparser debug) remaining data %d len=%d\n", remaining, datalen);
        if (remaining < datalen) {
            debug("(dvparser) warning: unexpected end of data\n");
            datalen = remaining;
        }

        // Skip the length byte in the variable length data.
        if (variable_length) {
            data++;
        }
        string value = bin2hex(data, datalen);
        (*values)[key] = { start_parse_here+data-data_start, value };
        if (value.length() > 0) {
            assert(data != databytes.end());
            assert(data+datalen <= databytes.end());
            // This call increments data with datalen.
            t->addExplanation(data, datalen, "%s", value.c_str());
            DEBUG_PARSER("(dvparser debug) data \"%s\"\n\n", value.c_str());
        }
        if (remaining == datalen) {
            // We are done here!
            break;
        }
    }

    string format_string = bin2hex(format_bytes);
    uint16_t hash = crc16_EN13757(&format_bytes[0], format_bytes.size());

    if (data_has_difvifs) {
        debug("(dvparser) found format \"%s\" with hash %x\n\n", format_string.c_str(), hash);
    }

    return true;
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

bool extractDVuint16(map<string,pair<int,string>> *values,
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

    pair<int,string>&  p = (*values)[key];
    *offset = p.first;
    vector<uchar> v;
    hex2bin(p.second, &v);

    *value = v[1]<<8 | v[0];
    return true;
}

bool extractDVdouble(map<string,pair<int,string>> *values,
                     string key,
                     int *offset,
                     double *value)
{
    if ((*values).count(key) == 0) {
        verbose("(dvparser) warning: cannot extract double from non-existant key \"%s\"\n", key.c_str());
        *offset = 0;
        *value = 0;
        return false;
    }
    uchar dif, vif;
    extractDV(key, &dif, &vif);

    pair<int,string>&  p = (*values)[key];
    *offset = p.first;

    if (p.second.length() == 0) {
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
        hex2bin(p.second, &v);
        unsigned int raw = 0;
        if (t == 0x1) {
            raw = v[0];
        } else if (t == 0x2) {
            raw = v[1]*256 + v[0];
        } else if (t == 0x3) {
            raw = v[2]*256*256 + v[1]*256 + v[0];
        } else if (t == 0x4) {
            raw = ((unsigned int)v[3])*256*256*256
                + ((unsigned int)v[2])*256*256
                + ((unsigned int)v[1])*256
                + ((unsigned int)v[0]);
        }
        double scale = vifScale(vif);
        *value = ((double)raw) / scale;
    }
    else
    if (t == 0x9 || // 2 digit BCD
        t == 0xA || // 4 digit BCD
        t == 0xB || // 6 digit BCD
        t == 0xC || // 8 digit BCD
        t == 0xE)   // 12 digit BCD
    {
        // 74140000 -> 00001474
        string& v = p.second;
        unsigned int raw = 0;
        if (t == 0x9) {
            raw = (v[0]-'0')*10 + (v[1]-'0');
        } else if (t ==  0xA) {
            raw = (v[2]-'0')*10*10*10 + (v[3]-'0')*10*10
                + (v[0]-'0')*10 + (v[1]-'0');
        } else if (t ==  0xB) {
            raw = (v[4]-'0')*10*10*10*10*10 + (v[5]-'0')*10*10*10*10
                + (v[2]-'0')*10*10*10 + (v[3]-'0')*10*10
                + (v[0]-'0')*10 + (v[1]-'0');
        } else if (t ==  0xC) {
            raw = (v[6]-'0')*10*10*10*10*10*10*10 + (v[7]-'0')*10*10*10*10*10*10
                + (v[4]-'0')*10*10*10*10*10 + (v[5]-'0')*10*10*10*10
                + (v[2]-'0')*10*10*10 + (v[3]-'0')*10*10
                + (v[0]-'0')*10 + (v[1]-'0');
        } else if (t ==  0xE) {
            raw =(v[10]-'0')*10*10*10*10*10*10*10*10*10*10*10 + (v[11]-'0')*10*10*10*10*10*10*10*10*10*10
                + (v[8]-'0')*10*10*10*10*10*10*10*10*10 + (v[9]-'0')*10*10*10*10*10*10*10*10
                + (v[6]-'0')*10*10*10*10*10*10*10 + (v[7]-'0')*10*10*10*10*10*10
                + (v[4]-'0')*10*10*10*10*10 + (v[5]-'0')*10*10*10*10
                + (v[2]-'0')*10*10*10 + (v[3]-'0')*10*10
                + (v[0]-'0')*10 + (v[1]-'0');
        }

        double scale = vifScale(vif);
        *value = ((double)raw) / scale;
    }
    else
    {
        error("Unsupported dif format for extraction to double! dif=%02x\n", dif);
    }

    return true;
}

bool extractDVdoubleCombined(std::map<std::string,std::pair<int,std::string>> *values,
                            std::string key_high_bits, // Which key to extract high bits from.
                            std::string key,
                            int *offset,
                            double *value)
{
    if ((*values).count(key) == 0 || (*values).count(key_high_bits) == 0) {
        verbose("(dvparser) warning: cannot extract combined double since at least one key is missing \"%s\" \"%s\"\n", key.c_str(),
                key_high_bits.c_str());
        *offset = 0;
        *value = 0;
        return false;
    }
    uchar dif, vif;
    extractDV(key, &dif, &vif);

    pair<int,string>&  p = (*values)[key];

    if (p.second.length() == 0) {
        verbose("(dvparser) warning: key found but no data  \"%s\"\n", key.c_str());
        *offset = 0;
        *value = 0;
        return false;
    }

    *offset = p.first;
    vector<uchar> v;
    hex2bin(p.second, &v);

    pair<int,string>&  pp = (*values)[key_high_bits];
    vector<uchar> v_high;
    hex2bin(pp.second, &v_high);

    int raw = v_high[3]*256*256*256 + v_high[2]*256*256 + v[1]*256 + v[0];
    double scale = vifScale(vif);
    *value = ((double)raw) / scale;

    return true;
}

bool extractDVstring(map<string,pair<int,string>> *values,
                     string key,
                     int *offset,
                     string *value)
{
    if ((*values).count(key) == 0) {
        verbose("(dvparser) warning: cannot extract string from non-existant key \"%s\"\n", key.c_str());
        *offset = -1;
        *value = "";
        return false;
    }
    pair<int,string>&  p = (*values)[key];
    *offset = p.first;
    *value = p.second;
    return true;
}

bool extractDVdate(map<string,pair<int,string>> *values,
                   string key,
                   int *offset,
                   time_t *value)
{
    if ((*values).count(key) == 0) {
        verbose("(dvparser) warning: cannot extract date from non-existant key \"%s\"\n", key.c_str());
        *offset = -1;
        *value = 0;
        return false;
    }
    uchar dif, vif;
    extractDV(key, &dif, &vif);

    pair<int,string>&  p = (*values)[key];
    *offset = p.first;
    vector<uchar> v;
    hex2bin(p.second, &v);

    *value = v[1]<<8 | v[0];

    int day = (0x1f) & v[0];
    int year1 = ((0xe0) & v[0]) >> 5;
    int month = (0x0f) & v[1];
    int year2 = ((0xf0) & v[1]) >> 1;
    int year = (2000 + year1 + year2);

    struct tm timestamp;
    memset(&timestamp, 0, sizeof(timestamp));
    timestamp.tm_mday = day;   /* Day of the month (1-31) */
    timestamp.tm_mon = month-1;    /* Month (0-11) */
    timestamp.tm_year = year -1900;   /* Year - 1900 */

    *value = mktime(&timestamp);
    return true;
}
