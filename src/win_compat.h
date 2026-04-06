/* Copyright (C) 2026 Fredrik Öhrström (gpl-3.0-or-later)
   Windows/MinGW compatibility shims automatically included during build. */
#ifndef WIN_COMPAT_H
#define WIN_COMPAT_H

#ifdef _WIN32

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#define _WIN32_WINNT 0x0601  /* Windows 7+ */
#define _USE_MATH_DEFINES    /* enable M_PI, M_E etc. in <math.h> */

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>

/* strndup is POSIX-only, not in Windows CRT */
#ifndef strndup
static inline char *strndup(const char *s, size_t n)
{
    size_t len = strnlen(s, n);
    char *p = (char *)malloc(len + 1);
    if (p) { memcpy(p, s, len); p[len] = '\0'; }
    return p;
}
#endif

/* sleep/usleep: POSIX-only, map to Windows Sleep (milliseconds).
   Declare Sleep() without pulling in all of windows.h to avoid name collisions. */
#ifdef __cplusplus
extern "C" {
#endif
__declspec(dllimport) void __stdcall Sleep(unsigned long dwMilliseconds);
#ifdef __cplusplus
}
#endif

#ifndef sleep
static inline unsigned int sleep(unsigned int seconds)
{
    Sleep((unsigned long)seconds * 1000UL);
    return 0;
}
#endif

#ifndef usleep
static inline int usleep(unsigned long usec)
{
    Sleep((unsigned long)((usec + 999UL) / 1000UL));
    return 0;
}
#endif

/* strptime is POSIX-only — minimal implementation for the formats used in wmbusmeters */
static inline char *strptime(const char *s, const char *fmt, struct tm *tm)
{
    const char *sp = s;
    const char *fp = fmt;
    while (*fp) {
        if (*fp == '%') {
            fp++;
            int val = 0;
            int n = 0;
            switch (*fp) {
            case 'Y':
                if (sscanf(sp, "%4d%n", &val, &n) != 1) return NULL;
                tm->tm_year = val - 1900; sp += n; break;
            case 'm':
                if (sscanf(sp, "%2d%n", &val, &n) != 1) return NULL;
                tm->tm_mon = val - 1; sp += n; break;
            case 'd':
                if (sscanf(sp, "%2d%n", &val, &n) != 1) return NULL;
                tm->tm_mday = val; sp += n; break;
            case 'H':
                if (sscanf(sp, "%2d%n", &val, &n) != 1) return NULL;
                tm->tm_hour = val; sp += n; break;
            case 'M':
                if (sscanf(sp, "%2d%n", &val, &n) != 1) return NULL;
                tm->tm_min = val; sp += n; break;
            case 'S':
                if (sscanf(sp, "%2d%n", &val, &n) != 1) return NULL;
                tm->tm_sec = val; sp += n; break;
            default:
                if (*sp != *fp) return NULL;
                sp++; break;
            }
            fp++;
        } else {
            if (*sp != *fp) return NULL;
            sp++; fp++;
        }
    }
    return (char *)sp;
}

/* gmtime_r is POSIX; Windows has gmtime_s with reversed argument order */
#ifndef gmtime_r
static inline struct tm *gmtime_r(const time_t *timep, struct tm *result)
{
    gmtime_s(result, timep);
    return result;
}
#endif

/* localtime_r is POSIX; Windows has localtime_s with reversed argument order */
#ifndef localtime_r
static inline struct tm *localtime_r(const time_t *timep, struct tm *result)
{
    localtime_s(result, timep);
    return result;
}
#endif

/* POSIX timeval macros not available on Windows */
#ifndef timerisset
#define timerisset(tvp) ((tvp)->tv_sec || (tvp)->tv_usec)
#endif
#ifndef timerclear
#define timerclear(tvp) ((tvp)->tv_sec = (tvp)->tv_usec = 0)
#endif
#ifndef timersub
#define timersub(a, b, result) \
    do { \
        (result)->tv_sec  = (a)->tv_sec  - (b)->tv_sec;  \
        (result)->tv_usec = (a)->tv_usec - (b)->tv_usec; \
        if ((result)->tv_usec < 0) { \
            --(result)->tv_sec; \
            (result)->tv_usec += 1000000; \
        } \
    } while (0)
#endif
#ifndef timeradd
#define timeradd(a, b, result) \
    do { \
        (result)->tv_sec  = (a)->tv_sec  + (b)->tv_sec;  \
        (result)->tv_usec = (a)->tv_usec + (b)->tv_usec; \
        if ((result)->tv_usec >= 1000000) { \
            ++(result)->tv_sec; \
            (result)->tv_usec -= 1000000; \
        } \
    } while (0)
#endif

/* MinGW provides gettimeofday and timeval in sys/time.h */
#include <sys/time.h>


/* timegm converts struct tm (UTC) to time_t — POSIX only, not on Windows */
static inline time_t timegm(struct tm *tm)
{
    /* _mkgmtime is the Windows equivalent */
    return _mkgmtime(tm);
}

/* uint/ushort/ulong are POSIX typedefs not in Windows standard headers */
#ifndef _UINT_DEFINED
#define _UINT_DEFINED
typedef unsigned int uint;
#endif
#ifndef _USHORT_DEFINED
#define _USHORT_DEFINED
typedef unsigned short ushort;
#endif
#ifndef _ULONG_DEFINED
#define _ULONG_DEFINED
typedef unsigned long ulong;
#endif

/* syslog severity constants — used as numeric log levels, not actual syslog */
#ifndef LOG_EMERG
#define LOG_EMERG   0
#define LOG_ALERT   1
#define LOG_CRIT    2
#define LOG_ERR     3
#define LOG_WARNING 4
#define LOG_NOTICE  5
#define LOG_INFO    6
#define LOG_DEBUG   7
#endif

/* unistd.h doesn't exist on Windows — provide minimal stubs */
#ifndef _UNISTD_H
#define _UNISTD_H
#include <io.h>
#include <process.h>
#endif

#endif /* _WIN32 */
#endif /* WIN_COMPAT_H */
