#ifndef UTILS_H
#define UTILS_H

#include "../memory/memory.h"
#include "../../app/shared.h"

u8 get_string_array_length(StringArray array);
char *add_string(char *buffer, String str);
char *get_string_at(StringArray array, u8 pos);
KV get_key_value(Dict dict, u8 pos);
char *add_string(char *buffer, String str);
char *find_value(const char key[], Dict dict);
void clear_leftovers(char *ptr);
char *copy_string(Memory *memory, const char *str);
boolean is_html_path(String path);

#endif /* UTILS_H */