#ifndef _LZR_MODUULE_H
#define _LZR_MODUULE_H

#include "atom.h"
#include "qword.h"

typedef struct Module Module;
typedef struct Function Function;

struct Module {
    Atom* name;
    Function* exports;
    union {
        void* handle;
        size_t refcount;
    };
};

struct Function {
    Function* next;
    Module* module;
    struct {
        uint16_t arity;
        unsigned ptr: 48;
    } code;
    struct {
        uint16_t stack;
        unsigned ptr: 48;
    } atom;
} Function;



#endif // _LZR_MODULE_H