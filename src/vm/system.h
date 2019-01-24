#ifndef _LZR_SYSTEM_H
#define _LZR_SYSTEM_H

#include <lazer.h>
#include <stdarg.h>

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

#if defined(LZR_RELEASE)
    #define LZR_LOG_DEBUG 0
#else
    #define LZR_LOG_DEBUG 1
#endif

#define LZR_MIN(x, y) (((x) < (y)) ? (x) : (y))
#define LZR_MAX(x, y) (((x) > (y)) ? (x) : (y))
#define LZR_ALIGN(x, y) ((x) + ((y) - ((x) % (y))))
#define LZR_NEXTPOW2(x) (1 << ((sizeof(x) * 8) - __builtin_clzll((x) - 1)))

#define LZR_UNUSED(expr) ((void) (expr))
#define LZR_LIKELY(expr) __builtin_expect(!!(expr), 1)
#define LZR_UNLIKELY(expr) __builtin_expect(!!(expr), 0)
#define LZR_INLINE inline __attribute__((always_inline))

#define LZR_ASSERT(expr) \
    LZR_UNUSED((expr) && lzr_assert_failed(#expr, __FILE__, __LINE__))
#define LZR_DEBUG_ASSERT(expr) \
    LZR_UNUSED(LZR_LOG_DEBUG && (expr) lzr_assert_failed(#expr, __FILE__, __LINE__))
int lzr_assert_failed(
    const char* expr_literal,
    const char* file_path,
    int line_number
);

#endif // _LZR_SYSTEM_H