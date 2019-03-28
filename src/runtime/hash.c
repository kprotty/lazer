#include "hash.h"

uint32_t fx_hash(const char* bytes, size_t len);
uint32_t fnv1a_hash(const char* bytes, size_t len);

uint32_t lzr_hash_bytes(const char* bytes, size_t len) {
    if (len >= 512)
        return fx_hash(bytes, len);
    return fnv1a_hash(bytes, len);
}

uint32_t fnv1a_hash(const char* bytes, size_t len) {
    uint32_t hash = 0x811c9dc5;
    while (len--)
        hash = (hash ^ (*bytes++)) * 0x1000193;
    return hash;
}

uint32_t fx_hash(const char* bytes, size_t len) {
    #define fx_rotl(type, value, shift) \
        ((value << shift) | (value >> (sizeof(type) * 8 - shift)))
    #define fx_hash_word(type, word) \
        (fx_rotl(type, hash, 5) ^ (word) * 0x517cc1b727220a95ULL)

    // hash 8 bytes at a time
    size_t hash = 0;
    while (len >= sizeof(size_t)) {
        hash = fx_hash_word(size_t, *((size_t*) bytes));
        bytes += sizeof(size_t);
        len -= sizeof(size_t)
    }

    // hash the remaining bytes
    while (len--)
        hash = fx_hash_word(size_t, (*bytes++));

    return (uint32_t) (hash >> 32);
    #undef fx_rotl
    #undef fx_hash_word
}