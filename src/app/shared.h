#ifndef SHARED_H
#define SHARED_H

#define PATH_MAX 4096

#define KB(value) ((value) * 1024)
#define PAGE_SIZE KB(4)

#define true 1
#define false 0

#define NULL ((void *)0)

#define array_count(array) (sizeof(array) / sizeof((array)[0]))

typedef signed char s8;
typedef unsigned char u8;
typedef short s16;
typedef unsigned short u16;
typedef int s32;
typedef unsigned int u32;
typedef unsigned long long u64;

typedef float f32;
typedef double f64;

typedef u8 boolean;

typedef struct {
    char *start_addr;
    char *end_addr;
    u8 count;
} CharsBlock;

typedef CharsBlock Dict;        /** { 'k', 'e', 'y', '\0', 'v', 'a', 'l', 'u', 'e', '\0' ... } */
typedef CharsBlock StringArray; /** { 'm', 'o', 'r', 'n', 'i', 'n', 'g', '\0', 'b', 'u', 'e', 'n', 'o', 's', ' ', 'd', 'i', 'a', 's', '\0' ...} */

typedef struct {
    size_t length;
    Dict **dicts;
} DictArray;

typedef struct {
    char *data;
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

typedef struct {
    char *key;
    size_t key_length;
    char *value;
    size_t value_length;
} KeyValue;

typedef struct {
    KeyValue *items;
    u8 count;
} KeyValueArray;

typedef struct {
    char *start;
    void *end;
} Assets;

typedef struct {
    String *locations;
    String *contents;
    u32 count;
} AssetSOA;

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