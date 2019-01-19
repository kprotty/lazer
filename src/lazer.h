#ifndef _LAZER_H
#define _LAZER_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

typedef union {
    double f64;
    uint64_t u64;
} Term;

typedef uint32_t Hash;

#define TERM_TYPE_HEADER(tag) ((0xfff8ULL | (tag)) << 48)
#define TERM_TYPE_NUMBER    TERM_TYPE_HEADER(0)
#define TERM_TYPE_MAP       TERM_TYPE_HEADER(1)
#define TERM_TYPE_ATOM      TERM_TYPE_HEADER(2)
#define TERM_TYPE_LIST      TERM_TYPE_HEADER(3)
#define TERM_TYPE_DATA      TERM_TYPE_HEADER(4)
#define TERM_TYPE_TUPLE     TERM_TYPE_HEADER(5)
#define TERM_TYPE_BINARY    TERM_TYPE_HEADER(6)
#define TERM_TYPE_FUNCTION  TERM_TYPE_HEADER(7)

#define GetTermType(term) \
    (((term).u64 <= TERM_TYPE_NUMBER) ? \
    TERM_TYPE_NUMBER : ((term).u64 & TERM_TYPE_HEADER(7)))
#define IsNumber(term) (GetTermType(term) == TERM_TYPE_NUMBER)
#define IsMap(term) (GetTermType(term) == TERM_TYPE_MAP)
#define IsAtom(term) (GetTermType(term) == TERM_TYPE_ATOM)
#define IsList(term) (GetTermType(term) == TERM_TYPE_LIST)
#define IsData(term) (GetTermType(term) == TERM_TYPE_DATA)
#define IsTuple(term) (GetTermType(term) == TERM_TYPE_TUPLE)
#define IsBinary(term) (GetTermType(term) == TERM_TYPE_BINARY)
#define IsFunction(term) (GetTermType(term) == TERM_TYPE_FUNCTION)

#endif // _LAZER_H