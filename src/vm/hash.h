#ifndef _LZR_HASH_H
#define _LZR_HASH_H

#include "system.h"

Hash HashNumber(uint64_t number);

Hash HashBytes(const uint8_t* bytes, size_t len);

#endif // _LZR_HASH_H