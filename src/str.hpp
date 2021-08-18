#ifndef __STR_H__
#define __STR_H__

#include "prelude.hpp"

struct String {
    const char* chars;
    u32         len;
};

#define TO_STR(literal)      \
    ((String){               \
        literal,             \
        sizeof(literal) - 1, \
    })

#define FMT_STR "%.*s"

#define ARG_STR(string) string.len, string.chars

#define PRINT_STR(stream, string) fprintf(stream, FMT_STR, ARG_STR(string))

static bool operator==(String a, String b) {
    return (a.len == b.len) && (!memcmp(a.chars, b.chars, a.len));
}

#endif
