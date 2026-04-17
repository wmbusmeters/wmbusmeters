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

#include"log.h"
#include"util.h"
#include"version.h"

#include<assert.h>
#include<stdarg.h>
#include<string.h>
#include<syslog.h>
#include<time.h>

using namespace std;

// From util.c
void exitHandler(int signum);

bool syslog_enabled_ = false;
bool logfile_enabled_ = false;
bool logging_silenced_ = false;
bool verbose_enabled_ = false;
bool debug_enabled_ = false;
bool trace_enabled_ = false;
AddLogTimestamps log_timestamps_ {};
bool stderr_enabled_ = false;
bool log_telegrams_enabled_ = false;
string log_file_;

void silentLogging(bool b)
{
    logging_silenced_ = b;
}

void enableSyslog()
{
    syslog_enabled_ = true;
}

bool enableLogfile(const string& logfile, bool daemon)
{
    log_file_ = logfile;
    logfile_enabled_ = true;
    FILE *output = fopen(log_file_.c_str(), "a");
    if (output) {
        char buf[256];
        time_t now = time(NULL);
        strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", localtime(&now));
        int n = 0;
        if (daemon) {
            n = fprintf(output, "(wmbusmeters) logging started %s %s\n", buf, VERSION);
            if (n == 0) {
                logfile_enabled_ = false;
                return false;
            }
        }
        fclose(output);
        return true;
    }
    logfile_enabled_ = false;
    return false;
}

void disableLogfile()
{
    logfile_enabled_ = false;
}

void verboseEnabled(bool b)
{
    verbose_enabled_ = b;
}

void debugEnabled(bool b)
{
    debug_enabled_ = b;
    if (debug_enabled_) {
        verbose_enabled_ = true;
        log_telegrams_enabled_ = true;
    }
}

void traceEnabled(bool b)
{
    trace_enabled_ = b;
    if (trace_enabled_)
    {
        debug_enabled_ = b;
        verbose_enabled_ = true;
        log_telegrams_enabled_ = true;
    }
}

void setLogTimestamps(AddLogTimestamps ts)
{
    log_timestamps_ = ts;
}

void stderrEnabled(bool b)
{
    stderr_enabled_ = b;
}

time_t telegrams_start_time_;

void logTelegramsEnabled(bool b)
{
    log_telegrams_enabled_ = b;
    telegrams_start_time_ = time(NULL);
}

bool isVerboseEnabled() {
    return verbose_enabled_;
}

bool isDebugEnabled() {
    return debug_enabled_;
}

bool isTraceEnabled() {
    return trace_enabled_;
}

bool isLogTelegramsEnabled() {
    return log_telegrams_enabled_;
}

void output_stuff(int syslog_level, bool use_timestamp, const char *fmt, va_list args)
{
    string timestamp;
    bool add_timestamp = false;

    if (log_timestamps_ == AddLogTimestamps::Always ||
        (log_timestamps_ == AddLogTimestamps::Important && use_timestamp))
    {
        timestamp = currentSeconds();
        add_timestamp = true;
    }
    if (logfile_enabled_)
    {
        // Open close at every log occasion, should not be too big of
        // a performance issue, since normal reception speed of
        // wmbusmessages are quite low.
        FILE *output = fopen(log_file_.c_str(), "a");
        if (output)
        {
            if (add_timestamp) fprintf(output, "[%s] ", timestamp.c_str());
            vfprintf(output, fmt, args);
            fclose(output);
        }
        else
        {
            // Ouch, disable the log file.
            // Reverting to syslog or stdout depending on settings.
            logfile_enabled_ = false;
            // This warning might be written in syslog or stdout.
            warning("Log file could not be written!\n");
            // Try again with logfile disabled.
            output_stuff(syslog_level, use_timestamp, fmt, args);
            return;
        }
    }
    else
    if (syslog_enabled_)
    {
        // Do not print timestamps in the syslog since it already adds timestamps.
        vsyslog(syslog_level, fmt, args);
    }
    else
    {
        if (stderr_enabled_)
        {
            if (add_timestamp) fprintf(stderr, "[%s] ", timestamp.c_str());
            vfprintf(stderr, fmt, args);
        }
        else
        {
            if (add_timestamp) printf("[%s] ", timestamp.c_str());
            vprintf(fmt, args);
        }
    }
}

void info(const char* fmt, ...)
{
    if (!logging_silenced_) {
        va_list args;
        va_start(args, fmt);
        output_stuff(LOG_INFO, false, fmt, args);
        va_end(args);
    }
}

void notice(const char* fmt, ...)
{
    if (!logging_silenced_) {
        va_list args;
        va_start(args, fmt);
        output_stuff(LOG_NOTICE, false, fmt, args);
        va_end(args);
    }
}

void notice_always(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    output_stuff(LOG_NOTICE, false, fmt, args);
    va_end(args);
}

void notice_timestamp(const char* fmt, ...) {
    if (!logging_silenced_) {
        va_list args;
        va_start(args, fmt);
        output_stuff(LOG_NOTICE, true, fmt, args);
        va_end(args);
    }
}

void warning(const char* fmt, ...) {
    if (!logging_silenced_) {
        va_list args;
        va_start(args, fmt);
        output_stuff(LOG_WARNING, true, fmt, args);
        va_end(args);
    }
}

void verbose_int(const char* fmt, ...) {
    if (verbose_enabled_) {
        va_list args;
        va_start(args, fmt);
        output_stuff(LOG_NOTICE, false, fmt, args);
        va_end(args);
    }
}

void debug_int(const char* fmt, ...) {
    if (debug_enabled_) {
        va_list args;
        va_start(args, fmt);
        output_stuff(LOG_NOTICE, false, fmt, args);
        va_end(args);
    }
}

void debug_prefixed_int(const char *prefix, const char* content) {
    if (debug_enabled_) {
        const char *stop = content+strlen(content);
        const char *line = content;
        const char *eol = NULL;

        while (line < stop)
        {
            eol = strchr(line, '\n');
            if (!eol) eol = stop;
            int len = eol-line;
            debug("%s %.*s\n", prefix, len, line);
            line = eol+1;
        }
    }
}

void trace_int(const char* fmt, ...) {
    if (trace_enabled_) {
        va_list args;
        va_start(args, fmt);
        output_stuff(LOG_NOTICE, false, fmt, args);
        va_end(args);
    }
}

void error(const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    output_stuff(LOG_NOTICE, true, fmt, args);
    va_end(args);
    exitHandler(0);
    exit(1);
}

void logTelegram(vector<uchar> &original, vector<uchar> &parsed, int header_size, int suffix_size)
{
    if (isLogTelegramsEnabled())
    {
        vector<uchar> logged = parsed;
        if (!original.empty())
        {
            logged = vector<uchar>(parsed);
            for (unsigned int i = 0; i < original.size(); i++)
            {
                logged[i] = original[i];
            }
        }
        time_t diff = time(NULL)-telegrams_start_time_;
        string parsed_hex = bin2hex(logged);
        string header = parsed_hex.substr(0, header_size*2);
        string content = parsed_hex.substr(header_size*2);
        if (suffix_size == 0)
        {
            notice_always("telegram=|%s_%s|+%ld\n",
                   header.c_str(), content.c_str(), diff);
        }
        else
        {
            assert((suffix_size*2) < (int)content.size());
            string content2 = content.substr(0, content.size()-suffix_size*2);
            string suffix = content.substr(content.size()-suffix_size*2);
            notice_always("telegram=|%s_%s_%s|+%ld\n",
                   header.c_str(), content2.c_str(), suffix.c_str(), diff);
        }
    }
}

void debugPayload(const string& intro, vector<uchar> &payload)
{
    debug("%s \"%s\"\n", intro.c_str(), bin2hex(payload).c_str());
}

void debugPayload(const string& intro, vector<uchar> &payload, vector<uchar>::iterator &pos)
{
    debug("%s \"%s\"\n", intro.c_str(), bin2hex(pos, payload.end(), 1024).c_str());
}
