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

#include"util.h"
#include<functional>
#include<signal.h>
#include<stdarg.h>
#include<stddef.h>
#include<string.h>
#include<string>

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

bool verbose_enabled_ = false;

void verboseEnabled(bool b) {
    verbose_enabled_ = b;
}

void verbose(const char* fmt, ...) {
    if (verbose_enabled_) {
        va_list args;
        va_start(args, fmt);
        vprintf(fmt, args);
        va_end(args);
    }
}

bool isValidId(char *id)
{
    if (strlen(id) != 8) return false;
    for (int i=0; i<8; ++i) {
        if (id[i]<'0' || id[i]>'9') return false;
    }
    return true;
}

bool isValidKey(char *key)
{
    if (strlen(key) != 32) return false;
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
