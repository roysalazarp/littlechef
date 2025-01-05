#include <ctype.h>
#include <string.h>

/* clang-format off */
#include "./shared.h"
#include "./memory.h"
/* clang-format on */

/**
 * Checks the file path's extension and returns the corresponding content type.
 * For a list of supported extensions, refer to the function implementation.
 */
char *file_content_type(Memory *memory, const char *path) {
    const char *path_end = path + strlen(path);

    char *content_type = NULL;

    while (path < path_end) {
        if (strncmp(path_end, ".css", strlen(".css")) == 0) {
            char type[] = "text/css";
            content_type = (char *)memory_alloc(memory, sizeof(type));
            memcpy(content_type, &type, sizeof(type));
            break;
        }

        if (strncmp(path_end, ".js", strlen(".js")) == 0) {
            char type[] = "text/javascript";
            content_type = (char *)memory_alloc(memory, sizeof(type));
            memcpy(content_type, &type, sizeof(type));
            break;
        }

        if (strncmp(path_end, ".json", strlen(".json")) == 0) {
            char type[] = "application/json";
            content_type = (char *)memory_alloc(memory, sizeof(type));
            memcpy(content_type, &type, sizeof(type));
            break;
        }

        path_end--;
    }

    return content_type;
}

String find_http_cookie_value(const char *key, String cookies) {
    String value = {0};

    if (!cookies.start_addr && cookies.length == 0) {
        return value;
    }

    char *ptr = cookies.start_addr;
    char *cookies_end = cookies.start_addr + cookies.length;

    while (ptr < cookies_end) {
        if (strncmp(key, ptr, strlen(key)) == 0) {
            ptr += strlen(key);

            while (isspace(*ptr)) {
                if (ptr == cookies_end) {
                    ASSERT(0);
                }

                ptr++;
            }

            ASSERT(*ptr == '=');
            ptr++; /** skip '=' */

            while (isspace(*ptr)) {
                if (ptr == cookies_end) {
                    ASSERT(0);
                }

                ptr++;
            }

            value.start_addr = ptr;

            while (*ptr != '\0' && !isspace(*ptr) && *ptr != ';' && strncmp(ptr, "\r\n", strlen("\r\n")) != 0) {
                ptr++;
            }

            value.length = ptr - value.start_addr;

            return value;
        }

        ptr++;
    }

    return value;
}

/**
 * Scans the `request` for the `\r\n\r\n` sequence that separates headers from the body
 * and returns a pointer to the body start. Return Pointer to the start of the body or
 * NULL if no body is found.
 */
String find_body(const char *request) {
    String body = {0};
    char *ptr = (char *)request;

    char request_headers_end[] = "\r\n\r\n";

    char *request_end = (char *)request + strlen(request);
    while (ptr < request_end) {
        if (strncmp(ptr, request_headers_end, strlen(request_headers_end)) == 0) {
            ptr += strlen(request_headers_end);

            body.start_addr = ptr;
            body.length = strlen(ptr);

            return body;
        }

        ptr++;
    }

    return body;
}

char hex_to_char(unsigned char c) {
    if (c >= '0' && c <= '9') {
        return c - '0';
    }

    if (c >= 'a' && c <= 'f') {
        return c - 'a' + 10;
    }

    if (c >= 'A' && c <= 'F') {
        return c - 'A' + 10;
    }

    return -1;
}

/**
 * Decodes a URL-encoded UTF-8 string in place,
 */
size_t url_decode_utf8(char **string, size_t length) {
    char *out = *string;

    size_t new_len = 0;

    size_t i;
    for (i = 0; i < length; i++) {
        if ((*string)[i] == '%' && i + 2 < length && isxdigit((*string)[i + 1]) && isxdigit((*string)[i + 2])) {
            char c = hex_to_char((*string)[i + 1]) * 16 + hex_to_char((*string)[i + 2]);
            *out++ = c;
            i += 2;
        } else if ((*string)[i] == '+') {
            *out++ = ' ';
        } else {
            *out++ = (*string)[i];
        }

        new_len++;
    }

    memset(out, 0, length - new_len);

    return new_len;
}

/**
 * Parses and decodes URL query or request body parameters into a dictionary.
 */
Dict parse_and_decode_params(Memory *memory, String raw_params) {
    Dict key_value = {0};
    char *p = NULL;

    if (raw_params.length == 0) {
        return key_value;
    }

    char *tmp = raw_params.start_addr;
    char *raw_params_end = raw_params.start_addr + raw_params.length;

    if (*tmp == '?') {
        /** skip '?' at the beginning of query params string */
        tmp++;
    }

    key_value.start_addr = p = (char *)memory_in_use(memory);
    while (tmp < raw_params_end) {
        char *key = tmp;
        char *key_end = key;
        while (*key_end != '=') {
            key_end++;
        }

        size_t key_length = key_end - key;
        memcpy(p, key, key_length);
        p += key_length;
        *p = '\0';
        p++;

        /** TODO: Check if it is possible that query param does not have a value? 🤔 */

        char *val = key_end + 1; /** +1 to skip '=' */
        char *val_end = val;
        while (*val_end != '&' && !isspace(*val_end) && *val_end != '\0') {
            val_end++;
        }

        size_t val_length = val_end - val;
        memcpy(p, val, val_length);
        size_t new_val_length = url_decode_utf8(&p, val_length);
        p += new_val_length;
        *p = '\0';
        p++;

        tmp = val_end + 1; /** +1 to skip possible '&' which marks the end of query (or body) param key-value and beginning of new one */
    }
    key_value.end_addr = p - 1;
    memory_out_of_use(memory, p);

    return key_value;
}

char char_to_hex(unsigned char nibble) {
    if (nibble < 10) {
        return '0' + nibble;
    }

    if (nibble < 16) {
        return 'A' + (nibble - 10);
    }

    ASSERT(0);

    return 0;
}

/* URL encode function */
size_t url_encode_utf8(char **string, size_t length) {
    char *in = *string;
    static char buffer[1024];
    char *out = buffer;

    size_t new_len = 0;

    size_t i;
    for (i = 0; i < length; i++) {
        unsigned char c = (unsigned char)in[i];

        /* Encode non-alphanumeric characters and special symbols */
        if (!isalnum(c) && c != '-' && c != '_' && c != '.' && c != '~') {
            *out++ = '%';
            *out++ = char_to_hex((c >> 4) & 0xF);
            *out++ = char_to_hex(c & 0xF);
            new_len += 3;
        } else if (c == ' ') {
            *out++ = '+';
            new_len++;
        } else {
            *out++ = c;
            new_len++;
        }
    }

    *out = '\0'; /* Null-terminate the encoded string */

    /* Copy the encoded string back to the input buffer if it fits */
    if (new_len < sizeof(buffer)) {
        strncpy(*string, buffer, new_len + 1);
    } else {
        /* Handle the case where the buffer is insufficient */
        return 0; /* Signal an error or handle it in another way */
    }

    return new_len;
}