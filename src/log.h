/*
 Copyright (C) 2017-2026 Fredrik Öhrström (gpl-3.0-or-later)

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

#ifndef LOG_H
#define LOG_H

#include"always.h"

#include<string>
#include<vector>

enum class AddLogTimestamps
{
    NotSet, Never, Always, Important
};

void info(const char* fmt, ...);
void notice(const char* fmt, ...);
void notice_always(const char* fmt, ...);
void notice_timestamp(const char* fmt, ...);

void warning(const char* fmt, ...);
void error(const char* fmt, ...);

#define verbose(...) { if (isVerboseEnabled()) { verbose_int(__VA_ARGS__); } }
void verbose_int(const char* fmt, ...);

#define trace(...) { if (isTraceEnabled()) { trace_int(__VA_ARGS__); } }
void trace_int(const char* fmt, ...);

#define debug(...) { if (isDebugEnabled()) { debug_int(__VA_ARGS__); } }
void debug_int(const char* fmt, ...);

#define debugPrefixed(prefix, content) { if (isDebugEnabled()) { debug_prefixed_int(prefix, content); } }
void debug_prefixed_int(const char *prefix, const char* content);

bool enableLogfile(const std::string& logfile, bool daemon);
void disableLogfile();
void enableSyslog();

void silentLogging(bool b);
void verboseEnabled(bool b);
void debugEnabled(bool b);
void traceEnabled(bool b);

void setLogTimestamps(AddLogTimestamps ts);
void stderrEnabled(bool b);
void logTelegramsEnabled(bool b);
void internalTestingEnabled(bool b);
bool isInternalTestingEnabled();

bool isVerboseEnabled();
bool isDebugEnabled();
bool isTraceEnabled();
bool isLogTelegramsEnabled();

void debugPayload(const std::string& intro, std::vector<uchar> &payload);
void debugPayload(const std::string& intro, std::vector<uchar> &payload, std::vector<uchar>::iterator &pos);
void logTelegram(std::vector<uchar> &original, std::vector<uchar> &parsed, int header_size, int suffix_size);

#endif // LOG_H
