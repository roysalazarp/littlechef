#include <ctype.h>
#include <string.h>

/* clang-format off */
#include "./shared.h"
/* clang-format on */

void js_minify(char *content) {
    char *dest = content;
    char *src = content;
    boolean inside_string = false;
    boolean last_was_space = false;

    while (*src != '\0') {
        /* Check for string literals (inside " or ') */
        if (*src == '\"' || *src == '\'') {
            if (inside_string) {
                if (*(src - 1) != '\\') {
                    inside_string = false; /* Exit string literal */
                }
            } else {
                inside_string = true; /* Enter string literal */
            }
        }

        if (inside_string) {
            /* Copy everything inside a string as-is */
            *dest++ = *src;
        } else {
            /* Outside of string literals, manage spaces */
            if (isspace(*src)) {
                if (!last_was_space) {
                    *dest++ = ' '; /* Replace sequence of spaces with one space */
                    last_was_space = true;
                }
            } else {
                /* Non-space character resets the space tracking */
                *dest++ = *src;
                last_was_space = false;
            }
        }

        src++;
    }

    /* Null-terminate the result */
    *dest = '\0';
}

/**
 * A simple HTML minifier that compresses the given HTML content and stores the
 * minified result in the provided buffer. It returns the size of the minified HTML.
 */
size_t html_minify(char *buffer, char *html, size_t html_length) {
    char *start = buffer;

    char *html_end = html + html_length;

    u8 skip_whitespace = 0;
    while (html < html_end) {
        /** IMPORTANT: Commented the following code because it
         * contains a bug that causes weird duplicated element
         * in the html */
        /** TODO: FIX, IMPORTANT */

        /*
        if (strlen(start) == 0 && isspace(*html)) {
            skip_whitespace = 1;
            html++;
            continue;
        }

        if (*html == '>') {
            char *temp = html - 1;
            if (isspace(*temp) && !skip_whitespace) {
                u8 i = 0;
                while (*temp) {
                    if (!isspace(*temp)) {
                        skip_whitespace = 1;
                        buffer -= i - 1;
                        break;
                    }

                    temp -= 1;
                    i++;
                }

                continue;
            }

            skip_whitespace = 1;
            goto copy_char;
        }

        if (*html == '<') {
            char *temp = html - 1;
            if (isspace(*temp) && !skip_whitespace) {
                u8 i = 0;
                while (*temp) {
                    if (!isspace(*temp)) {
                        skip_whitespace = 1;
                        buffer -= i - 1;
                        break;
                    }

                    temp -= 1;
                    i++;
                }

                continue;
            }

            skip_whitespace = 0;
            goto copy_char;
        }

        if (!skip_whitespace && *html == '\n') {
            html++;
            continue;
        }

        if (skip_whitespace && isspace(*html)) {
            html++;
            continue;
        }

        if (skip_whitespace && !isspace(*html)) {
            skip_whitespace = 0;
            goto copy_char;
        }

        copy_char:
        */

        *buffer = *html;
        buffer++;

        html++;
    }

    buffer[0] = '\0';
    buffer++;

    size_t length = buffer - start;

    return length;
}