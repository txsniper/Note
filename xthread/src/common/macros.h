#ifndef XTHREAD_COMMON_MACROS_H
#define XTHREAD_COMMON_MACROS_H

#define likely(x) __builtin_expect(!!(x), 1)
#define unlikely(x) __builtin_expect(!!(x), 0)

#endif
