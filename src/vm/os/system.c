#include "system.h"
#include <stdio.h>
#include <stdlib.h>

int lzr_assert_failed(const char* expr, const char* file, int line) {
    fprintf(stderr, "%s:%d: %s\n", file, line, expr);
    exit(1);
    return 1;
}