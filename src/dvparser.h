/*
 Copyright (C) 2018-2020 Fredrik Öhrström

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
#include"wmbus.h"

#include<map>
#include<stdint.h>
#include<time.h>
#include<functional>
#include<vector>

#define LIST_OF_VALUETYPES \
    X(Volume,0x10,0x17)       \
    X(VolumeFlow,0x38,0x3F) \
    X(FlowTemperature,0x58,0x5B) \
    X(ReturnTemperature,0x5D,0x5E) \
    X(ExternalTemperature,0x64,0x67) \
    X(HeatCostAllocation,0x6E,0x6E) \
    X(Date,0x6C,0x6C) \
    X(DateTime,0x6D,0x6D) \
    X(EnergyMJ,0x0e,0x0e) \
    X(EnergyWh,0x00,0x07) \
    X(PowerW,0x28,0x2f) \

enum class ValueInformation
{
    None,
#define X(name,from,to) name,
LIST_OF_VALUETYPES
#undef X
};

const char *toString(ValueInformation v);
ValueInformation toValueInformation(int i);

bool loadFormatBytesFromSignature(uint16_t format_signature, vector<uchar> *format_bytes);

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
bool findKey(MeasurementType mt, ValueInformation vi, int storagenr, int tariffnr,
             std::string *key, std::map<std::string,std::pair<int,DVEntry>> *values);

#define ANY_STORAGENR -1
#define ANY_TARIFFNR -1

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

// All volume values are scaled to cubic meters, m3.
bool extractDVdouble(std::map<std::string,std::pair<int,DVEntry>> *values,
                    std::string key,
                    int *offset,
                    double *value,
                    bool auto_scale = true);

bool extractDVstring(std::map<std::string,std::pair<int,DVEntry>> *values,
                     std::string key,
                     int *offset,
                     string *value);

bool extractDVdate(std::map<std::string,std::pair<int,DVEntry>> *values,
                   std::string key,
                   int *offset,
                   struct tm *value);

void extractDV(string &s, uchar *dif, uchar *vif);

#endif
