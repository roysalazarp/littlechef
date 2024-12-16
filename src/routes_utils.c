#include "headers.h"

/**
 * Locates the value corresponding to a specified key from the given HTTP request string.
 */
String find_http_request_value(const char key[], char *request) {
    String value = {0};

    if (strncmp(key, "METHOD", strlen(key)) == 0) {
        value.start_addr = request;

        char *end = request;
        while (*end != ' ') {
            end++;
        }

        value.length = end - value.start_addr;

        return value;
    }

    if (strncmp(key, "URL", strlen(key)) == 0) {
        char *ptr = request;

        while (*ptr != ' ') {
            ptr++;
        }

        ptr++;

        value.start_addr = ptr;

        char *end = ptr;
        while (*end != '?') {
            if (*end == ' ') {
                break;
            }

            end++;
        }

        value.length = end - value.start_addr;

        return value;
    }

    if (strncmp(key, "QUERY_PARAMS", strlen(key)) == 0) {
        char *ptr = request;

        while (*ptr != '\n') {
            if (*ptr == '?') {
                value.start_addr = ptr;

                break;
            }

            ptr++;
        }

        if (!value.start_addr) {
            return value;
        }

        ptr++;

        char *end = value.start_addr;
        while (*end != ' ') {
            end++;
        }

        value.length = end - value.start_addr;

        return value;
    }

    if (strncmp(key, "PROTOCOL_VERSION", strlen(key)) == 0) {
        char *ptr = request;

        uint8_t skiped_spaces = 0;
        while (*ptr != '\0') {
            if (*ptr == ' ') {
                skiped_spaces++;
            }

            if (skiped_spaces == 2) {
                break;
            }

            ptr++;
        }

        ptr++;

        value.start_addr = ptr;

        char *end_sign = "\r\n";

        char *end = ptr;
        while (*end != '\0') {
            if (strncmp(end, end_sign, strlen(end_sign)) == 0) {
                break;
            }
            end++;
        }

        value.length = end - value.start_addr;

        return value;
    }

    char *ptr = request;
    while (*ptr != '\0') {
        if (strncmp(key, ptr, strlen(key)) == 0) {
            const char str[] = ": ";
            ptr += strlen(key);

            if (strncmp(ptr, str, strlen(str)) == 0) {
                char *start = ptr + strlen(str);
                value.start_addr = start;

                char *end_sign = "\r\n";
                char *end = start;
                while (*end != '\0') {
                    if (strncmp(end, end_sign, strlen(end_sign)) == 0) {
                        break;
                    }

                    end++;
                }

                value.length = end - start;

                return value;
            }
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

/**
 * Searches the `body` for the specified `key` and returns its
 * corresponding value as a `String`.
 */
String find_body_value(const char key[], String body) {
    /** TODO: Check whether key can have no value */
    String value = {0};

    char *ptr = body.start_addr;
    char *body_end = body.start_addr + body.length + 1;

    while (ptr < body_end) {
        if (strncmp(ptr, key, strlen(key)) == 0) {
            char *key_end = ptr + strlen(key);

            if (*key_end == '=') {
                key_end++;
                value.start_addr = key_end;
                break;
            }
        }

        ptr++;
    }

    char *value_end = value.start_addr;
    while (value_end < body_end) {
        if (*value_end == '&' || *value_end == '\0') {
            value.length = value_end - value.start_addr;

            break;
        }

        value_end++;
    }

    return value;
}

/**
 * Checks the file path's extension and returns the corresponding content type.
 * For a list of supported extensions, refer to the function implementation.
 */
char *file_content_type(Arena *scratch_arena, const char *path) {
    const char *path_end = path + strlen(path);

    while (path < path_end) {
        if (strncmp(path_end, ".css", strlen(".css")) == 0) {
            char type[] = "text/css";
            char *content_type = (char *)arena_alloc(scratch_arena, sizeof(type));
            memcpy(content_type, &type, sizeof(type));
            return content_type;
        }

        if (strncmp(path_end, ".js", strlen(".js")) == 0) {
            char type[] = "text/javascript";
            char *content_type = (char *)arena_alloc(scratch_arena, sizeof(type));
            memcpy(content_type, &type, sizeof(type));
            return content_type;
        }

        if (strncmp(path_end, ".json", strlen(".json")) == 0) {
            char type[] = "application/json";
            char *content_type = (char *)arena_alloc(scratch_arena, sizeof(type));
            memcpy(content_type, &type, sizeof(type));
            return content_type;
        }

        path_end--;
    }

    assert(0);
}

char char_to_hex(unsigned char nibble) {
    if (nibble < 10) {
        return '0' + nibble;
    }

    if (nibble < 16) {
        return 'A' + (nibble - 10);
    }

    assert(0);
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
 * TODO: ADD FUNCTION DOCUMENTATION
 */
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
Dict parse_and_decode_params(Arena *scratch_arena, String raw_params) {
    Dict key_value = {0};

    if (raw_params.length == 0) {
        return key_value;
    }

    char *ptr = raw_params.start_addr;
    char *raw_params_end = raw_params.start_addr + raw_params.length;

    char *params_dict = (char *)scratch_arena->current;
    key_value.start_addr = params_dict;

    if (*ptr == '?') {
        /** skip '?' at the beginning of query params string */
        ptr++;
    }

    while (ptr < raw_params_end) {
        char *key = ptr;
        char *key_end = key;
        while (*key_end != '=') {
            key_end++;
        }

        size_t key_length = key_end - key;
        memcpy(params_dict, key, key_length);
        params_dict += key_length;
        *params_dict = '\0';
        params_dict++;

        /** TODO: Check if it is possible that query param does not have a value? 🤔 */

        char *val = key_end + 1; /** +1 to skip '=' */
        char *val_end = val;
        while (*val_end != '&' && !isspace(*val_end) && *val_end != '\0') {
            val_end++;
        }

        size_t val_length = val_end - val;
        memcpy(params_dict, val, val_length);
        size_t new_val_length = url_decode_utf8(&params_dict, val_length);
        params_dict += new_val_length;
        *params_dict = '\0';
        params_dict++;

        ptr = val_end + 1; /** +1 to skip possible '&' which marks the end of query (or body) param key-value and beginning of new one */
    }

    key_value.end_addr = params_dict - 1; /** -1 because we (params_dict++) at the end of query (or body) param value processing */

    scratch_arena->current = params_dict;

    return key_value;
}

String find_cookie_value(const char *key, String cookies) {
    String value = {0};

    if (!cookies.start_addr && cookies.length == 0) {
        return value;
    }

    char *ptr = cookies.start_addr;
    char *cookies_end = cookies.start_addr + cookies.length;

    char *end_sign = "\r\n";

    while (ptr < cookies_end) {
        if (strncmp(key, ptr, strlen(key)) == 0) {
            ptr += strlen(key);

            while (isspace(*ptr)) {
                if (ptr == cookies_end) {
                    assert(0);
                }

                ptr++;
            }

            assert(*ptr == '=');
            ptr++; /** skip '=' */

            while (isspace(*ptr)) {
                if (ptr == cookies_end) {
                    assert(0);
                }

                ptr++;
            }

            value.start_addr = ptr;

            while (*ptr != '\0' && !isspace(*ptr) && strncmp(ptr, end_sign, strlen(end_sign)) != 0) {
                ptr++;
            }

            value.length = ptr - value.start_addr;

            return value;
        }

        ptr++;
    }

    return value;
}
