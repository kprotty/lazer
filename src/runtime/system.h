#ifndef LZR_SYSTEM_H
#define LZR_SYSTEM_H

#include <lazer.h>
#include <assert.h>

#if defined(_WIN32) || defined(__WIN32__) || defined(_WIN64)
    #define LZR_WINDOWS
    #define _WIN32_LEAN_AND_MEAN
#elif defined(__linux__)
    #define LZR_LINUX
    #define _GNU_SOURCE
#else
    #error "Operating system not supported"
#endif

#if defined(_MSC_VER) && !defined(__clang__)
    #error "MSVC is frowned upon in these parts"
#endif

#if defined(__x86_64__) || defined(_M_AMD64) || defined(_M_X64)
    #define LZR_X86
#elif defined(__arm64__) || defined(__aarch64__) || defined(_M_ARM64)
    #define LZR_ARM
#else
    #error "Architecture not supported"
#endif

#endif // LZR_SYSTEM_H