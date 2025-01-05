#include <stdarg.h>
#include <stdio.h>
#include <string.h>

/* clang-format off */
#include "./utils.h"
#include "./shared.h"
/* clang-format on */

uint8_t get_string_array_length(StringArray array) {
    size_t array_length = 0;

    char *ptr = array.start_addr;
    while (ptr < array.end_addr) {
        ptr += strlen(ptr) + 1;
        array_length++;
    }

    return array_length;
}

char *get_string_at(StringArray array, uint8_t pos) {
    uint8_t array_length = get_string_array_length(array);

    if (pos > array_length) {
        printf("You requested to get a string at pos %d, but string array only contains %d elements", pos, array_length);
        ASSERT(0);
    }

    char *ptr = array.start_addr;

    uint8_t i;
    for (i = 0; i < array_length; i++) {
        if (i == pos) {
            break;
        }

        ptr += strlen(ptr) + 1;
    }

    return ptr;
}

char *add_string(char *buffer, String str) {
    strncpy(buffer, str.start_addr, str.length + 1);
    return buffer + strlen(buffer) + 1;
}

uint8_t get_dictionary_size(Dict dict) {
    size_t size = 0;

    char *ptr = dict.start_addr;
    while (ptr < dict.end_addr) {
        ptr += strlen(ptr) + 1; /* Advance past key */
        ptr += strlen(ptr) + 1; /* Advance past value */
        size++;
    }

    return size;
}

KV get_key_value(Dict dict, uint8_t pos) {
    KV kv = {0};

    uint8_t size = get_dictionary_size(dict);

    if (pos > size) {
        printf("You requested to get key-value at pos %d, but dict only contains %d elements", pos, size);
        ASSERT(0);
    }

    char *ptr = dict.start_addr;

    uint8_t i;
    for (i = 0; i < size; i++) {
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
