#ifndef LZR_SYSTEM_H
#define LZR_SYSTEM_H

#include <lazer.h>
#include <assert.h>

#if defined(_WIN32) || defined(WIN32)
    #define LZR_WINDOWS
    #define _WIN32_NT 0x0600
    #define _WIN32_LEAN_AND_MEAN
#elif defined(__linux__)
    #define LZR_LINUX
    #define _GNU_SOURCE
#else
    #error "Operating system not supported"
#endif

#endif // LZR_SYSTEM_H