#ifndef XTHREAD_BASE_TIME_H
#define XTHREAD_BASE_TIME_H

#include <time.h>                            // timespec, clock_gettime
#include <sys/time.h>                        // timeval, gettimeofday
#include <stdint.h>                          // int64_t, uint64_t

namespace xthread
{
namespace base
{
inline int64_t timespec_to_nanoseconds(const timespec& ts) {
    return ts.tv_sec * 1000000000L + ts.tv_nsec;
}

inline int64_t timespec_to_microseconds(const timespec& ts) {
    return timespec_to_nanoseconds(ts) / 1000L;
}

inline int64_t timespec_to_milliseconds(const timespec& ts) {
    return timespec_to_nanoseconds(ts) / 1000000L;
}

inline int64_t timespec_to_seconds(const timespec& ts) {
    return timespec_to_nanoseconds(ts) / 1000000000L;
}

inline int64_t gettimeofday_us() {
    struct timeval currTime;
    int ret = gettimeofday(&currTime, NULL);
    (void)ret;
    return currTime.tv_sec * 1000000L + currTime.tv_usec;
}

inline int64_t gettimeofday_ms() {
    return gettimeofday_us() / 1000;
}

inline timespec ns2timespec(int64_t ns) {
    timespec ret = {0, 0};
    ret.tv_sec = ns / 1000000000L;
    ret.tv_nsec = ns - ret.tv_sec * 1000000000L;
    return ret;
}

inline timespec us2timespec(int64_t us) {
    return ns2timespec(us * 1000);
}

inline timespec milliseconds2timespec(int64_t ms) {
    return ns2timespec(ms * 1000000L);
}

inline timespec seconds2timespec(int64_t s) {
    return ns2timespec(s * 1000000000L);
}

inline void timespec_normalize(timespec* tm) {
    if (tm->tv_nsec >= 1000000000L) {
        const int64_t added_sec = tm->tv_nsec / 1000000000L;
        tm->tv_sec += added_sec;
        tm->tv_nsec -= added_sec * 1000000000L;
    } else if (tm->tv_nsec < 0) {
        const int64_t sub_sec = (tm->tv_nsec - 999999999L) / 1000000000L;
        tm->tv_sec += sub_sec;
        tm->tv_nsec -= sub_sec * 1000000000L;
    }
}

inline timespec nanoseconds_from(timespec start_time, int64_t nanoseconds) {
    start_time.tv_nsec += nanoseconds;
    timespec_normalize(&start_time);
    return start_time;
}

inline timespec nanoseconds_from_now(int64_t nanoseconds) {
    timespec time;
    clock_gettime(CLOCK_REALTIME, &time);
    return nanoseconds_from(time, nanoseconds);
}

inline timespec milliseconds_from(timespec start_time, int64_t milliseconds) {
    return nanoseconds_from(start_time, milliseconds * 1000000L);
}

inline timespec seconds_from(timespec start_time, int64_t seconds) {
    return nanoseconds_from(start_time, seconds * 1000000000L);
}

inline timespec seconds_from_now(int64_t seconds) {
    return nanoseconds_from_now(seconds * 1000000000L);
}

}
}
#endif
