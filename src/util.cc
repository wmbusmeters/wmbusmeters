/*
 Copyright (C) 2017-2022 Fredrik Öhrström (gpl-3.0-or-later)

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

#include"util.h"
#include"shell.h"
#include"version.h"

#include<algorithm>
#include<assert.h>
#include<dirent.h>
#include<errno.h>
#include<fcntl.h>
#include<functional>
#include<grp.h>
#include<pwd.h>
#include<math.h>
#include<set>
#include<signal.h>
#include<stdarg.h>
#include<stddef.h>
#include<string.h>
#include<string>
#include<sys/stat.h>
#include<sys/time.h>
#include<sys/types.h>
#include<syslog.h>
#include<time.h>
#include<unistd.h>

#if defined(__APPLE__) && defined(__MACH__)
#include <mach-o/dyld.h>
#endif

using namespace std;

// Sigint, sigterm will call the exit handler.
function<void()> exit_handler_;

bool got_hupped_ {};

void exitHandler(int signum)
{
    got_hupped_ = signum == SIGHUP;
    if (exit_handler_) exit_handler_();
}

bool gotHupped()
{
    return got_hupped_;
}

pthread_t wake_me_up_on_sig_chld_ {};

void wakeMeUpOnSigChld(pthread_t t)
{
    wake_me_up_on_sig_chld_ = t;
}

void doNothing(int signum)
{
}

void signalMyself(int signum)
{
    if (wake_me_up_on_sig_chld_)
    {
        if (signalsInstalled())
        {
            pthread_kill(wake_me_up_on_sig_chld_, SIGUSR1);
        }
    }
}

struct sigaction old_int, old_hup, old_term, old_chld, old_usr1, old_usr2;

void onExit(function<void()> cb)
{
    exit_handler_ = cb;
    struct sigaction new_action;

    new_action.sa_handler = exitHandler;
    sigemptyset (&new_action.sa_mask);
    new_action.sa_flags = 0;

    sigaction(SIGINT, &new_action, &old_int);
    sigaction(SIGHUP, &new_action, &old_hup);
    sigaction(SIGTERM, &new_action, &old_term);

    new_action.sa_handler = signalMyself;
    sigemptyset (&new_action.sa_mask);
    new_action.sa_flags = 0;
    sigaction(SIGCHLD, &new_action, &old_chld);

    new_action.sa_handler = doNothing;
    sigemptyset (&new_action.sa_mask);
    new_action.sa_flags = 0;
    sigaction(SIGUSR1, &new_action, &old_usr1);

    new_action.sa_handler = doNothing;
    sigemptyset (&new_action.sa_mask);
    new_action.sa_flags = 0;
    sigaction(SIGUSR2, &new_action, &old_usr2);
}

bool signalsInstalled()
{
    return exit_handler_ != NULL;
}

void restoreSignalHandlers()
{
    exit_handler_ = NULL;

    sigaction(SIGINT, &old_int, NULL);
    sigaction(SIGHUP, &old_hup, NULL);
    sigaction(SIGTERM, &old_term, NULL);
    sigaction(SIGCHLD, &old_chld, NULL);
    sigaction(SIGUSR1, &old_usr1, NULL);
    sigaction(SIGUSR2, &old_usr2, NULL);
}

int char2int(char input)
{
    if(input >= '0' && input <= '9')
        return input - '0';
    if(input >= 'A' && input <= 'F')
        return input - 'A' + 10;
    if(input >= 'a' && input <= 'f')
        return input - 'a' + 10;
    return -1;
}

bool isHexChar(uchar c)
{
    return char2int(c) != -1;
}

// The byte 0x13 i converted into the integer value 13.
uchar bcd2bin(uchar c)
{
    return (c&15)+(c>>4)*10;
}

// The byte 0x13 is converted into the integer value 31.
uchar revbcd2bin(uchar c)
{
    return (c&15)*10+(c>>4);
}

uchar reverse(uchar c)
{
    return ((c&15)<<4) | (c>>4);
}


bool isHexString(const char* txt, bool *invalid, bool strict)
{
    *invalid = false;
    // An empty string is not an hex string.
    if (*txt == 0) return false;

    const char *i = txt;
    int n = 0;
    for (;;)
    {
        char c = *i++;
        if (!strict && c == '#') continue; // Ignore hashes if not strict
        if (!strict && c == ' ') continue; // Ignore hashes if not strict
        if (!strict && c == '|') continue; // Ignore hashes if not strict
        if (!strict && c == '_') continue; // Ignore underlines if not strict
        if (c == 0) break;
        n++;
        if (char2int(c) == -1) return false;
    }
    // An empty string is not an hex string.
    if (n == 0) return false;
    if (n%2 == 1) *invalid = true;

    return true;
}

bool isHexStringFlex(const char* txt, bool *invalid)
{
    return isHexString(txt, invalid, false);
}

bool isHexStringFlex(const string &txt, bool *invalid)
{
    return isHexString(txt.c_str(), invalid, false);
}

bool isHexStringStrict(const char* txt, bool *invalid)
{
    return isHexString(txt, invalid, true);
}

bool isHexStringStrict(const string &txt, bool *invalid)
{
    return isHexString(txt.c_str(), invalid, true);
}

bool hex2bin(const char* src, vector<uchar> *target)
{
    if (!src) return false;
    while(*src && src[1]) {
        if (*src == ' ' || *src == '#' || *src == '|' || *src == '_') {
            // Ignore space and hashes and pipes and underlines.
            src++;
        } else {
            int hi = char2int(*src);
            int lo = char2int(src[1]);
            if (hi<0 || lo<0) return false;
            target->push_back(hi*16 + lo);
            src += 2;
        }
    }
    return true;
}

bool hex2bin(const string &src, vector<uchar> *target)
{
    return hex2bin(src.c_str(), target);
}

bool hex2bin(vector<uchar> &src, vector<uchar> *target)
{
    if (src.size() % 2 == 1) return false;
    for (size_t i=0; i<src.size(); i+=2) {
        if (src[i] != ' ') {
            int hi = char2int(src[i]);
            int lo = char2int(src[i+1]);
            if (hi<0 || lo<0) return false;
            target->push_back(hi*16 + lo);
        }
    }
    return true;
}

char const hex[16] = { '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A','B','C','D','E','F'};

string bin2hex(const vector<uchar> &target) {
    string str;
    for (size_t i = 0; i < target.size(); ++i) {
        const char ch = target[i];
        str.append(&hex[(ch  & 0xF0) >> 4], 1);
        str.append(&hex[ch & 0xF], 1);
    }
    return str;
}

string bin2hex(vector<uchar>::iterator data, vector<uchar>::iterator end, int len) {
    string str;
    while (data != end && len-- > 0) {
        const char ch = *data;
        data++;
        str.append(&hex[(ch  & 0xF0) >> 4], 1);
        str.append(&hex[ch & 0xF], 1);
    }
    return str;
}

string bin2hex(vector<uchar> &data, int offset, int len) {
    string str;
    vector<uchar>::iterator i = data.begin();
    i += offset;
    while (i != data.end() && len-- > 0) {
        const char ch = *i;
        i++;
        str.append(&hex[(ch  & 0xF0) >> 4], 1);
        str.append(&hex[ch & 0xF], 1);
    }
    return str;
}

string safeString(vector<uchar> &target) {
    string str;
    for (size_t i = 0; i < target.size(); ++i) {
        const char ch = target[i];
        if (ch >= 32 && ch < 127 && ch != '<' && ch != '>') {
            str += ch;
        } else {
            str += '<';
            str.append(&hex[(ch  & 0xF0) >> 4], 1);
            str.append(&hex[ch & 0xF], 1);
            str += '>';
        }
    }
    return str;
}

string tostrprintf(const char* fmt, ...)
{
    string s;
    char buf[4096];
    va_list args;
    va_start(args, fmt);
    size_t n = vsnprintf(buf, 4096, fmt, args);
    assert(n < 4096);
    va_end(args);
    s = buf;
    return s;
}

string tostrprintf(const string& fmt, ...)
{
    string s;
    char buf[4096];
    va_list args;
    va_start(args, fmt);
    size_t n = vsnprintf(buf, 4096, fmt.c_str(), args);
    assert(n < 4096);
    va_end(args);
    s = buf;
    return s;
}

void strprintf(string *s, const char* fmt, ...)
{
    char buf[4096];
    va_list args;
    va_start(args, fmt);
    size_t n = vsnprintf(buf, 4096, fmt, args);
    assert(n < 4096);
    va_end(args);
    *s = buf;
}

void xorit(uchar *srca, uchar *srcb, uchar *dest, int len)
{
    for (int i=0; i<len; ++i) { dest[i] = srca[i]^srcb[i]; }
}

void shiftLeft(uchar *srca, uchar *srcb, int len)
{
    uchar overflow = 0;

    for (int i = len-1; i >= 0; i--)
    {
        srcb[i] = srca[i] << 1;
        srcb[i] |= overflow;
        overflow = (srca[i] & 0x80) >> 7;
    }
    return;
}

string format3fdot3f(double v)
{
    string r;
    strprintf(&r, "%3.3f", v);
    return r;
}

bool syslog_enabled_ = false;
bool logfile_enabled_ = false;
bool logging_silenced_ = false;
bool verbose_enabled_ = false;
bool debug_enabled_ = false;
bool trace_enabled_ = false;
AddLogTimestamps log_timestamps_ {};
bool stderr_enabled_ = false;
bool log_telegrams_enabled_ = false;
bool internal_testing_enabled_ = false;

string log_file_;


void silentLogging(bool b) {
    logging_silenced_ = b;
}

void enableSyslog() {
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
            n = fprintf(output, "(wmbusmeters) logging started %s using " VERSION "\n", buf);
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

void verboseEnabled(bool b) {
    verbose_enabled_ = b;
}

void debugEnabled(bool b) {
    debug_enabled_ = b;
    if (debug_enabled_) {
        verbose_enabled_ = true;
        log_telegrams_enabled_ = true;
    }
}

void traceEnabled(bool b) {
    trace_enabled_ = b;
    if (trace_enabled_) {
        debug_enabled_ = b;
        verbose_enabled_ = true;
        log_telegrams_enabled_ = true;
    }
}

void setLogTimestamps(AddLogTimestamps ts) {
    log_timestamps_ = ts;
}

void stderrEnabled(bool b) {
    stderr_enabled_ = b;
}

time_t telegrams_start_time_;

void logTelegramsEnabled(bool b) {
    log_telegrams_enabled_ = b;
    telegrams_start_time_ = time(NULL);
}

void internalTestingEnabled(bool b)
{
    internal_testing_enabled_ = b;
}

bool isInternalTestingEnabled()
{
    return internal_testing_enabled_;
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

void info(const char* fmt, ...) {
    if (!logging_silenced_) {
        va_list args;
        va_start(args, fmt);
        output_stuff(LOG_INFO, false, fmt, args);
        va_end(args);
    }
}

void notice(const char* fmt, ...) {
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

void verbose(const char* fmt, ...) {
    if (verbose_enabled_) {
        va_list args;
        va_start(args, fmt);
        output_stuff(LOG_NOTICE, false, fmt, args);
        va_end(args);
    }
}

void debug(const char* fmt, ...) {
    if (debug_enabled_) {
        va_list args;
        va_start(args, fmt);
        output_stuff(LOG_NOTICE, false, fmt, args);
        va_end(args);
    }
}

void trace(const char* fmt, ...) {
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

bool is_ascii_alnum(char c)
{
    if (c >= 'A' && c <= 'Z') return true;
    if (c >= 'a' && c <= 'z') return true;
    if (c >= '0' && c <= '9') return true;
    if (c == '_') return true;
    return false;
}

bool is_ascii(char c)
{
    if (c >= 'A' && c <= 'Z') return true;
    if (c >= 'a' && c <= 'z') return true;
    return false;
}

bool isValidAlias(const string& alias)
{
    if (alias.length() == 0) return false;

    if (!is_ascii(alias[0])) return false;

    for (char c : alias)
    {
        if (!is_ascii_alnum(c)) return false;
    }

    return true;
}

bool isValidMatchExpression(const string& s, bool non_compliant)
{
    string me = s;

    // Examples of valid match expressions:
    //  12345678
    //  *
    //  123*
    // !12345677
    //  2222222*
    // !22222222

    // A match expression cannot be empty.
    if (me.length() == 0) return false;

    // An me can be negated with an exclamation mark first.
    if (me.front() == '!') me.erase(0, 1);

    // A match expression cannot be only a negation mark.
    if (me.length() == 0) return false;

    int count = 0;
    if (non_compliant)
    {
        // Some non-compliant meters have full hex in the id,
        // but according to the standard there should only be bcd here...
        while (me.length() > 0 &&
               ((me.front() >= '0' && me.front() <= '9') ||
                (me.front() >= 'a' && me.front() <= 'f')))
        {
            me.erase(0,1);
            count++;
        }
    }
    else
    {
        // But compliant meters use only a bcd subset.
        while (me.length() > 0 &&
               (me.front() >= '0' && me.front() <= '9'))
        {
            me.erase(0,1);
            count++;
        }
    }

    bool wildcard_used = false;
    // An expression can end with a *
    if (me.length() > 0 && me.front() == '*')
    {
        me.erase(0,1);
        wildcard_used = true;
    }

    // Now we should have eaten the whole expression.
    if (me.length() > 0) return false;

    // Check the length of the matching bcd/hex
    // If no wildcard is used, then the match expression must be exactly 8 digits.
    if (!wildcard_used) return count == 8;

    // If wildcard is used, then the match expressions must be 7 or less digits,
    // even zero is allowed which means a single *, which matches any bcd/hex id.
    return count <= 7;
}

bool isValidMatchExpressions(const string& mes, bool non_compliant)
{
    vector<string> v = splitMatchExpressions(mes);

    for (string me : v)
    {
        if (!isValidMatchExpression(me, non_compliant)) return false;
    }
    return true;
}

bool isValidId(const string& id, bool accept_non_compliant)
{

    for (size_t i=0; i<id.length(); ++i)
    {
        if (id[i] >= '0' && id[i] <= '9') continue;
        if (accept_non_compliant)
        {
            if (id[i] >= 'a' && id[i] <= 'f') continue;
            if (id[i] >= 'A' && id[i] <= 'F') continue;
        }
        return false;
    }
    return true;
}

bool doesIdMatchExpression(const string& s, string match)
{
    string id = s;
    if (id.length() == 0) return false;

    // Here we assume that the match expression has been
    // verified to be valid.
    bool can_match = true;

    // Now match bcd/hex until end of id, or '*' in match.
    while (id.length() > 0 && match.length() > 0 && match.front() != '*')
    {
        if (id.front() != match.front())
        {
            // We hit a difference, it cannot match.
            can_match = false;
            break;
        }
        id.erase(0,1);
        match.erase(0,1);
    }

    bool wildcard_used = false;
    if (match.length() && match.front() == '*')
    {
        wildcard_used = true;
        match.erase(0,1);
    }

    if (can_match)
    {
        // Ok, now the match expression should be empty.
        // If wildcard is true, then the id can still have digits,
        // otherwise it must also be empty.
        if (wildcard_used)
        {
            can_match = match.length() == 0;
        }
        else
        {
            can_match = match.length() == 0 && id.length() == 0;
        }
    }

    return can_match;
}

bool hasWildCard(const string& mes)
{
    return mes.find('*') != string::npos;
}

bool doesIdsMatchExpressions(vector<string> &ids, vector<string>& mes, bool *used_wildcard)
{
    bool match = false;
    for (string &id : ids)
    {
        if (doesIdMatchExpressions(id, mes, used_wildcard))
        {
            match = true;
        }
        // Go through all ids even though there is an early match.
        // This way we can see if theres an exact match later.
    }
    return match;
}

bool doesIdMatchExpressions(const string& id, vector<string>& mes, bool *used_wildcard)
{
    bool found_match = false;
    bool found_negative_match = false;
    bool exact_match = false;
    *used_wildcard = false;

    // Goes through all possible match expressions.
    // If no expression matches, neither positive nor negative,
    // then the result is false. (ie no match)

    // If more than one positive match is found, and no negative,
    // then the result is true.

    // If more than one negative match is found, irrespective
    // if there is any positive matches or not, then the result is false.

    // If a positive match is found, using a wildcard not any exact match,
    // then *used_wildcard is set to true.

    for (string me : mes)
    {
        bool has_wildcard = hasWildCard(me);
        bool is_negative_rule = (me.length() > 0 && me.front() == '!');
        if (is_negative_rule)
        {
            me.erase(0, 1);
        }

        bool m = doesIdMatchExpression(id, me);

        if (is_negative_rule)
        {
            if (m) found_negative_match = true;
        }
        else
        {
            if (m)
            {
                found_match = true;
                if (!has_wildcard)
                {
                    exact_match = true;
                }
            }
        }
    }

    if (found_negative_match)
    {
        return false;
    }
    if (found_match)
    {
        if (exact_match)
        {
            *used_wildcard = false;
        }
        else
        {
            *used_wildcard = true;
        }
        return true;
    }
    return false;
}

string toIdsCommaSeparated(vector<string> &ids)
{
    string cs;
    for (string& s: ids)
    {
        cs += s;
        cs += ",";
    }
    if (cs.length() > 0) cs.pop_back();
    return cs;
}

bool isFrequency(const string& fq)
{
    int len = fq.length();
    if (len == 0) return false;
    if (fq[len-1] != 'M') return false;
    len--;
    for (int i=0; i<len; ++i) {
        if (!isdigit(fq[i]) && fq[i] != '.') return false;
    }
    return true;
}

bool isNumber(const string& fq)
{
    int len = fq.length();
    if (len == 0) return false;
    for (int i=0; i<len; ++i) {
        if (!isdigit(fq[i])) return false;
    }
    return true;
}

vector<string> splitMatchExpressions(const string& mes)
{
    vector<string> r;
    bool eof, err;
    vector<uchar> v (mes.begin(), mes.end());
    auto i = v.begin();

    for (;;) {
        auto id = eatTo(v, i, ',', 16, &eof, &err);
        if (err) break;
        trimWhitespace(&id);
        if (id == "ANYID") id = "*";
        r.push_back(id);
        if (eof) break;
    }
    return r;
}

void incrementIV(uchar *iv, size_t len) {
    uchar *p = iv+len-1;
    while (p >= iv) {
        int pp = *p;
        (*p)++;
        if (pp+1 <= 255) {
            // Nice, no overflow. We are done here!
            break;
        }
        // Move left add add one.
        p--;
    }
}

bool checkCharacterDeviceExists(const char *tty, bool fail_if_not)
{
    struct stat info;

    int rc = stat(tty, &info);
    if (rc != 0) {
        if (fail_if_not) {
            error("Device \"%s\" does not exist.\n", tty);
        } else {
            return false;
        }
    }
    if (!S_ISCHR(info.st_mode)) {
        if (fail_if_not) {
            error("Device %s is not a character device.\n", tty);
        } else {
            return false;
        }
    }
    return true;
}

bool checkFileExists(const char *file)
{
    struct stat info;

    int rc = stat(file, &info);
    if (rc != 0) {
        return false;
    }
    if (!S_ISREG(info.st_mode)) {
        return false;
    }
    return true;
}

bool checkIfSimulationFile(const char *file)
{
    if (!checkFileExists(file))
    {
        return false;
    }
    const char *filename = strrchr(file, '/');
    if (filename) {
        filename++;
    } else {
        filename = file;
    }
    if (filename < file) filename = file;
    if (strncmp(filename, "simulation", 10)) {
        return false;
    }
    return true;
}

bool checkIfDirExists(const char *dir)
{
    struct stat info;

    int rc = stat(dir, &info);
    if (rc != 0) {
        return false;
    }
    if (!S_ISDIR(info.st_mode)) {
        return false;
    }
    if (info.st_mode & S_IWUSR &&
        info.st_mode & S_IRUSR &&
        info.st_mode & S_IXUSR) {
        // Check the directory is writeable.
        return true;
    }
    return false;
}

void debugPayload(const string& intro, vector<uchar> &payload)
{
    if (isDebugEnabled())
    {
        string msg = bin2hex(payload);
        debug("%s \"%s\"\n", intro.c_str(), msg.c_str());
    }
}

void debugPayload(const string& intro, vector<uchar> &payload, vector<uchar>::iterator &pos)
{
    if (isDebugEnabled())
    {
        string msg = bin2hex(pos, payload.end(), 1024);
        debug("%s \"%s\"\n", intro.c_str(), msg.c_str());
    }
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
            notice_always("telegram=|%s|%s|%s|+%ld\n",
                   header.c_str(), content2.c_str(), suffix.c_str(), diff);
        }
    }
}

string eatTo(vector<uchar> &v, vector<uchar>::iterator &i, int c, size_t max, bool *eof, bool *err)
{
    string s;

    *eof = false;
    *err = false;
    while (max > 0 && i != v.end() && (c == -1 || *i != c))
    {
        s += *i;
        i++;
        max--;
    }
    if (c != -1 && i != v.end() && *i != c)
    {
        *err = true;
    }
    if (i != v.end())
    {
        i++;
    }
    if (i == v.end()) {
        *eof = true;
    }
    return s;
}

void padWithZeroesTo(vector<uchar> *content, size_t len, vector<uchar> *full_content)
{
    if (content->size() < len) {
        warning("Padded with zeroes.", (int)len);
        size_t old_size = content->size();
        content->resize(len);
        for(size_t i = old_size; i < len; ++i) {
            (*content)[i] = 0;
        }
        full_content->insert(full_content->end(), content->begin()+old_size, content->end());
    }
}

static string space = "                                                                                                                                                               ";
string padLeft(const string& input, int width)
{
    int w = width-input.size();
    if (w < 0) return input;
    assert(w < (int)space.length());
    return space.substr(0, w)+input;
}

int parseTime(const string& s)
{
    string time = s;
    int mul = 1;
    if (time.back() == 'h') {
        time.pop_back();
        mul = 3600;
    }
    if (time.back() == 'm') {
        time.pop_back();
        mul = 60;
    }
    if (time.back() == 's') {
        time.pop_back();
        mul = 1;
    }
    int n = atoi(time.c_str());
    return n*mul;
}

#define CRC16_EN_13757 0x3D65

uint16_t crc16_EN13757_per_byte(uint16_t crc, uchar b)
{
    unsigned char i;

    for (i = 0; i < 8; i++) {

        if (((crc & 0x8000) >> 8) ^ (b & 0x80)){
            crc = (crc << 1)  ^ CRC16_EN_13757;
        }else{
            crc = (crc << 1);
        }

        b <<= 1;
    }

    return crc;
}

uint16_t crc16_EN13757(uchar *data, size_t len)
{
    uint16_t crc = 0x0000;

    assert(len == 0 || data != NULL);

    for (size_t i=0; i<len; ++i)
    {
        crc = crc16_EN13757_per_byte(crc, data[i]);
    }

    return (~crc);
}

#define CRC16_INIT_VALUE 0xFFFF
#define CRC16_GOOD_VALUE 0x0F47
#define CRC16_POLYNOM    0x8408

uint16_t crc16_CCITT(uchar *data, uint16_t length)
{
    uint16_t initVal = CRC16_INIT_VALUE;
    uint16_t crc = initVal;
    while(length--)
    {
        int bits = 8;
        uchar byte = *data++;
        while(bits--)
        {
            if((byte & 1) ^ (crc & 1))
            {
                crc = (crc >> 1) ^ CRC16_POLYNOM;
            }
            else
                crc >>= 1;
            byte >>= 1;
        }
    }
    return crc;
}

bool crc16_CCITT_check(uchar *data, uint16_t length)
{
    uint16_t crc = ~crc16_CCITT(data, length);
    return crc == CRC16_GOOD_VALUE;
}

bool listFiles(const string& dir, vector<string> *files)
{
    DIR *dp = NULL;
    struct dirent *dptr = NULL;

    if (NULL == (dp = opendir(dir.c_str())))
    {
        return false;
    }
    while(NULL != (dptr = ::readdir(dp)))
    {
        if (!strcmp(dptr->d_name,".") ||
            !strcmp(dptr->d_name,".."))
        {
            // Ignore . ..  dirs.
            continue;
        }
        size_t len = strlen(dptr->d_name);
        if (len > 0 && dptr->d_name[len-1] == '~')
        {
            // Ignore emacs backup files ending in ~
            continue;
        }
        files->push_back(string(dptr->d_name));
    }
    closedir(dp);

    return true;
}

int loadFile(const string& file, vector<string> *lines)
{
    char block[32768+1];
    vector<uchar> buf;

    int fd = open(file.c_str(), O_RDONLY);
    if (fd == -1) {
        return -1;
    }
    while (true) {
        ssize_t n = read(fd, block, sizeof(block));
        if (n == -1) {
            if (errno == EINTR) {
                continue;
            }
            error("Could not read file %s errno=%d\n", file.c_str(), errno);
            close(fd);
            return -1;
        }
        buf.insert(buf.end(), block, block+n);
        if (n < (ssize_t)sizeof(block)) {
            break;
        }
    }
    close(fd);

    bool eof, err;
    auto i = buf.begin();
    for (;;) {
        string line = eatTo(buf, i, '\n', 32768, &eof, &err);
        if (err) {
            error("Error parsing simulation file.\n");
        }
        if (line.length() > 0) {
            lines->push_back(line);
        }
        if (eof) break;
    }

    return 0;
}

bool loadFile(const string& file, vector<char> *buf)
{
    int blocksize = 1024;
    char block[blocksize];

    int fd = open(file.c_str(), O_RDONLY);
    if (fd == -1) {
        warning("Could not open file %s errno=%d\n", file.c_str(), errno);
        return false;
    }
    while (true) {
        ssize_t n = read(fd, block, sizeof(block));
        if (n == -1) {
            if (errno == EINTR) {
                continue;
            }
            warning("Could not read file %s errno=%d\n", file.c_str(), errno);
            close(fd);

            return false;
        }
        buf->insert(buf->end(), block, block+n);
        if (n < (ssize_t)sizeof(block)) {
            break;
        }
    }
    close(fd);
    return true;
}

string eatToSkipWhitespace(vector<char> &v, vector<char>::iterator &i, int c, size_t max, bool *eof, bool *err)
{
    eatWhitespace(v, i, eof);
    if (*eof) {
        if (c != -1) {
            *err = true;
        }
        return "";
    }
    string s = eatTo(v,i,c,max,eof,err);
    trimWhitespace(&s);
    return s;
}

string eatTo(vector<char> &v, vector<char>::iterator &i, int c, size_t max, bool *eof, bool *err)
{
    string s;

    *eof = false;
    *err = false;
    while (max > 0 && i != v.end() && (c == -1 || *i != c))
    {
        s += *i;
        i++;
        max--;
    }
    if (c != -1 && (i == v.end() || *i != c))
    {
        *err = true;
    }
    if (i != v.end())
    {
        i++;
    }
    if (i == v.end()) {
        *eof = true;
    }
    return s;
}

void eatWhitespace(vector<char> &v, vector<char>::iterator &i, bool *eof)
{
    *eof = false;
    while (i != v.end() && (*i == ' ' || *i == '\t'))
    {
        i++;
    }
    if (i == v.end()) {
        *eof = true;
    }
}

void trimWhitespace(string *s)
{
    const char *ws = " \t";
    s->erase(0, s->find_first_not_of(ws));
    s->erase(s->find_last_not_of(ws) + 1);
}

string strdate(struct tm *date)
{
    char buf[256];
    strftime(buf, sizeof(buf), "%Y-%m-%d", date);
    return string(buf);
}

string strdate(double v)
{
    if (isnan(v)) return "null";
    struct tm date;
    time_t t = v;
    localtime_r(&t, &date);
    return strdate(&date);
}

string strdatetime(struct tm *datetime)
{
    char buf[256];
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M", datetime);
    return string(buf);
}

string strdatetime(double v)
{
    if (isnan(v)) return "null";
    struct tm datetime;
    time_t t = v;
    localtime_r(&t, &datetime);
    return strdatetime(&datetime);
}

string strdatetimesec(struct tm *datetime)
{
    char buf[256];
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", datetime);
    return string(buf);
}

string strdatetimesec(double v)
{
    struct tm datetime;
    time_t t = v;
    localtime_r(&t, &datetime);
    return strdatetimesec(&datetime);
}

bool is_leap_year(int year)
{
    year += 1900;
    if (year % 4 != 0) return false;
    if (year % 400 == 0) return true;
    if (year % 100 == 0) return false;
    return true;
}

int days_in_months[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

int get_days_in_month(int year, int month)
{
    if (month < 0 || month >= 12)
    {
        month = 0;
    }

    int days = days_in_months[month];

    if (month == 1 && is_leap_year(year))
    {
        // Handle february in a leap year.
        days += 1;
    }

    return days;
}

double addMonths(double t, int months)
{
    time_t ut = (time_t)t;
    struct tm time;
    localtime_r(&ut, &time);
    addMonths(&time, months);
    return (double)mktime(&time);
}

void addMonths(struct tm *date, int months)
{
    bool is_last_day_in_month = date->tm_mday == get_days_in_month(date->tm_year, date->tm_mon);

    int year = date->tm_year + months / 12;
    int month = date->tm_mon + months % 12;

    while (month > 11)
    {
        year += 1;
        month -= 12;
    }

    while (month < 0)
    {
        year -= 1;
        month += 12;
    }

    int day;

    if (is_last_day_in_month)
    {
        day = get_days_in_month(year, month); // Last day of month maps to last day of result month
    }
    else
    {
        day = min(date->tm_mday, get_days_in_month(year, month));
    }

    date->tm_year = year;
    date->tm_mon = month;
    date->tm_mday = day;
}

const char* toString(AccessCheck ac)
{
    switch (ac)
    {
    case AccessCheck::NoSuchDevice: return "NoSuchDevice";
    case AccessCheck::NoProperResponse: return "NoProperResponse";
    case AccessCheck::NoPermission: return "NoPermission";
    case AccessCheck::NotSameGroup: return "NotSameGroup";
    case AccessCheck::AccessOK: return "AccessOK";
    }
    return "?";
}

AccessCheck checkIfExistsAndHasAccess(const string& device)
{
    struct stat device_sb;

    int ok = stat(device.c_str(), &device_sb);

    // The file did not exist.
    if (ok) return AccessCheck::NoSuchDevice;

    int r = access(device.c_str(), R_OK);
    int w = access(device.c_str(), W_OK);
    if (r == 0 && w == 0)
    {
        // We have read and write access!
        return AccessCheck::AccessOK;
    }

    // We are not permitted to read and write to this tty. Why?
    // Lets check the group settings.

#if defined(__APPLE__) && defined(__MACH__)
        int my_groups[256];
#else
        gid_t my_groups[256];
#endif
    int ngroups = 256;

    struct passwd *p = getpwuid(getuid());

    // What are the groups I am member of?
    int rc = getgrouplist(p->pw_name, p->pw_gid, my_groups, &ngroups);
    if (rc < 0) {
        error("(wmbusmeters) cannot handle users with more than 256 groups\n");
    }

    // What is the group of the tty?
    struct group *device_group = getgrgid(device_sb.st_gid);

    // Go through my groups to see if the device's group is in there.
    for (int i=0; i<ngroups; ++i)
    {
        if (my_groups[i] == device_group->gr_gid)
        {
            // We belong to the same group as the tty. Typically dialout.
            // Then there is some other reason for the lack of access.
            return AccessCheck::NoPermission;
        }
    }
    // We have examined all the groups that we belong to and yet not
    // found the device's group. We can at least conclude that we
    // being in the device's group would help, ie dialout.
    return AccessCheck::NotSameGroup;
}

int countSetBits(int v)
{
    int n = 0;
    while (v)
    {
        v &= (v-1);
        n++;
    }
    return n;
}

bool startsWith(const string& s, string &prefix)
{
    return startsWith(s, prefix.c_str());
}

bool startsWith(const string& s, const char *prefix)
{
    size_t len = strlen(prefix);
    if (s.length() < len) return false;
    if (s.length() == len) return s == prefix;
    return !strncmp(&s[0], prefix, len);
}

string makeQuotedJson(const string& s)
{
    size_t p = s.find('=');
    string key, value;
    if (p != string::npos)
    {
        key = s.substr(0,p);
        value = s.substr(p+1);
    }
    else
    {
        key = s;
        value = "";
    }

    return string("\"")+key+"\":\""+value+"\"";
}

string currentYear()
{
    char datetime[40];
    memset(datetime, 0, sizeof(datetime));

    struct timeval tv;
    gettimeofday(&tv, NULL);

    strftime(datetime, 20, "%Y", localtime(&tv.tv_sec));
    return string(datetime);
}

string currentDay()
{
    char datetime[40];
    memset(datetime, 0, sizeof(datetime));

    struct timeval tv;
    gettimeofday(&tv, NULL);

    strftime(datetime, 20, "%Y-%m-%d", localtime(&tv.tv_sec));
    return string(datetime);
}

string currentHour()
{
    char datetime[40];
    memset(datetime, 0, sizeof(datetime));

    struct timeval tv;
    gettimeofday(&tv, NULL);

    strftime(datetime, 20, "%Y-%m-%d_%H", localtime(&tv.tv_sec));
    return string(datetime);
}

string currentMinute()
{
    char datetime[40];
    memset(datetime, 0, sizeof(datetime));

    struct timeval tv;
    gettimeofday(&tv, NULL);

    strftime(datetime, 20, "%Y-%m-%d_%H:%M", localtime(&tv.tv_sec));
    return string(datetime);
}

string currentSeconds()
{
    char datetime[40];
    memset(datetime, 0, sizeof(datetime));

    struct timeval tv;
    gettimeofday(&tv, NULL);

    strftime(datetime, 20, "%Y-%m-%d_%H:%M:%S", localtime(&tv.tv_sec));
    return string(datetime);
}

string currentMicros()
{
    char datetime[40];
    memset(datetime, 0, sizeof(datetime));

    struct timeval tv;
    gettimeofday(&tv, NULL);

    strftime(datetime, 20, "%Y-%m-%d_%H:%M:%S", localtime(&tv.tv_sec));
    return string(datetime)+"."+to_string(tv.tv_usec);
}

bool hasBytes(int n, vector<uchar>::iterator &pos, vector<uchar> &frame)
{
    int remaining = distance(pos, frame.end());
    if (remaining < n) return false;
    return true;
}

bool startsWith(const string& s, vector<uchar> &data)
{
    if (s.length() > data.size()) return false;

    for (size_t i=0; i<s.length(); ++i)
    {
        if (s[i] != data[i]) return false;
    }
    return true;
}

struct TimePeriod
{
    int day_in_week_from {}; // 0 = mon 6 = sun
    int day_in_week_to {}; // 0 = mon 6 = sun
    int hour_from {}; // Greater than or equal.
    int hour_to {}; // Less than or equal.
};

bool is_inside(struct tm *nowt, TimePeriod *tp)
{
    int day = nowt->tm_wday-1; // tm_wday 0=sun
    if (day == -1) day = 6;    // adjust so 0=mon and 6=sun
    int hour = nowt->tm_hour;  // hours since midnight 0-23

    // Test is inclusive. mon-sun(00-23) will cover whole week all hours.
    // mon-tue(00-00) will cover mon and tue one hour after midnight.
    if (day >= tp->day_in_week_from && day <= tp->day_in_week_to && hour >= tp->hour_from && hour <= tp->hour_to)
    {
        return true;
    }
    return false;
}

bool extract_times(const char *p, TimePeriod *tp)
{
    if (strlen(p) != 7) return false; // Expect (00-23)
    if (p[3] != '-') return false; // Must have - in middle.
    int fa = p[1]-48;
    if (fa < 0 || fa > 9) return false;
    int fb = p[2]-48;
    if (fb < 0 || fb > 9) return false;
    int ta = p[4]-48;
    if (ta < 0 || ta > 9) return false;
    int tb = p[5]-48;
    if (tb < 0 || tb > 9) return false;
    tp->hour_from = fa*10+fb;
    tp->hour_to = ta*10+tb;
    if (tp->hour_from > 23) return false; // Hours are 00-23
    if (tp->hour_to > 23) return false;   // Ditto.
    if (tp->hour_to < tp->hour_from) return false; // To must be strictly larger than from, hence the need for 23.

    return true;
}

int day_name_to_nr(const string& name)
{
    if (name == "mon") return 0;
    if (name == "tue") return 1;
    if (name == "wed") return 2;
    if (name == "thu") return 3;
    if (name == "fri") return 4;
    if (name == "say") return 5;
    if (name == "sun") return 6;
    return -1;
}

bool extract_days(char *p, TimePeriod *tp)
{
    if (strlen(p) == 3)
    {
        string s = p;
        int d = day_name_to_nr(s);
        if (d == -1) return false;
        tp->day_in_week_from = d;
        tp->day_in_week_to = d;
        return true;
    }

    if (strlen(p) != 7) return false; // Expect mon-fri
    if (p[3] != '-') return false; // Must have - in middle.
    string from = string(p, p+3);
    string to = string(p+4, p+7);

    int f = day_name_to_nr(from);
    int t = day_name_to_nr(to);
    if (f == -1 || t == -1) return false;
    if (f >= t) return false;
    tp->day_in_week_from = f;
    tp->day_in_week_to = t;
    return true;
}

bool extract_single_period(char *tok, TimePeriod *tp)
{
    // Minimum length is 8 chars, eg "1(00-23)"
    size_t len = strlen(tok);
    if (len < 8) return false;
    // tok is for example: mon-fri(00-23) or tue(18-19) or 1(00-23)
    char *p = strchr(tok, '(');
    if (p == NULL) return false; // There must be a (
    if (tok[len-1] != ')') return false; // Must end in )
    bool ok = extract_times(p, tp);
    if (!ok) return false;
    *p = 0; // Terminate in the middle of tok.
    ok = extract_days(tok, tp);
    if (!ok) return false;

    return true;
}

bool extract_periods(const string& periods, vector<TimePeriod> *period_structs)
{
    if (periods.length() == 0) return false;

    char buf[periods.length()+1];
    strcpy(buf, periods.c_str());

    char *saveptr {};
    char *tok = strtok_r(buf, ",", &saveptr);
    if (tok == NULL)
    {
        // No comma found.
        TimePeriod tp {};
        bool ok = extract_single_period(tok, &tp);
        if (!ok) return false;
        period_structs->push_back(tp);
        return true;
    }

    while (tok != NULL)
    {
        TimePeriod tp {};
        bool ok = extract_single_period(tok, &tp);
        if (!ok) return false;
        period_structs->push_back(tp);
        tok = strtok_r(NULL, ",", &saveptr);
    }

    return true;
}

bool isValidTimePeriod(const string& periods)
{
    vector<TimePeriod> period_structs;
    bool ok = extract_periods(periods, &period_structs);
    return ok;
}

bool isInsideTimePeriod(time_t now, string periods)
{
    struct tm nowt {};
    localtime_r(&now, &nowt);

    vector<TimePeriod> period_structs;

    bool ok = extract_periods(periods, &period_structs);
    if (!ok) return false;

    for (auto &tp : period_structs)
    {
        //debug("period %d %d %d %d\n", tp.day_in_week_from, tp.day_in_week_to, tp.hour_from, tp.hour_to);
        if (is_inside(&nowt, &tp)) return true;
    }
    return false;
}

size_t memoryUsage()
{
    return 0;
}

vector<string> alarm_shells_;

const char* toString(Alarm type)
{
    switch (type)
    {
    case Alarm::DeviceFailure: return "DeviceFailure";
    case Alarm::RegularResetFailure: return "RegularResetFailure";
    case Alarm::DeviceInactivity: return "DeviceInactivity";
    case Alarm::SpecifiedDeviceNotFound: return "SpecifiedDeviceNotFound";
    }
    return "?";
}

void logAlarm(Alarm type, string info)
{
    vector<string> envs;
    string ts = toString(type);
    envs.push_back("ALARM_TYPE="+ts);

    string msg = tostrprintf("[ALARM %s] %s", ts.c_str(), info.c_str());
    envs.push_back("ALARM_MESSAGE="+msg);

    warning("%s\n", msg.c_str());

    for (auto &s : alarm_shells_)
    {
        vector<string> args;
        args.push_back("-c");
        args.push_back(s);
        invokeShell("/bin/sh", args, envs);
    }
}

void setAlarmShells(vector<string> &alarm_shells)
{
    alarm_shells_ = alarm_shells;
}

bool stringFoundCaseIgnored(const string& h, const string& n)
{
    string haystack = h;
    string needle = n;
    // Modify haystack and needle, in place, to become lowercase.
    for_each(haystack.begin(), haystack.end(), [](char & c) {
        c = ::tolower(c);
    });
    for_each(needle.begin(), needle.end(), [](char & c) {
        c = ::tolower(c);
    });

    // Now use default c++ find, return true if needle was found in haystack.
    return haystack.find(needle) != string::npos;
}

vector<string> splitString(const string &s, char c)
{
    auto end = s.cend();
    auto start = end;

    vector<string> v;
    for (auto i = s.cbegin(); i != end; ++i)
    {
        if (*i != c)
        {
            if (start == end)
            {
                start = i;
            }
            continue;
        }
        if (start != end)
        {
            v.emplace_back(start, i);
            start = end;
        }
    }
    if (start != end)
    {
        v.emplace_back(start, end);
    }
    return v;
}

set<string> splitStringIntoSet(const string &s, char c)
{
    vector<string> v = splitString(s, c);
    set<string> words(v.begin(), v.end());
    return words;
}

vector<string> splitDeviceString(const string& ds)
{
    string s = ds;
    string cmd;

    // The CMD(...) might have colons inside.
    // Check this first.
    size_t p = s.rfind(":CMD(");
    if (s.back() == ')' && p != string::npos)
    {
        cmd = s.substr(p+1);
        s = s.substr(0,p);
    }

    // Now we can split.
    vector<string> r = splitString(s, ':');

    if (cmd != "")
    {
        // Re-append the comand last.
        r.push_back(cmd);
    }
    return r;
}

uint32_t indexFromRtlSdrName(const string& s)
{
    size_t p = s.find('_');
    if (p == string::npos) return -1;
    string n = s.substr(0, p);
    return (uint32_t)atoi(n.c_str());
}

#define KB 1024ull

string helper(size_t scale, size_t s, string suffix)
{
    size_t o = s;
    s /= scale;
    size_t diff = o-(s*scale);
    if (diff == 0) {
        return to_string(s) + ".00"+suffix;
    }
    size_t dec = (int)(100*(diff+1) / scale);
    return to_string(s) + ((dec<10)?".0":".") + to_string(dec) + suffix;
}

string humanReadableTwoDecimals(size_t s)
{
    if (s < KB)
    {
        return to_string(s) + " B";
    }
    if (s < KB * KB)
    {
        return helper(KB, s, " KiB");
    }
    if (s < KB * KB * KB)
    {
        return helper(KB*KB, s, " MiB");
    }
#if SIZEOF_SIZE_T == 8
    if (s < KB * KB * KB * KB)
    {
        return helper(KB*KB*KB, s, " GiB");
    }
    if (s < KB * KB * KB * KB * KB)
    {
        return helper(KB*KB*KB*KB, s, " TiB");
    }
    return helper(KB*KB*KB*KB*KB, s, " PiB");
#else
    return helper(KB*KB*KB, s, " GiB");
#endif
}

bool check_if_rtlwmbus_exists_in_path()
{
    bool found = false;
    vector<string> args;
    args.push_back("-c");
    args.push_back("rtl_wmbus < /dev/null");
    vector<string> envs;
    string out;
    int rc = invokeShellCaptureOutput("/bin/sh", args, envs, &out, true);
    if ((rc == 0 || // Newest version of rtl_wmbus properly returns 0.
         rc == 2)   // Older version returns 2.
        && out.find("rtl_wmbus") == string::npos)
    {
        debug("(main) rtl_wmbus found in path\n");
        found = true;
    }
    else
    {
        debug("(main) rtl_wmbus NOT found in path\n");
    }
    return found;
}

bool check_if_rtlsdr_exists_in_path()
{
    bool found = false;
    vector<string> args;
    args.push_back("-c");
    args.push_back("rtl_sdr < /dev/null");
    vector<string> envs;
    string out;
    invokeShellCaptureOutput("/bin/sh", args, envs, &out, true);
    if (out.find("RTL2832") != string::npos)
    {
        debug("(main) rtl_srd found in path\n");
        found = true;
    }
    else
    {
        debug("(main) rtl_sdr NOT found in path\n");
    }
    return found;
}

string currentProcessExe()
{
    char buf[1024];
    memset(buf, 0, 1024);

#if defined(__APPLE__) && defined(__MACH__)
    uint32_t size = sizeof(buf);

    int rs = _NSGetExecutablePath(buf,&size);
    if (rs != 0)
    {
        // Buf not big enough.
        return "";
    }
    return buf;
#else
#  if (defined(__FreeBSD__))
        const char *self = "/proc/curproc/file";
    #else
        const char *self = "/proc/self/exe";
    #endif

    ssize_t s = readlink(self, buf, 1023);

    if (s == 1023) return "";
    if (s <= 0) return "";
    return buf;
#endif
}

string dirname(const string& p)
{
    size_t s = p.rfind('/');
    if (s == string::npos) return "";
    return p.substr(0, s);
}

string lookForExecutable(const string& prog, string bin_dir, string default_dir)
{
    string tmp = bin_dir+"/"+prog;
    if (checkFileExists(tmp.c_str()))
    {
        return tmp;
    }
    tmp = default_dir+"/"+prog;
    if (checkFileExists(tmp.c_str()))
    {
        return tmp;
    }
    return "";
}

bool parseExtras(const string& s, map<string,string> *extras)
{
    vector<string> parts = splitString(s, ' ');

    for (auto &p : parts)
    {
        vector<string> kv = splitString(p, '=');
        if (kv.size() != 2) return false;
        (*extras)[kv[0]] = kv[1];
    }
    return true;
}

void checkIfMultipleWmbusMetersRunning()
{
    pid_t my_pid = getpid();
    vector<int> daemons;
    detectProcesses("wmbusmetersd", &daemons);
    for (int i : daemons)
    {
        if (i != my_pid)
        {
            info("Notice! Wmbusmeters daemon (pid %d) is running and it might hog any wmbus devices.\n", i);
        }
    }

    vector<int> processes;
    detectProcesses("wmbusmeters", &processes);

    for (int i : processes)
    {
        if (i != my_pid)
        {
            info("Notice! Other wmbusmeters (pid %d) is running and it might hog any wmbus devices.\n", i);
        }
    }
}

bool isValidBps(const string& b)
{
    if (b == "300") return true;
    if (b == "600") return true;
    if (b == "1200") return true;
    if (b == "2400") return true;
    if (b == "4800") return true;
    if (b == "9600") return true;
    if (b == "14400") return true;
    if (b == "19200") return true;
    if (b == "38400") return true;
    if (b == "57600") return true;
    if (b == "115200") return true;
    return false;
}

bool findBytes(vector<uchar> &v, uchar a, uchar b, uchar c, size_t *out)
{
    size_t p = 0;

    while (p+2 < v.size())
    {
        if (v[p+0] == a &&
            v[p+1] == b &&
            v[p+2] == c)
        {
            *out = p;
            return true;
        }
        p++;
    }
    *out = 999999;
    return false;
}

string reverseBCD(const string& v)
{
    int vlen = v.length();
    if (vlen % 2 != 0)
    {
        return "BADHEX:"+v;
    }

    string n = "";
    for (int i=0; i<vlen; i+=2)
    {
        n += v[vlen-2-i];
        n += v[vlen-1-i];
    }
    return n;
}

string reverseBinaryAsciiSafeToString(const string& v)
{
    vector<uchar> bytes;
    bool ok = hex2bin(v, &bytes);
    if (!ok) return "BADHEX:"+v;
    reverse(bytes.begin(), bytes.end());
    return safeString(bytes);
}

#define SLIP_END             0xc0    /* indicates end of packet */
#define SLIP_ESC             0xdb    /* indicates byte stuffing */
#define SLIP_ESC_END         0xdc    /* ESC ESC_END means END data byte */
#define SLIP_ESC_ESC         0xdd    /* ESC ESC_ESC means ESC data byte */

void addSlipFraming(vector<uchar>& from, vector<uchar> &to)
{
    to.push_back(SLIP_END);
    for (uchar c : from)
    {
        if (c == SLIP_END)
        {
            to.push_back(SLIP_ESC);
            to.push_back(SLIP_ESC_END);
        }
        else if (c == SLIP_ESC)
        {
            to.push_back(SLIP_ESC);
            to.push_back(SLIP_ESC_ESC);
        }
        else
        {
            to.push_back(c);
        }
    }
    to.push_back(SLIP_END);
}

void removeSlipFraming(vector<uchar>& from, size_t *frame_length, vector<uchar> &to)
{
    *frame_length = 0;
    to.clear();
    to.reserve(from.size());
    bool esc = false;
    size_t i;
    bool found_end = false;

    for (i = 0; i < from.size(); ++i)
    {
        uchar c = from[i];
        if (c == SLIP_END)
        {
            if (to.size() > 0)
            {
                found_end = true;
                i++;
                break;
            }
        }
        else if (c == SLIP_ESC)
        {
            esc = true;
        }
        else if (esc)
        {
            if (c == SLIP_ESC_END) to.push_back(SLIP_END);
            else if (c == SLIP_ESC_ESC) to.push_back(SLIP_ESC);
            else to.push_back(c); // This is an error......
        }
        else
        {
            to.push_back(c);
        }
    }

    if (found_end)
    {
        *frame_length = i;
    }
    else
    {
        *frame_length = 0;
        to.clear();
    }

}

// Check if hex string is likely to be ascii
bool isLikelyAscii(const string& v)
{
    vector<uchar> val;
    bool ok = hex2bin(v, &val);

    // For example 64 bits:
    // 0000 0000 4142 4344
    // is probably the string DCBA

    if (!ok) return false;

    size_t i = 0;
    for (; i < val.size(); ++i)
    {
        if (val[i] != 0) break;
    }

    if (i == val.size())
    {
        // Value is all zeroes, this is probably a number.
        return false;
    }

    for (; i < val.size(); ++i)
    {
        if (val[i] < 20 || val[i] > 126) return false;
    }

    return true;
}

string joinStatusOKStrings(const string &aa, const string &bb)
{
    string a = aa;
    while (a.length() > 0 && a.back() == ' ') a.pop_back();
    string b = bb;
    while (b.length() > 0 && b.back() == ' ') b.pop_back();

    if (a == "" || a == "OK" || a == "null")
    {
        if (b == "" || b == "null") return "OK";
        return b;
    }
    if (b == "" || b == "OK" || b == "null")
    {
        if (a == "" || a == "null") return "OK";
        return a;
    }

    return a+" "+b;
}

string joinStatusEmptyStrings(const string &aa, const string &bb)
{
    string a = aa;
    while (a.length() > 0 && a.back() == ' ') a.pop_back();
    string b = bb;
    while (b.length() > 0 && b.back() == ' ') b.pop_back();

    if (a == "" || a == "null")
    {
        if (b == "" || b == "null") return "";
        return b;
    }
    if (b == "" || b == "null")
    {
        if (a == "" || a == "null") return "";
        return a;
    }

    return a+" "+b;
}


string sortStatusString(const string &a)
{
    set<string> flags;
    string curr;

    for (size_t i=0; i<a.length(); ++i)
    {
        uchar c = a[i];
        if (c == ' ')
        {
            if (curr.length() > 0)
            {
                flags.insert(curr);
                curr = "";
            }
        }
        else
        {
            curr += c;
        }
    }

    if (curr.length() > 0)
    {
        flags.insert(curr);
        curr = "";
    }

    string result;
    for (auto &s : flags)
    {
        result += s+" ";
    }

    while (result.size() > 0 && result.back() == ' ') result.pop_back();

    // This feature is only used for the em24 deprecated backwards compatible error field.
    // This should go away in the future.
    replace(result.begin(), result.end(), '~', ' ');

    return result;
}

uchar *safeButUnsafeVectorPtr(vector<uchar> &v)
{
    if (v.size() == 0) return NULL;
    return &v[0];
}

int strlen_utf8(const char *s)
{
    int len = 0;
    for (; *s; ++s) if ((*s & 0xC0) != 0x80) ++len;
    return len;
}
