#include <ctype.h>
#include <regex.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

/* clang-format off */
#include "./utils.h"
#include "./memory.h"
#include "./minifiers.h"
#include "./template_engine.h"
/* clang-format on */

typedef enum { TAG_OPENING, TAG_CLOSING, TAG_SELFCLOSING } TagType;

typedef enum {
    TOKEN_EOF = 0,

    TOKEN_COMPONENT_DEFINITION, // x-component-def
    TOKEN_COMPONENT_IMPORT,     // x-component
    TOKEN_SLOT_DEFINITION,      // x-slot
    TOKEN_SLOT_INSERT,          // x-insert
    TOKEN_VAL,                  // x-val

    TOKEN_ATTRIBUTE_NAME,  // for example "name" in <x-component-def name"layout" />
    TOKEN_ATTRIBUTE_VALUE, // for example "layout" in <x-component-def name"layout" />

    TOKEN_PLACEHOLDER, // for example "%product_image%" <img src="%product_image%" /> or "%attributes%" in <button %attributes%>...</button>

    TOKEN_INVALID
} TokenType;

typedef struct {
    TokenType token_type;
    TagType tag_type;
    String string;
} Token;

typedef struct {
    String file_path;

    String content;
    size_t cursor;

    u32 line_number;
    char *bol; // beginning of line
} Lexer;

char *peek(Lexer *lexer) { return &lexer->content.data[lexer->cursor]; }

TokenType get_token_type(String str) {
    if (strncmp("component-def", str.data, str.length) == 0) {
        return TOKEN_COMPONENT_DEFINITION;
    } else if (strncmp("component", str.data, str.length) == 0) {
        return TOKEN_COMPONENT_IMPORT;
    } else if (strncmp("slot", str.data, str.length) == 0) {
        return TOKEN_SLOT_DEFINITION;
    } else if (strncmp("insert", str.data, str.length) == 0) {
        return TOKEN_SLOT_INSERT;
    } else if (strncmp("val", str.data, str.length) == 0) {
        return TOKEN_VAL;
    } else {
        return TOKEN_INVALID;
    }
}

Token get_next_token(Lexer *lexer) {
    Token token = {0};
    while (lexer->cursor < lexer->content.length) {
        char *c = peek(lexer);

        if (*c == 'x' && *(c + 1) == '-') {
            if (*(c - 1) == '<') { // opening tag
                char *tag_type = c + 2;

                String html_tag_type = {0};
                html_tag_type.data = tag_type;
                while (*tag_type != '\0' && !isspace(*tag_type)) {
                    html_tag_type.length += 1;
                    tag_type += 1;
                }

                // check if tag is self closing
                char *p = html_tag_type.data;
                while (*p != '\0') {
                    if (*p == '>') {
                        token.tag_type = TAG_OPENING;
                        break;
                    }

                    if (*p == '/' && *(p + 1) == '>') {
                        token.tag_type = TAG_SELFCLOSING;
                        break;
                    }

                    p++;
                }

                token.token_type = get_token_type(html_tag_type);
                token.string = html_tag_type;

                lexer->cursor = (token.string.data + token.string.length) - lexer->content.data;

                return token;
            }

            if (*(c - 1) == '/' && *(c - 2) == '<') { // closing tag
                char *tag_type = c + 2;

                String html_tag_type = {0};
                html_tag_type.data = tag_type;
                while (*tag_type != '\0' && !isspace(*tag_type) && *tag_type != '>') {
                    html_tag_type.length += 1;
                    tag_type += 1;
                }

                token.tag_type = TAG_CLOSING;
                token.token_type = get_token_type(html_tag_type);
                token.string = html_tag_type;

                lexer->cursor = (token.string.data + token.string.length) - lexer->content.data;

                return token;
            }
        }

        if (*c == '\n') {
            lexer->line_number += 1;
            lexer->bol = &lexer->content.data[lexer->cursor + 1];
        }

        lexer->cursor += 1;
    }

    return token;
}

char *find_next_line(char *p, char *text_end) {
    while (p < text_end) {
        if (*p == '\n') {
            return ++p;
        }
        p++;
    }

    return NULL;
}

char *find_previous_line(char *p, char *text_start) {
    while (p > text_start) {
        if (*(p - 1) == '\n') {
            return p;
        }

        p--;
    }

    return NULL;
}

void print_invalid_token_error(Lexer *lexer, Token token) {
    int i;

    printf("Invalid opening tag at:\n");
    printf("    file: %.*s\n", (int)lexer->file_path.length, lexer->file_path.data);
    printf("    line: %d\n", lexer->line_number);
    printf("\n");
    if (lexer->line_number > 1) {
        char *prev_line = find_previous_line(lexer->bol - 1, lexer->content.data);
        int line_length = (lexer->bol - 1) - prev_line;
        printf("    %d|  %.*s\n", lexer->line_number - 1, line_length, prev_line);
    }

    printf("    %d|  ", lexer->line_number);
    char *next_line_start = find_next_line(lexer->bol, lexer->content.data + lexer->content.length);
    int end;
    if (next_line_start == NULL) {
        end = (lexer->content.data + lexer->content.length) - lexer->bol;
    } else {
        end = (next_line_start - 1) - lexer->bol;
    }

    int highlight_start = token.string.data - lexer->bol;
    int highlight_end = (token.string.data + token.string.length) - lexer->bol;
    for (i = 0; i < end; i++) {
        if (i >= highlight_start && i < highlight_end) {
            // ANSI escape codes for highlighting (red background)
            printf("\033[41m%c\033[0m", lexer->bol[i]);
        } else {
            printf("%c", lexer->bol[i]);
        }
    }
    printf("\n");

    if (next_line_start != NULL) {
        char *next_line_end = find_next_line(next_line_start, lexer->content.data + lexer->content.length);
        if (next_line_end == NULL) {
            next_line_end = lexer->content.data + lexer->content.length;
        }

        int next_line_length = next_line_end - next_line_start;
        printf("    %d|  %.*s\n", lexer->line_number + 1, next_line_length, next_line_start);
    }
}

void build_html_components(Memory *memory, Memory *scratch_memory, AssetList asset_list) {
    size_t i;
    for (i = 0; i < asset_list.count; i++) {
        Lexer lexer = {0};

        // init lexer
        lexer.file_path = asset_list.asset_list[i];
        lexer.content = asset_list.asset_list_content[i];
        lexer.bol = lexer.content.data;
        lexer.line_number = 1;

        Token token = get_next_token(&lexer);
        while (token.token_type != TOKEN_EOF) {
            if (token.tag_type == TAG_SELFCLOSING) {
                if (token.token_type == TOKEN_INVALID) {
                    print_invalid_token_error(&lexer, token);
                }

                printf("%.*s(selfclosing)\n", (int)token.string.length, token.string.data);
            }

            if (token.tag_type == TAG_OPENING) {
                if (token.token_type == TOKEN_INVALID) {
                    print_invalid_token_error(&lexer, token);
                }

                printf("%.*s(opening)\n", (int)token.string.length, token.string.data);
            }

            if (token.tag_type == TAG_CLOSING) {
                if (token.token_type == TOKEN_INVALID) {
                    print_invalid_token_error(&lexer, token);
                }

                printf("%.*s(closing)\n", (int)token.string.length, token.string.data);
            }

            token = get_next_token(&lexer);
        }
    }

    return;
}

size_t render_val(char *template, char *val_name, char *value) {
    size_t r = 0;
    return r;
}
size_t replace_val(char *template, char *val_name, char *value) {
    size_t r = 0;
    return r;
}