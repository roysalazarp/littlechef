#ifndef UTILS_H
#define UTILS_H

#include "./shared.h"

u8 get_string_array_length(StringArray array);
char *add_string(char *buffer, String str);
char *get_string_at(StringArray array, u8 pos);
u8 get_dictionary_size(Dict dict);
KV get_key_value(Dict dict, u8 pos);
char *add_string(char *buffer, String str);
char *find_value(const char key[], Dict dict);

#endif /* UTILS_H */