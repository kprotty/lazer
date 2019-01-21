#ifndef _LZR_MODUULE_H
#define _LZR_MODUULE_H

#include "atom.h"

typedef struct Module Module;
typedef struct Function {
    struct Function* next;
    Module* module;
    void* code;
    Atom* name;
} Function;

struct Module {
    Atom* name;
    Function* exports;
    union {
        void* handle;
        size_t refcount;
    };
};

#endif // _LZR_MODULE_H