/*
 Copyright (C) 2017-2019 Fredrik Öhrström

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
#include<string>
#include<functional>
#include<vector>

void onExit(std::function<void()> cb);
void restoreSignalHandlers();
bool gotHupped();
void wakeMeUpOnSigChld(pthread_t t);
bool signalsInstalled();

typedef unsigned char uchar;

#define call(A,B) ([&](){A->B();})
#define calll(A,B,T) ([&](T t){A->B(t);})

bool hex2bin(const char* src, std::vector<uchar> *target);
bool hex2bin(std::string &src, std::vector<uchar> *target);
bool hex2bin(std::vector<uchar> &src, std::vector<uchar> *target);
std::string bin2hex(std::vector<uchar> &target);
std::string bin2hex(std::vector<uchar>::iterator data, std::vector<uchar>::iterator end, int len);
std::string safeString(std::vector<uchar> &target);
void strprintf(std::string &s, const char* fmt, ...);
// Return for example: 2010-03-21
std::string strdate(struct tm *date);
// Return for example: 2010-03-21 15:22:03
std::string strdatetime(struct tm *date);

void xorit(uchar *srca, uchar *srcb, uchar *dest, int len);
std::string format3fdot3f(double v);
bool enableLogfile(std::string logfile, bool daemon);
void disableLogfile();
void enableSyslog();
void error(const char* fmt, ...);
void verbose(const char* fmt, ...);
void debug(const char* fmt, ...);
void warning(const char* fmt, ...);
void info(const char* fmt, ...);
void notice(const char* fmt, ...);

void warningSilenced(bool b);
void verboseEnabled(bool b);
void debugEnabled(bool b);
void logTelegramsEnabled(bool b);

bool isVerboseEnabled();
bool isDebugEnabled();
bool isLogTelegramsEnabled();

void debugPayload(std::string intro, std::vector<uchar> &payload);
void logTelegram(std::string intro, std::vector<uchar> &header, std::vector<uchar> &content);

bool isValidMatchExpression(std::string id, bool non_compliant);
bool isValidMatchExpressions(std::string ids, bool non_compliant);
bool doesIdMatchExpression(std::string id, std::string match);
bool doesIdMatchExpressions(std::string& id, std::vector<std::string>& ids);

bool isValidKey(std::string& key);
bool isFrequency(std::string& fq);
bool isNumber(std::string& fq);

std::vector<std::string> splitMatchExpressions(std::string& mes);

void incrementIV(uchar *iv, size_t len);

bool checkCharacterDeviceExists(const char *tty, bool fail_if_not);
bool checkFileExists(const char *file);
bool checkIfSimulationFile(const char *file);
bool checkIfDirExists(const char *dir);
bool listFiles(std::string dir, std::vector<std::string> *files);
bool loadFile(std::string file, std::vector<char> *buf);

std::string eatTo(std::vector<uchar> &v, std::vector<uchar>::iterator &i, int c, size_t max, bool *eof, bool *err);

void padWithZeroesTo(std::vector<uchar> *content, size_t len, std::vector<uchar> *full_content);

int parseTime(std::string time);

uint16_t crc16_EN13757(uchar *data, size_t len);

// This crc is used by im871a for its serial communication.
uint16_t crc16_CCITT(uchar *data, uint16_t length);
bool     crc16_CCITT_check(uchar *data, uint16_t length);

// Eat characters from the vector v, iterating using i, until the end char c is found.
// If end char == -1, then do not expect any end char, get all until eof.
// If the end char is not found, return error.
// If the maximum length is reached without finding the end char, return error.
std::string eatTo(std::vector<char> &v, std::vector<char>::iterator &i, int c, size_t max, bool *eof, bool *err);
// Eat whitespace (space and tab, not end of lines).
void eatWhitespace(std::vector<char> &v, std::vector<char>::iterator &i, bool *eof);
// First eat whitespace, then start eating until c is found or eof. The found string is trimmed from beginning and ending whitespace.
std::string eatToSkipWhitespace(std::vector<char> &v, std::vector<char>::iterator &i, int c, size_t max, bool *eof, bool *err);
// Remove leading and trailing white space
void trimWhitespace(std::string *s);
// Returns true if device exists and this programs user, belongs
// to the same group that the device belongs to.
enum class AccessCheck { NotThere, NotSameGroup, OK };
AccessCheck checkIfExistsAndSameGroup(std::string device);
// Count the number of 1:s in the binary number v.
int countSetBits(int v);

bool startsWith(std::string &s, const char *prefix);

// Given alfa=beta it returns "alfa":"beta"
std::string makeQuotedJson(std::string &s);

#endif
