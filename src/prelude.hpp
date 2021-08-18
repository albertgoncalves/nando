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

#define ERROR_WITH(x)                                                        \
    {                                                                        \
        fprintf(stderr, "%s:%s:%d `%s`\n", __FILE__, __func__, __LINE__, x); \
        exit(EXIT_FAILURE);                                                  \
    }

#define EXIT_IF(condition)      \
    if (condition) {            \
        ERROR_WITH(#condition); \
    }

#endif
