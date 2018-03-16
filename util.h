// Copyright (c) 2017 Fredrik Öhrström
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#ifndef UTIL_H
#define UTIL_H

#include<signal.h>
#include<stdint.h>
#include<functional>
#include<vector>

void onExit(std::function<void()> cb);

typedef unsigned char uchar;

#define call(A,B) ([A](){A->B();})
#define calll(A,B,T) ([A](T t){A->B(t);})

bool hex2bin(const char* src, std::vector<uchar> *target);
std::string bin2hex(std::vector<uchar> &target);

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

uint16_t crc_16_EN_13757(uchar *data, size_t len);

#endif
