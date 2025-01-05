#ifndef SHARED_H
#define SHARED_H

#define PATH_MAX 4096

#define KB(value) ((value) * 1024)
#define PAGE_SIZE KB(4)

#define true 1
#define false 0

#define NULL ((void *)0)

typedef unsigned char uint8_t;
typedef uint8_t boolean;
typedef long unsigned int size_t;

typedef struct {
    char *start_addr;
    char *end_addr;
} CharsBlock;

typedef CharsBlock Dict;        /** { 'k', 'e', 'y', '\0', 'v', 'a', 'l', 'u', 'e', '\0' ... } */
typedef CharsBlock StringArray; /** { 'm', 'o', 'r', 'n', 'i', 'n', 'g', '\0', 'b', 'u', 'e', 'n', 'o', 's', ' ', 'd', 'i', 'a', 's', '\0' ...} */

typedef struct {
    size_t length;
    Dict **dicts;
} DictArray;

typedef struct {
    char *start_addr;
    size_t length;
} String;

typedef struct {
    size_t length;
    char *content;
} Response;

typedef struct {
    char *k;
    char *v;
} KV;

/* clang-format off */
#ifdef DEBUG
#define ASSERT(condition) \
    do { \
        if (!(condition)) { \
            volatile int* crash = (void*)0; \
            *crash = 0; \
        } \
    } while(0)
#else
#define ASSERT(condition)
#endif
/* clang-format on */

#endif /* SHARED_H */
