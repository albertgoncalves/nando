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

#ifdef DEBUG

static void print(File* stream, String string) {
    fprintf(stream, "%.*s", string.len, string.chars);
}

#endif

static bool operator==(String a, String b) {
    return (a.len == b.len) && (!memcmp(a.chars, b.chars, a.len));
}

#endif
