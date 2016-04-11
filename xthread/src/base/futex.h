#ifndef XTHREAD_BASE_FUTEX
#define XTHREAD_BASE_FUTEX
#include <syscall.h>
#include <unistd.h>
#include <time.h>
#include <linux/futex.h>
#include "../common/log.h"
namespace xthread
{
namespace base
{
inline long int futex_wait_private(void* addr1, int expected, const timespec* timeout) {
#ifdef DEBUG
    if (timeout != NULL) {
        long nsec = timeout->tv_nsec;
        long sec = timeout->tv_sec;
        if (sec < 0 || nsec > 1000000000L) {
            log_fatal("param error");
        }
    }
#endif
    return syscall(SYS_futex, addr1, (FUTEX_WAIT | FUTEX_PRIVATE_FLAG),
            expected, timeout, NULL, 0);
}

inline long int futex_wake_private(void* addr1, int nwake) {
    return syscall(SYS_futex, addr1, (FUTEX_WAKE | FUTEX_PRIVATE_FLAG),
            nwake, NULL, NULL, 0);
}
}
}
#endif
