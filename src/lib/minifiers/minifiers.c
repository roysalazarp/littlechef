#include <ctype.h>
#include <string.h>

/* clang-format off */
#include "../utils/utils.h"
#include "../../app/shared.h"
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
    clear_leftovers(content + strlen(content));
}

void html_minify(char *content) {
    char *dest = content;
    char *src = content;
    boolean inside_tag = false;
    boolean last_was_space = false;

    while (*src != '\0') {
        if (*src == '<') {
            inside_tag = true;
            *dest++ = *src;
            last_was_space = false; /* Reset space tracking at tag start */
        } else if (*src == '>') {
            inside_tag = false;
            *dest++ = *src;
            last_was_space = false; /* Reset space tracking at tag end */
        } else if (inside_tag) {
            /* Inside tags, keep spaces, but reduce multiple to one. */
            if (isspace(*src)) {
                if (!last_was_space) {
                    *dest++ = ' ';
                    last_was_space = true;
                }
            } else {
                *dest++ = *src;
                last_was_space = false;
            }
        } else {
            /* Outside of tags, remove unnecessary spaces and newlines */
            if (isspace(*src)) {
                if (!last_was_space) {
                    *dest++ = ' ';
                    last_was_space = true;
                }
            } else {
                *dest++ = *src;
                last_was_space = false;
            }
        }
        src++;
    }

    *dest = '\0';
    clear_leftovers(content + strlen(content));
}