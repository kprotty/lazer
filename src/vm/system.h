#ifndef _LZR_SYSTEM_H
#define _LZR_SYSTEM_H

#include <lazer.h>

#if defined(_WIN32)
    #define LZR_WINDOWS
#elif defined(__linux__)
    #define LZR_LINUX
#endif

#if defined(__i386__) || defined(__x86_64__)
    #define LZR_X86
#elif defined(__arm__) || defined(__aarch64__)
    #define LZR_ARM
#endif

#if UINTPTR_MAX == 0xffffffffffffffff
    #define LZR_64
#elif UINTPTR_MAX == 0xffffffff
    #define LZR_32
#endif

#define LZR_MIN(x, y) (((x) < (y)) ? (x) : (y))
#define LZR_MAX(x, y) (((x) > (y)) ? (x) : (y))
#define LZR_ALIGN(x, y) ((x) + ((y) - ((x) % (y))))
#define LZR_NEXTPOW2(x) (1 << ((sizeof(x) * 8) - __builtin_clzll((x) - 1)))

#define LZR_LIKELY(expr) __builtin_expect(!!(expr), 1)
#define LZR_UNLIKELY(expr) __builtin_expect(!!(expr), 0)
#define LZR_INLINE inline __attribute__((always_inline))

#endif // _LZR_SYSTEM_H