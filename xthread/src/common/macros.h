#ifndef XTHREAD_COMMON_MACROS_H
#define XTHREAD_COMMON_MACROS_H
#include <cstddef>
#define likely(x) __builtin_expect(!!(x), 1)
#define unlikely(x) __builtin_expect(!!(x), 0)

namespace xthread
{
namespace base
{
    template <typename T, size_t N>
    char (&ArraySizeHelper(T (&array)[N]))[N];
    template <typename T, size_t N>
    char (&ArraySizeHelper(const T (&array)[N]))[N];
}
}

#define arraysize(array) (sizeof(xthread::base::ArraySizeHelper(array)))

#define ARRAY_SIZE(array) arraysize(array)
#endif
