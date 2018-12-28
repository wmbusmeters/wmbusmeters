/*
 Copyright (C) 2017-2018 Fredrik Öhrström

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

#ifndef UTIL_H
#define UTIL_H

#include<signal.h>
#include<stdint.h>
#include<functional>
#include<vector>

void onExit(std::function<void()> cb);

typedef unsigned char uchar;

#define call(A,B) ([&](){A->B();})
#define calll(A,B,T) ([&](T t){A->B(t);})

bool hex2bin(const char* src, std::vector<uchar> *target);
bool hex2bin(std::string &src, std::vector<uchar> *target);
std::string bin2hex(std::vector<uchar> &target);
std::string bin2hex(std::vector<uchar>::iterator data, int len);
void strprintf(std::string &s, const char* fmt, ...);

void xorit(uchar *srca, uchar *srcb, uchar *dest, int len);

void error(const char* fmt, ...);
void verbose(const char* fmt, ...);
void debug(const char* fmt, ...);
void warning(const char* fmt, ...);

void warningSilenced(bool b);
void verboseEnabled(bool b);
void debugEnabled(bool b);
void logTelegramsEnabled(bool b);

bool isVerboseEnabled();
bool isDebugEnabled();
bool isLogTelegramsEnabled();

void debugPayload(std::string intro, std::vector<uchar> &payload);
void logTelegram(std::string intro, std::vector<uchar> &header, std::vector<uchar> &content);

bool isValidId(char *id);
bool isValidKey(char *key);

void incrementIV(uchar *iv, size_t len);

bool checkCharacterDeviceExists(const char *tty, bool fail_if_not);
bool checkIfSimulationFile(const char *file);
bool checkIfDirExists(const char *dir);

std::string eatTo(std::vector<uchar> &v, std::vector<uchar>::iterator &i, int c, size_t max, bool *eof, bool *err);

void padWithZeroesTo(std::vector<uchar> *content, size_t len, std::vector<uchar> *full_content);

int parseTime(std::string time);

uint16_t crc16_EN13757(uchar *data, size_t len);

// This crc is used by im871a for its serial communication.
uint16_t crc16_CCITT(uchar *data, uint16_t length);
bool     crc16_CCITT_check(uchar *data, uint16_t length);

#endif
