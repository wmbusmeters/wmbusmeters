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

#ifndef DVPARSER_H
#define DVPARSER_H

#include"util.h"
#include"wmbus.h"

#include<map>
#include<stdint.h>
#include<time.h>
#include<functional>
#include<vector>

// DV stands for DIF VIF

bool parseDV(Telegram *t,
             std::vector<uchar> &databytes,
             std::vector<uchar>::iterator data,
             size_t data_len,
             std::map<std::string,std::pair<int,std::string>> *values,
             std::vector<uchar>::iterator *format = NULL,
             size_t format_len = 0,
             uint16_t *format_hash = NULL,
             std::function<int(int,int,int)> overrideDifLen = NULL
             );

bool extractDVuint16(std::map<std::string,std::pair<int,std::string>> *values,
                     std::string key,
                     int *offset,
                     uint16_t *value);

// All volume values are scaled to cubic meters, m3.
bool extractDVdouble(std::map<std::string,std::pair<int,std::string>> *values,
                    std::string key,
                    int *offset,
                    double *value);

// Extract low bits from primary entry and hight bits from secondary entry.
bool extractDVdoubleCombined(std::map<std::string,std::pair<int,std::string>> *values,
                            std::string key_high_bits, // Which key to extract high bits from.
                            std::string key,
                            int *offset,
                            double *value);

bool extractDVstring(std::map<std::string,std::pair<int,std::string>> *values,
                     std::string key,
                     int *offset,
                     string *value);

bool extractDVdate(std::map<std::string,std::pair<int,std::string>> *values,
                   std::string key,
                   int *offset,
                   time_t *value);

void extractDV(string &s, uchar *dif, uchar *vif);

#endif
