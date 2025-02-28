#include <string.h>

/* clang-format off */
#include "./http_utils.h"
/* clang-format on */

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

        u8 skiped_spaces = 0;
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
