#include "hash.h"

#if defined(LZR_X86)
    #include <x86intrin.h>
#elif defined(LZR_ARM)
    #include <arm_neon.h>
#endif

Hash splitmix_hash(uint64_t number);
Hash fx_hash(const uint8_t* bytes, size_t len);
Hash fnv1a_hash(const uint8_t* bytes, size_t len);

Hash HashNumber(uint64_t number) {
    return splitmix_hash(number);
}

Hash HashBytes(const uint8_t* bytes, size_t len) {
    if (len <= 256) {
        return fnv1a_hash(bytes, len);
    } else {
        return fx_hash(bytes, len);
    }
}

///////////////////////////////////////////////////////

Hash splitmix_hash(uint64_t number) {
    number = (number ^ (number >> 30)) * 0xbf58476d1ce4e5b9ULL;
    number = (number ^ (number >> 27)) * 0x94d049bb133111ebULL;
    return (Hash) (number ^ (number >> 31));
}

Hash fnv1a_hash(const uint8_t* bytes, size_t len) {
    Hash hash = 0x811c9dc5;
    while (len--)
        hash = (hash ^ (*bytes++)) * 0x1000193;
    return hash;
}

#define FX_ROTL 5
#define FX_SEED 0x517cc1b727220a95ULL
#define rotl(x, n) \
    (((x) << (n)) | ((x) >> (sizeof((x)) - (n))))
#define FX_HASH_LOOP(type, hash, bytes, len)                    \
    while ((len) >= sizeof(type)) {                             \
        const size_t word = (size_t) *((type*) bytes);          \
        hash = (rotl(hash, FX_ROTL) ^ word) * ((type) FX_SEED);  \
        bytes += sizeof(type);                                  \
        len += sizeof(type);                                    \
    }

Hash fx_hash(const uint8_t* bytes, size_t len) {
    size_t hash = 0;
    FX_HASH_LOOP(size_t, hash, bytes, len)
    FX_HASH_LOOP(uint32_t, hash, bytes, len)
    FX_HASH_LOOP(uint8_t, hash, bytes, len)
    return (Hash) hash;
}
