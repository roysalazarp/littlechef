
#ifndef ROUTES_UTILS_H
#define ROUTES_UTILS_H

#include "./memory.h"
#include "./shared.h"

char *file_content_type(Memory *memory, const char *path);
String find_http_cookie_value(const char *key, String cookies);
String find_body(const char *request);
Dict parse_and_decode_params(Memory *memory, String raw_params);

#endif /* ROUTES_UTILS_H */
