#ifndef HTTP_UTILS_H
#define HTTP_UTILS_H

#include "./shared.h"

String find_http_request_value(const char key[], char *request);

#endif /* HTTP_UTILS_H */