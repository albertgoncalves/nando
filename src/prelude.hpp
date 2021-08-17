#ifndef __PRELUDE_H__
#define __PRELUDE_H__

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;

typedef int32_t i32;

typedef FILE File;

#define null nullptr

struct String {
    const char* chars;
    u32         len;
};

template <typename T>
struct Vec2 {
    T x;
    T y;
};

#define ERROR()                                                      \
    {                                                                \
        fprintf(stderr, "%s:%s:%d\n", __FILE__, __func__, __LINE__); \
        exit(EXIT_FAILURE);                                          \
    }

#define EXIT_IF(condition)        \
    if (condition) {              \
        fprintf(stderr,           \
                "%s:%s:%d\n%s\n", \
                __FILE__,         \
                __func__,         \
                __LINE__,         \
                #condition);      \
        exit(EXIT_FAILURE);       \
    }

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
