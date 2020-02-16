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

#include"util.h"
#include"meters.h"
#include<assert.h>
#include<dirent.h>
#include<functional>
#include<grp.h>
#include<pwd.h>
#include<signal.h>
#include<stdarg.h>
#include<stddef.h>
#include<string.h>
#include<string>
#include<sys/errno.h>
#include<sys/stat.h>
#include<sys/time.h>
#include<syslog.h>
#include<unistd.h>
#include<sys/types.h>
#include<fcntl.h>

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

bool hex2bin(const char* src, vector<uchar> *target)
{
    if (!src) return false;
    while(*src && src[1]) {
        if (*src == ' ') {
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

bool hex2bin(string &src, vector<uchar> *target)
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

std::string bin2hex(vector<uchar> &target) {
    std::string str;
    for (size_t i = 0; i < target.size(); ++i) {
        const char ch = target[i];
        str.append(&hex[(ch  & 0xF0) >> 4], 1);
        str.append(&hex[ch & 0xF], 1);
    }
    return str;
}

std::string bin2hex(vector<uchar>::iterator data, vector<uchar>::iterator end, int len) {
    std::string str;
    while (data != end && len-- > 0) {
        const char ch = *data;
        data++;
        str.append(&hex[(ch  & 0xF0) >> 4], 1);
        str.append(&hex[ch & 0xF], 1);
    }
    return str;
}

std::string safeString(vector<uchar> &target) {
    std::string str;
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

void strprintf(std::string &s, const char* fmt, ...)
{
    char buf[4096];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, 4095, fmt, args);
    va_end(args);
    s = buf;
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
    strprintf(r, "%3.3f", v);
    return r;
}

bool syslog_enabled_ = false;
bool logfile_enabled_ = false;
bool warning_enabled_ = true;
bool verbose_enabled_ = false;
bool debug_enabled_ = false;
bool log_telegrams_enabled_ = false;

string log_file_;

void warningSilenced(bool b) {
    warning_enabled_ = !b;
}

void enableSyslog() {
    syslog_enabled_ = true;
}

bool enableLogfile(string logfile, bool daemon)
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
            n = fprintf(output, "(wmbusmeters) logging started %s\n", buf);
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

time_t telegrams_start_time_;

void logTelegramsEnabled(bool b) {
    log_telegrams_enabled_ = b;
    telegrams_start_time_ = time(NULL);
}

bool isVerboseEnabled() {
    return verbose_enabled_;
}

bool isDebugEnabled() {
    return debug_enabled_;
}

bool isLogTelegramsEnabled() {
    return log_telegrams_enabled_;
}

void outputStuff(int syslog_level, const char *fmt, va_list args)
{
    if (logfile_enabled_)
    {
        // Open close at every log occasion, should not be too big of
        // a performance issue, since normal reception speed of
        // wmbusmessages are quite low.
        FILE *output = fopen(log_file_.c_str(), "a");
        if (output) {
            vfprintf(output, fmt, args);
            fclose(output);
        } else {
            // Ouch, disable the log file.
            // Reverting to syslog or stdout depending on settings.
            logfile_enabled_ = false;
            // This warning might be written in syslog or stdout.
            warning("Log file could not be written!\n");
            // Try again with logfile disabled.
            outputStuff(syslog_level, fmt, args);
            return;
        }
    } else
    if (syslog_enabled_) {
        vsyslog(syslog_level, fmt, args);
    }
    else {
        vprintf(fmt, args);
    }
}

void info(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    outputStuff(LOG_INFO, fmt, args);
    va_end(args);
}

void notice(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    outputStuff(LOG_NOTICE, fmt, args);
    va_end(args);
}

void warning(const char* fmt, ...) {
    if (warning_enabled_) {
        va_list args;
        va_start(args, fmt);
        outputStuff(LOG_WARNING, fmt, args);
        va_end(args);
    }
}

void verbose(const char* fmt, ...) {
    if (verbose_enabled_) {
        va_list args;
        va_start(args, fmt);
        outputStuff(LOG_NOTICE, fmt, args);
        va_end(args);
    }
}

void debug(const char* fmt, ...) {
    if (debug_enabled_) {
        va_list args;
        va_start(args, fmt);
        outputStuff(LOG_NOTICE, fmt, args);
        va_end(args);
    }
}

void error(const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    outputStuff(LOG_NOTICE, fmt, args);
    va_end(args);
    exitHandler(0);
    exit(1);
}

bool isValidMatchExpression(string me, bool non_compliant)
{
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
        // Some non-compliant meters have full hex in the me....
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

bool isValidMatchExpressions(string mes, bool non_compliant)
{
    vector<string> v = splitMatchExpressions(mes);

    for (string me : v)
    {
        if (!isValidMatchExpression(me, non_compliant)) return false;
    }
    return true;
}

bool doesIdMatchExpression(string id, string match)
{
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
    if (match.front() == '*')
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

bool doesIdMatchExpressions(string& id, vector<string>& mes)
{
    bool found_match = false;
    bool found_negative_match = false;

    // Goes through all possible match expressions.
    // If no expression matches, neither positive nor negative,
    // then the result is false. (ie no match)

    // If more than one positive match is found, and no negative,
    // then the result is true.

    // If more than one negative match is found, irrespective
    // if there is any positive matches or not, then the result is false.

    for (string me : mes)
    {
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
            if (m) found_match = true;
        }
    }

    if (found_negative_match) return false;
    if (found_match) return true;
    return false;
}

bool isValidKey(string& key, MeterType mt)
{
    if (key.length() == 0) return true;
    if (key == "NOKEY") {
        key = "";
        return true;
    }
    if ((mt == MeterType::IZAR && key.length() != 16) ||
        (mt != MeterType::IZAR && key.length() != 32)) return false;
    vector<uchar> tmp;
    return hex2bin(key, &tmp);
}

bool isFrequency(std::string& fq)
{
    int len = fq.length();
    if (len == 0) return false;
    if (fq[len-1] == 'M') len--;
    for (int i=0; i<len; ++i) {
        if (!isdigit(fq[i]) && fq[i] != '.') return false;
    }
    return true;
}

bool isNumber(std::string& fq)
{
    int len = fq.length();
    if (len == 0) return false;
    for (int i=0; i<len; ++i) {
        if (!isdigit(fq[i])) return false;
    }
    return true;
}

vector<string> splitMatchExpressions(string& mes)
{
    vector<string> r;
    bool eof, err;
    vector<uchar> v (mes.begin(), mes.end());
    auto i = v.begin();

    for (;;) {
        auto id = eatTo(v, i, ',', 16, &eof, &err);
        if (err) break;
        trimWhitespace(&id);
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

void debugPayload(string intro, vector<uchar> &payload)
{
    if (isDebugEnabled())
    {
        string msg = bin2hex(payload);
        debug("%s \"%s\"\n", intro.c_str(), msg.c_str());
    }
}

void debugPayload(string intro, vector<uchar> &payload, vector<uchar>::iterator &pos)
{
    if (isDebugEnabled())
    {
        string msg = bin2hex(pos, payload.end(), 1024);
        debug("%s \"%s\"\n", intro.c_str(), msg.c_str());
    }
}

void logTelegram(string intro, vector<uchar> &parsed, int header_size, int suffix_size)
{
    if (isLogTelegramsEnabled())
    {
        time_t diff = time(NULL)-telegrams_start_time_;
        string parsed_hex = bin2hex(parsed);
        string header = parsed_hex.substr(0, header_size*2);
        string content = parsed_hex.substr(header_size*2);
        if (suffix_size == 0)
        {
            notice("%s \"telegram=|%s|%s|+%ld\"\n", intro.c_str(),
                   header.c_str(), content.c_str(), diff);
        }
        else
        {
            assert((suffix_size*2) < (int)content.size());
            string content2 = content.substr(0, content.size()-suffix_size*2);
            string suffix = content.substr(content.size()-suffix_size*2);
            notice("%s \"telegram=|%s|%s|%s|+%ld\"\n", intro.c_str(),
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

int parseTime(string time) {
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
    assert(len < 1024);
    for (size_t i=0; i<len; ++i) {
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

bool listFiles(string dir, vector<string> *files)
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

        files->push_back(string(dptr->d_name));
    }
    closedir(dp);

    return true;
}

bool loadFile(string file, vector<char> *buf)
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

string strdatetime(struct tm *datetime)
{
    char buf[256];
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M", datetime);
    return string(buf);
}

AccessCheck checkIfExistsAndSameGroup(string device)
{
    struct stat sb;

    int ok = stat(device.c_str(), &sb);

    // The file did not exist.
    if (ok) return AccessCheck::NotThere;

#if defined(__APPLE__) && defined(__MACH__)
        int groups[256];
#else
        gid_t groups[256];
#endif
    int ngroups = 256;

    struct passwd *p = getpwuid(getuid());

    int rc = getgrouplist(p->pw_name, p->pw_gid, groups, &ngroups);
    if (rc < 0) {
        error("(wmbusmeters) cannot handle users with more than 256 groups\n");
    }
    struct group *g = getgrgid(sb.st_gid);

    for (int i=0; i<ngroups; ++i) {
        if (groups[i] == g->gr_gid) {
            return AccessCheck::OK;
        }
    }

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

bool startsWith(string &s, const char *prefix)
{
    size_t len = strlen(prefix);
    if (s.length() < len) return false;
    if (s.length() == len) return s == prefix;
    return !strncmp(&s[0], prefix, len);
}

string makeQuotedJson(string &s)
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
