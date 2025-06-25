#include <string.h>

/* clang-format off */
#include "./memory.h"
#include "./json_parser.h"
/* clang-format on */

typedef enum {
    TOKEN_EOF,

    TOKEN_OPEN_BRACE,
    TOKEN_OPEN_BRACKET,
    TOKEN_CLOSE_BRACE,
    TOKEN_CLOSE_BRACKET,
    TOKEN_COMMA,
    TOKEN_COLON,
    TOKEN_SEMI_COLON,
    TOKEN_STRING_LITERAL,
    TOKEN_NUMBER,
    TOKEN_TRUE,
    TOKEN_FALSE,
    TOKEN_NULL,

    TOKEN_INVALID
} JSONTokenKind;

typedef struct {
    JSONTokenKind kind;
    String string;
} JSONToken;

typedef struct {
    String file_path;
    String content;
    u64 cursor;
    u32 line_number;
} JSONParser;

struct JSONElement {
    String key;
    String value;
    JSONElement *first_child;
    JSONElement *next_sibling;
};

typedef enum { JSON_ARRAY, JSON_OBJECT } JSONElementType;

JSONElement *json_parse_list(Memory *memory, JSONParser *parser, JSONTokenKind closing_token, JSONElementType element_type);
JSONElement *json_parse_element(Memory *memory, JSONParser *parser, String object_key, JSONToken value);

boolean json_is_in_bounds(String content, u64 cursor) {
    boolean result = (cursor < content.length);
    return result;
}

boolean json_is_digit(String content, u64 cursor) {
    ASSERT(json_is_in_bounds(content, cursor));

    char c = content.data[cursor];
    boolean result = ((c >= '0') && (c <= '9'));

    return result;
}

boolean json_is_whitespace(String content, u64 cursor) {
    ASSERT(json_is_in_bounds(content, cursor));

    u8 c = content.data[cursor];
    boolean result = ((c == ' ') || (c == '\t') || (c == '\n') || (c == '\r'));

    return result;
}

boolean json_str_equal(String a, String b) {
    if (a.length != b.length) {
        return false;
    }

    u64 i;
    for (i = 0; i < a.length; ++i) {
        if (a.data[i] != b.data[i]) {
            return false;
        }
    }

    return true;
}

char json_peek(JSONParser *parser) { return parser->content.data[parser->cursor]; }
void json_advance_cursor(JSONParser *parser) { parser->cursor += 1; }

JSONToken json_get_token(JSONParser *parser) {
    JSONToken token = {0};

check_again:;
    if (json_is_in_bounds(parser->content, parser->cursor)) {
        char c = parser->content.data[parser->cursor];

        // skip whitespace
        if ((c == ' ') || (c == '\t') || (c == '\v') || (c == '\f') || (c == '\r')) {
            parser->cursor += 1;

            goto check_again;
        }

        if (c == '\n') {
            parser->line_number += 1;
            parser->cursor += 1;

            goto check_again;
        }

        switch (c) {
            case '{': {
                token.kind = TOKEN_OPEN_BRACE;
                token.string.data = parser->content.data + parser->cursor;
                token.string.length = 1;

                parser->cursor += 1;

                break;
            }
            case '[': {
                token.kind = TOKEN_OPEN_BRACKET;
                token.string.data = parser->content.data + parser->cursor;
                token.string.length = 1;

                parser->cursor += 1;

                break;
            }
            case '}': {
                token.kind = TOKEN_CLOSE_BRACE;
                token.string.data = parser->content.data + parser->cursor;
                token.string.length = 1;

                parser->cursor += 1;

                break;
            }
            case ']': {
                token.kind = TOKEN_CLOSE_BRACKET;
                token.string.data = parser->content.data + parser->cursor;
                token.string.length = 1;

                parser->cursor += 1;

                break;
            }
            case ',': {
                token.kind = TOKEN_COMMA;
                token.string.data = parser->content.data + parser->cursor;
                token.string.length = 1;

                parser->cursor += 1;

                break;
            }
            case ':': {
                token.kind = TOKEN_COLON;
                token.string.data = parser->content.data + parser->cursor;
                token.string.length = 1;

                parser->cursor += 1;

                break;
            }
            case ';': {
                token.kind = TOKEN_SEMI_COLON;
                token.string.data = parser->content.data + parser->cursor;
                token.string.length = 1;

                parser->cursor += 1;

                break;
            }
            case 'f': {
                if (json_is_in_bounds(parser->content, parser->cursor + 5 /* false */)) {
                    if (parser->content.data[parser->cursor] == 'f' && parser->content.data[parser->cursor + 1] == 'a' && parser->content.data[parser->cursor + 2] == 'l' && parser->content.data[parser->cursor + 3] == 's' && parser->content.data[parser->cursor + 4] == 'e') {
                        token.kind = TOKEN_FALSE;
                        token.string.data = parser->content.data + parser->cursor;
                        token.string.length = 5 /* false */;

                        parser->cursor += token.string.length;
                    }
                }

                break;
            }
            case 'n': {
                if (json_is_in_bounds(parser->content, parser->cursor + 4 /* null */)) {
                    if (parser->content.data[parser->cursor] == 'n' && parser->content.data[parser->cursor + 1] == 'u' && parser->content.data[parser->cursor + 2] == 'l' && parser->content.data[parser->cursor + 3] == 'l') {
                        token.kind = TOKEN_NULL;
                        token.string.data = parser->content.data + parser->cursor;
                        token.string.length = 4 /* null */;

                        parser->cursor += token.string.length;
                    }
                }

                break;
            }
            case 't': {
                if (json_is_in_bounds(parser->content, parser->cursor + 4 /* true */)) {
                    if (parser->content.data[parser->cursor] == 't' && parser->content.data[parser->cursor + 1] == 'r' && parser->content.data[parser->cursor + 2] == 'u' && parser->content.data[parser->cursor + 3] == 'e') {
                        token.kind = TOKEN_TRUE;
                        token.string.data = parser->content.data + parser->cursor;
                        token.string.length = 4 /* true */;

                        parser->cursor += token.string.length;
                    }
                }

                break;
            }
            case '"': {
                parser->cursor += 1;
                if (json_is_in_bounds(parser->content, parser->cursor)) {
                    token.kind = TOKEN_STRING_LITERAL;

                    token.string.data = parser->content.data + parser->cursor;
                    while (json_is_in_bounds(parser->content, parser->cursor)) {
                        if (parser->content.data[parser->cursor] == '"') {
                            if (parser->content.data[parser->cursor - 1] == '\\') {
                                // Skip escaped quotation marks
                                parser->cursor += 1;
                            } else {
                                break;
                            }
                        }

                        parser->cursor += 1;
                        token.string.length += 1;
                    }

                    // we should have exited at '"' otherwise we existed early
                    ASSERT(parser->content.data[parser->cursor] == '"');
                    parser->cursor += 1;
                }

                break;
            }
            case '-':
            case '0':
            case '1':
            case '2':
            case '3':
            case '4':
            case '5':
            case '6':
            case '7':
            case '8':
            case '9': {
                token.kind = TOKEN_NUMBER;
                token.string.data = parser->content.data + parser->cursor;

                parser->cursor += 1;

                // Move past a leading negative sign if one exists
                if (json_is_in_bounds(parser->content, parser->cursor) && parser->content.data[parser->cursor] == '-') {
                    parser->cursor += 1;
                }

                // If the leading digit wasn't 0, parse any digits before the decimal point
                if (parser->content.data[parser->cursor] != '0') {
                    while (json_is_in_bounds(parser->content, parser->cursor) && ((parser->content.data[parser->cursor] >= '0') && (parser->content.data[parser->cursor] <= '9'))) {
                        parser->cursor += 1;
                    }
                }

                // If there is a decimal point, parse any digits after the decimal point
                if (parser->content.data[parser->cursor] == '.') {
                    parser->cursor += 1;
                    while (json_is_in_bounds(parser->content, parser->cursor) && ((parser->content.data[parser->cursor] >= '0') && (parser->content.data[parser->cursor] <= '9'))) {
                        parser->cursor += 1;
                    }
                }

                // If it's in scientific notation, parse any digits after the "e"
                if ((parser->content.data[parser->cursor] == 'e') || (parser->content.data[parser->cursor] == 'E')) {
                    parser->cursor += 1;

                    if (json_is_in_bounds(parser->content, parser->cursor) && ((parser->content.data[parser->cursor] == 'e') || (parser->content.data[parser->cursor] == 'E'))) {
                        parser->cursor += 1;
                    }

                    while (json_is_in_bounds(parser->content, parser->cursor) && ((parser->content.data[parser->cursor] >= '0') && (parser->content.data[parser->cursor] <= '9'))) {
                        parser->cursor += 1;
                    }
                }

                token.string.length = (parser->content.data + parser->cursor) - token.string.data;

                break;
            }
            default: {
                token.kind = TOKEN_INVALID;
                token.string.data = parser->content.data + parser->cursor;
                token.string.length = 1;

                ASSERT(0);

                break;
            }
        }
    }

    return token;
}

JSONElement *json_parse_list(Memory *memory, JSONParser *parser, JSONTokenKind closing_token, JSONElementType element_type) {
    JSONElement *first_element = {0};
    JSONElement *last_element = {0};

    while (json_is_in_bounds(parser->content, parser->cursor)) {
        String key = {0};
        JSONToken value = json_get_token(parser);
        if (element_type == JSON_OBJECT) {
            JSONToken object_key = value;
            if (object_key.kind == TOKEN_STRING_LITERAL) {
                key = object_key.string;

                JSONToken token = json_get_token(parser);
                ASSERT(token.kind == TOKEN_COLON);

                // after the colon we have the object value
                value = json_get_token(parser);
            } else if (object_key.kind != closing_token) {
                ASSERT(0);
            }
        }

        JSONElement *element = json_parse_element(memory, parser, key, value);
        if (element) {
            if (last_element) {
                last_element->next_sibling = element;
                last_element = element;
            } else {
                first_element = element;
                last_element = element;
            }
        } else if (value.kind == closing_token) {
            break;
        } else {
            ASSERT(0);
        }

        JSONToken token = json_get_token(parser);
        if (token.kind == closing_token) {
            break;
        } else if (token.kind != TOKEN_COMMA) {
            ASSERT(0);
        }
    }

    return first_element;
}

JSONElement *json_parse_element(Memory *memory, JSONParser *parser, String object_key, JSONToken token) {
    boolean valid = true;

    JSONElement *first_child = NULL;
    if (token.kind == TOKEN_OPEN_BRACKET) {
        first_child = json_parse_list(memory, parser, TOKEN_CLOSE_BRACKET, false);
    } else if (token.kind == TOKEN_OPEN_BRACE) {
        first_child = json_parse_list(memory, parser, TOKEN_CLOSE_BRACE, true);
    } else if ((token.kind == TOKEN_STRING_LITERAL) || (token.kind == TOKEN_TRUE) || (token.kind == TOKEN_FALSE) || (token.kind == TOKEN_NULL) || (token.kind == TOKEN_NUMBER)) {
        // Nothing to do here, since there is no additional data
    } else {
        valid = false;
    }

    JSONElement *element = {0};

    if (valid) {
        element = (JSONElement *)memory_alloc(memory, sizeof(JSONElement));
        element->key = object_key;
        element->value = token.string;
        element->first_child = first_child;
        element->next_sibling = NULL; // is this correct?
    }

    return element;
}

JSONElement *json_parse(Memory *memory, String input_json) {
    JSONParser parser = {0};
    parser.content = input_json;
    parser.line_number = 1;

    String object_key = {0};
    JSONToken token = json_get_token(&parser);
    JSONElement *result = json_parse_element(memory, &parser, object_key, token);

    return result;
}
