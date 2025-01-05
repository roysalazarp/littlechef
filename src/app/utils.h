#ifndef UTILS_H
#define UTILS_H

#include "./shared.h"

uint8_t get_string_array_length(StringArray array);
char *add_string(char *buffer, String str);
char *get_string_at(StringArray array, uint8_t pos);
uint8_t get_dictionary_size(Dict dict);
KV get_key_value(Dict dict, uint8_t pos);
char *add_string(char *buffer, String str);
char *find_value(const char key[], Dict dict);

#endif /* UTILS_H */