#include <stdarg.h>
#include <stdio.h>
#include <string.h>

/* clang-format off */
#include "./utils.h"
#include "../../app/shared.h"
/* clang-format on */

u8 get_string_array_length(StringArray array) {
    size_t array_length = 0;

    char *ptr = array.start_addr;
    while (ptr < array.end_addr) {
        ptr += strlen(ptr) + 1;
        array_length++;
    }

    return array_length;
}

char *get_string_at(StringArray array, u8 pos) {
    u8 array_length = get_string_array_length(array);

    if (pos > array_length) {
        printf("You requested to get a string at pos %d, but string array only contains %d elements", pos, array_length);
        ASSERT(0);
    }

    char *ptr = array.start_addr;

    u8 i;
    for (i = 0; i < array_length; i++) {
        if (i == pos) {
            break;
        }

        ptr += strlen(ptr) + 1;
    }

    return ptr;
}

char *add_string(char *buffer, String str) {
    strncpy(buffer, str.data, str.length + 1);
    return buffer + strlen(buffer) + 1;
}

KV get_key_value(Dict dict, u8 pos) {
    KV kv = {0};

    if (pos > dict.count - 1) {
        printf("You requested to get key-value at pos %d, but dict only contains %d elements", pos, dict.count);
        ASSERT(0);
    }

    char *ptr = dict.start_addr;

    u8 i;
    for (i = 0; i < dict.count; i++) {
        if (i == pos) {
            char *key = ptr;
            ptr += strlen(ptr) + 1; /* pass key */

            char *value = ptr;

            kv.k = key;
            kv.v = value;

            break;
        }

        ptr += strlen(ptr) + 1; /* pass key */
        ptr += strlen(ptr) + 1; /* pass value */
    }

    return kv;
}

/**
 * Finds the value associated with a given key in a dictionary.
 */
char *find_value(const char key[], Dict dict) {
    char *ptr = dict.start_addr;
    while (ptr < dict.end_addr) {
        /** Include null-terminator (+ 1) because key is a null-terminated string */
        if (strncmp(ptr, key, strlen(key) + 1) == 0) {
            ptr += strlen(ptr) + 1; /* Advance past key */
            return (ptr);
        }

        ptr += strlen(ptr) + 1; /* Advance past key */
        ptr += strlen(ptr) + 1; /* Advance past value */
    }

    return NULL;
}

void clear_leftovers(char *ptr) {
    if (*ptr == '\0') {
        ptr++;
    }

    while (*ptr) {
        size_t str_len = strlen(ptr);
        memset(ptr, 0, str_len);
        ptr += str_len + 1;
    }
}

char *copy_string(Memory *memory, const char *str) {
    char *buffer = (char *)memory_alloc(memory, strlen(str) + 1);
    memcpy(buffer, str, strlen(str));

    return buffer;
}

boolean is_html_path(String path) {
    if (strncmp(path.data + path.length - strlen(".html"), ".html", strlen(".html")) == 0) {
        return true;
    }

    return false;
}