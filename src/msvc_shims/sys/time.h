#ifndef MSVC_SHIM_SYS_TIME_H
#define MSVC_SHIM_SYS_TIME_H

/*
 * Minimal <sys/time.h> shim for MSVC. libfizmo's mathemat.h includes this
 * unconditionally (not guarded by __WIN32__) for gettimeofday(), used to seed
 * the RNG. <winsock2.h> already defines struct timeval, so include it and
 * provide a gettimeofday() backed by GetSystemTimeAsFileTime().
 */
#include <winsock2.h>
#include <windows.h>

struct timezone {
    int tz_minuteswest;
    int tz_dsttime;
};

static __inline int gettimeofday(struct timeval *tv, struct timezone *tz) {
    (void)tz;
    if (tv) {
        FILETIME ft;
        unsigned long long t;
        GetSystemTimeAsFileTime(&ft);
        t = ((unsigned long long)ft.dwHighDateTime << 32) | ft.dwLowDateTime;
        /* FILETIME is 100ns ticks since 1601-01-01; convert to Unix epoch. */
        t -= 116444736000000000ULL;
        tv->tv_sec  = (long)(t / 10000000ULL);
        tv->tv_usec = (long)((t % 10000000ULL) / 10ULL);
    }
    return 0;
}

#endif
