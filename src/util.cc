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

#include"util.h"
#include<dirent.h>
#include<functional>
#include<signal.h>
#include<stdarg.h>
#include<stddef.h>
#include<string.h>
#include<string>
#include<sys/errno.h>
#include<sys/stat.h>
#include<syslog.h>
#include<unistd.h>
#include<sys/types.h>
#include<fcntl.h>

using namespace std;

function<void()> exit_handler;

void exitHandler(int signum)
{
    if (exit_handler) exit_handler();
}

void doNothing(int signum) {
}

void onExit(function<void()> cb)
{
    exit_handler = cb;
    struct sigaction new_action, old_action;

    new_action.sa_handler = exitHandler;
    sigemptyset (&new_action.sa_mask);
    new_action.sa_flags = 0;

    sigaction (SIGINT, NULL, &old_action);
    if (old_action.sa_handler != SIG_IGN) sigaction(SIGINT, &new_action, NULL);
    sigaction (SIGHUP, NULL, &old_action);
    if (old_action.sa_handler != SIG_IGN) sigaction (SIGHUP, &new_action, NULL);
    sigaction (SIGTERM, NULL, &old_action);
    if (old_action.sa_handler != SIG_IGN) sigaction (SIGTERM, &new_action, NULL);

    new_action.sa_handler = doNothing;
    sigemptyset (&new_action.sa_mask);
    new_action.sa_flags = 0;

    sigaction (SIGUSR1, NULL, &old_action);
    if (old_action.sa_handler != SIG_IGN) sigaction(SIGUSR1, &new_action, NULL);

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

std::string bin2hex(vector<uchar>::iterator data, int len) {
    std::string str;
    while (len-- > 0) {
        const char ch = *data;
        data++;
        str.append(&hex[(ch  & 0xF0) >> 4], 1);
        str.append(&hex[ch & 0xF], 1);
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

void error(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
    exitHandler(0);
    exit(1);
}

bool syslog_enabled_ = false;
bool warning_enabled_ = true;
bool verbose_enabled_ = false;
bool debug_enabled_ = false;
bool log_telegrams_enabled_ = false;

void warningSilenced(bool b) {
    warning_enabled_ = !b;
}

void enableSyslog() {
    syslog_enabled_ = true;
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

void info(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    if (syslog_enabled_) {
        vsyslog(LOG_NOTICE, fmt, args);
    } else {
        vprintf(fmt, args);
    }
    va_end(args);
}

void notice(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    if (syslog_enabled_) {
        vsyslog(LOG_NOTICE, fmt, args);
    } else {
        vprintf(fmt, args);
    }
    va_end(args);
}

void warning(const char* fmt, ...) {
    if (warning_enabled_) {
        va_list args;
        va_start(args, fmt);
        if (syslog_enabled_) {
            vsyslog(LOG_WARNING, fmt, args);
        } else {
            vprintf(fmt, args);
        }
        va_end(args);
    }
}

void verbose(const char* fmt, ...) {
    if (verbose_enabled_) {
        va_list args;
        va_start(args, fmt);
        if (syslog_enabled_) {
            vsyslog(LOG_NOTICE, fmt, args);
        } else {
            vprintf(fmt, args);
        }
        va_end(args);
    }
}

void debug(const char* fmt, ...) {
    if (debug_enabled_) {
        va_list args;
        va_start(args, fmt);
        if (syslog_enabled_) {
            vsyslog(LOG_NOTICE, fmt, args);
        } else {
            vprintf(fmt, args);
        }
        va_end(args);
    }
}

bool isValidId(string& id)
{
    if (id.length() != 8) return false;
    for (int i=0; i<8; ++i) {
        if (id[i]<'0' || id[i]>'9') return false;
    }
    return true;
}

bool isValidKey(string& key)
{
    if (key.length() == 0) return true;
    if (key.length() != 32) return false;
    vector<uchar> tmp;
    return hex2bin(key, &tmp);
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
            error("Device %s does not exist.\n", tty);
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

bool checkIfSimulationFile(const char *file)
{
    struct stat info;

    int rc = stat(file, &info);
    if (rc != 0) {
        return false;
    }
    if (!S_ISREG(info.st_mode)) {
        return false;
    }
    const char *filename = strrchr(file, '/')+1;
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

void logTelegram(string intro, vector<uchar> &header, vector<uchar> &content)
{
    if (isLogTelegramsEnabled())
    {
        string h = bin2hex(header);
        string cntnt = bin2hex(content);
        time_t diff = time(NULL)-telegrams_start_time_;
        printf("%s \"telegram=|%s|%s|+%ld\"\n", intro.c_str(), h.c_str(), cntnt.c_str(), diff);
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
    if (c != -1 && *i != c)
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
